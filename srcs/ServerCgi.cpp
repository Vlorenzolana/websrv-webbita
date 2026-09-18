#include "../includes/Server.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

// Starts a CGI child and registers its stdin/stdout pipes with epoll.
bool Server::_startCgi(int clientFd, const Request& request,
    const ServerConfig& server, const LocationConfig* location,
    const std::string& fullPath)
{
    std::string interpreter;
    if (!_findCgiInterpreter(location, fullPath, interpreter) ||
        access(fullPath.c_str(), R_OK) != 0)
        return false;

    const std::size_t slash = interpreter.find_last_of('/');
    const std::string name = interpreter.substr(slash == std::string::npos
        ? 0 : slash + 1);
    CGIHandler handler(fullPath, interpreter,
        name == "cgi_tester" || name == "cgi_test");
    CgiProcess process;
    if (!handler.execute(request, location->upload_path, server.server_name,
            server.port, process))
        return false;

    CgiState state;
    state.clientFd = clientFd;
    state.childPid = process.pid;
    state.stdinFd = process.stdinFd;
    state.stdoutFd = process.stdoutFd;
    state.input = request.getBody();
    state.startTime = std::time(NULL);
    _cgiByPid[process.pid] = state;
    _cgiPipeRefs[process.stdoutFd] = CgiPipeRef(process.pid, false);

    struct epoll_event outputEvent;
    std::memset(&outputEvent, 0, sizeof(outputEvent));
    // EPOLLIN watches CGI stdout; EPOLLRDHUP detects a closed output pipe.
    outputEvent.events = EPOLLIN | EPOLLRDHUP;
    outputEvent.data.fd = process.stdoutFd;
    // Add the CGI output pipe to the epoll watch list.
    if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, process.stdoutFd, &outputEvent) < 0)
    {
        _cgiByPid[process.pid].clientFd = -1;
        _terminateCgi(process.pid, false);
        return false;
    }
    if (state.input.empty())
    {
        close(process.stdinFd);
        _cgiByPid[process.pid].stdinFd = -1;
        _cgiByPid[process.pid].stdinClosed = true;
    }
    else
    {
        _cgiPipeRefs[process.stdinFd] = CgiPipeRef(process.pid, true);
        struct epoll_event inputEvent;
        std::memset(&inputEvent, 0, sizeof(inputEvent));
        // EPOLLOUT means CGI stdin can accept more request-body bytes.
        inputEvent.events = EPOLLOUT | EPOLLRDHUP;
        inputEvent.data.fd = process.stdinFd;
        // Add the CGI input pipe to the epoll watch list.
        if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, process.stdinFd, &inputEvent) < 0)
        {
            _cgiByPid[process.pid].clientFd = -1;
            _terminateCgi(process.pid, false);
            return false;
        }
    }

    return true;
}

// Unregisters and closes one CGI pipe, updating its state flags.
void Server::_closeCgiPipe(int fd)
{
    const std::map<int, CgiPipeRef>::iterator reference = _cgiPipeRefs.find(fd);
    if (reference == _cgiPipeRefs.end())
    {
        for (std::map<pid_t, CgiState>::iterator it = _cgiByPid.begin();
             it != _cgiByPid.end(); ++it)
        {
            if (it->second.stdinFd == fd)
            {
                close(fd);
                it->second.stdinFd = -1;
                it->second.stdinClosed = true;
                return;
            }
            if (it->second.stdoutFd == fd)
            {
                close(fd);
                it->second.stdoutFd = -1;
                it->second.stdoutClosed = true;
                return;
            }
        }
        return;
    }

    const pid_t pid = reference->second.childPid;
    const bool isInput = reference->second.isInput;
    epoll_ctl(_epollFd, EPOLL_CTL_DEL, fd, NULL);
    close(fd);
    _cgiPipeRefs.erase(reference);

    std::map<pid_t, CgiState>::iterator cgi = _cgiByPid.find(pid);
    if (cgi == _cgiByPid.end())
        return;
    if (isInput)
    {
        cgi->second.stdinFd = -1;
        cgi->second.stdinClosed = true;
    }
    else
    {
        cgi->second.stdoutFd = -1;
        cgi->second.stdoutClosed = true;
    }
}

// Collects CGI children that have exited without blocking the server.
void Server::_reapCgiProcesses()
{
    std::vector<pid_t> pids;
    for (std::map<pid_t, CgiState>::const_iterator it = _cgiByPid.begin();
         it != _cgiByPid.end(); ++it)
        pids.push_back(it->first);

    for (std::size_t i = 0; i < pids.size(); ++i)
    {
        std::map<pid_t, CgiState>::iterator cgi = _cgiByPid.find(pids[i]);
        if (cgi == _cgiByPid.end() || cgi->second.childExited)
            continue;

        int status = 0;
        const pid_t result = waitpid(pids[i], &status, WNOHANG);
        if (result == pids[i])
        {
            cgi->second.childExited = true;
            cgi->second.exitStatus = status;
            _tryFinalizeCgi(pids[i]);
        }
        else if (result < 0 && errno == ECHILD)
        {
            cgi->second.childExited = true;
            cgi->second.exitStatus = 0;
            _tryFinalizeCgi(pids[i]);
        }
        else if (result < 0 && errno == EINTR)
            continue;
    }
}

// Closes idle clients and terminates CGI processes that exceed their deadlines.
void Server::_checkTimeouts()
{
    const std::time_t now = std::time(NULL);
    std::vector<int> timedOutClients;
    for (std::map<int, ClientState>::const_iterator it = _clients.begin();
         it != _clients.end(); ++it)
    {
        if (!it->second.processing && now - it->second.lastActivity > CLIENT_TIMEOUT_SECONDS)
            timedOutClients.push_back(it->first);
    }
    for (std::size_t i = 0; i < timedOutClients.size(); ++i)
    {
        std::map<int, ClientState>::iterator client = _clients.find(timedOutClients[i]);
        if (client == _clients.end())
            continue;
        const ServerConfig* server = _selectDefaultServer(
            client->second.listenPort, client->second.listenHost);
        client->second.processing = true;
        _sendErrorResponse(timedOutClients[i], 408, server, NULL);
    }

    std::vector<pid_t> timedOutCgi;
    for (std::map<pid_t, CgiState>::const_iterator it = _cgiByPid.begin();
         it != _cgiByPid.end(); ++it)
    {
        if (!it->second.childExited && now - it->second.startTime > CGI_TIMEOUT_SECONDS)
            timedOutCgi.push_back(it->first);
    }
    for (std::size_t i = 0; i < timedOutCgi.size(); ++i)
        _terminateCgi(timedOutCgi[i], true);
}

// Builds the final HTTP response once CGI output and process status are ready.
void Server::_tryFinalizeCgi(pid_t childPid)
{
    std::map<pid_t, CgiState>::iterator cgi = _cgiByPid.find(childPid);
    if (cgi == _cgiByPid.end() || !cgi->second.childExited ||
        !cgi->second.stdoutClosed)
        return;

    const int clientFd = cgi->second.clientFd;
    const bool timedOut = cgi->second.timedOut;
    const bool outputTooLarge = cgi->second.outputTooLarge;
    const int status = cgi->second.exitStatus;
    const std::string output = cgi->second.output;

    if (!cgi->second.stdinClosed && cgi->second.stdinFd >= 0)
        _closeCgiPipe(cgi->second.stdinFd);
    _eraseCgiState(childPid);

    if (clientFd < 0 || _clients.find(clientFd) == _clients.end())
        return;
    if (timedOut)
    {
        const ClientState& client = _clients[clientFd];
        const ServerConfig* server = _selectServerConfig(client.listenPort,
            client.listenHost, client.request);
        const LocationConfig* location = server == NULL ? NULL :
            _matchLocation(*server, client.request.getPath());
        _sendErrorResponse(clientFd, 504, server, location);
        return;
    }
    if (outputTooLarge)
    {
        const ClientState& client = _clients[clientFd];
        const ServerConfig* server = _selectServerConfig(client.listenPort,
            client.listenHost, client.request);
        const LocationConfig* location = server == NULL ? NULL :
            _matchLocation(*server, client.request.getPath());
        _sendErrorResponse(clientFd, 502, server, location);
        return;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
        const ClientState& client = _clients[clientFd];
        const ServerConfig* server = _selectServerConfig(client.listenPort,
            client.listenHost, client.request);
        const LocationConfig* location = server == NULL ? NULL :
            _matchLocation(*server, client.request.getPath());
        _sendErrorResponse(clientFd, 500, server, location);
        return;
    }
    _queueResponse(clientFd, _buildCgiHttpResponse(output));
}

// Stops a CGI process and closes both communication pipes.
void Server::_terminateCgi(pid_t childPid, bool timedOut)
{
    std::map<pid_t, CgiState>::iterator cgi = _cgiByPid.find(childPid);
    if (cgi == _cgiByPid.end())
        return;

    cgi->second.timedOut = cgi->second.timedOut || timedOut;
    if (cgi->second.stdinFd >= 0)
        _closeCgiPipe(cgi->second.stdinFd);
    cgi = _cgiByPid.find(childPid);
    if (cgi != _cgiByPid.end() && cgi->second.stdoutFd >= 0)
        _closeCgiPipe(cgi->second.stdoutFd);
    cgi = _cgiByPid.find(childPid);
    if (cgi != _cgiByPid.end() && !cgi->second.childExited)
    {
        if (kill(-childPid, SIGKILL) < 0)
            kill(childPid, SIGKILL);
    }
}

// Deletes all bookkeeping for a completed CGI process.
void Server::_eraseCgiState(pid_t childPid)
{
    _cgiByPid.erase(childPid);
}

