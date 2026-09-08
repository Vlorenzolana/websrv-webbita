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
#include <netdb.h>
#include <netinet/in.h>
#include <sstream>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

Server::Server(const std::vector<ServerConfig>& servers)
    : _servers(servers), _epollFd(-1)
{
}

Server::~Server()
{
    for (std::map<pid_t, CgiState>::iterator it = _cgiByPid.begin();
         it != _cgiByPid.end(); ++it)
    {
        if (it->second.stdinFd >= 0)
            close(it->second.stdinFd);
        if (it->second.stdoutFd >= 0)
            close(it->second.stdoutFd);
        if (!it->second.childExited && it->second.childPid > 0)
        {
            if (kill(-it->second.childPid, SIGKILL) < 0)
                kill(it->second.childPid, SIGKILL);
            waitpid(it->second.childPid, NULL, WNOHANG);
        }
    }

    for (std::map<int, ClientState>::iterator it = _clients.begin();
         it != _clients.end(); ++it)
        close(it->first);
    for (std::size_t i = 0; i < _listenFds.size(); ++i)
        close(_listenFds[i]);
    if (_epollFd >= 0)
        close(_epollFd);
}

bool Server::_setNonBlocking(int fd)
{
    return fcntl(fd, F_SETFL, O_NONBLOCK) >= 0;
}

std::string Server::_trim(const std::string& value)
{
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return "";
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string Server::_toLower(const std::string& value)
{
    std::string result(value);
    for (std::size_t i = 0; i < result.size(); ++i)
    {
        if (result[i] >= 'A' && result[i] <= 'Z')
            result[i] = static_cast<char>(result[i] - 'A' + 'a');
    }
    return result;
}

std::string Server::_intToString(long value)
{
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

void Server::init()
{
    _epollFd = epoll_create(128);
    if (_epollFd < 0)
        throw std::runtime_error("Could not create epoll instance");

    for (std::size_t i = 0; i < _servers.size(); ++i)
    {
        bool alreadyOpen = false;
        for (std::map<int, ListenerState>::const_iterator it = _listeners.begin();
             it != _listeners.end(); ++it)
        {
            if (it->second.host == _servers[i].host &&
                it->second.port == _servers[i].port)
            {
                alreadyOpen = true;
                break;
            }
        }
        if (!alreadyOpen)
            _openListener(_servers[i]);
    }
}

void Server::run()
{
    struct epoll_event events[128];
    while (true)
    {
        const int count = epoll_wait(_epollFd, events, 128, 1000);
        if (count < 0)
        {
            if (errno == EINTR)
                continue;
            throw std::runtime_error("Could not wait for events");
        }

        for (int i = 0; i < count; ++i)
        {
            const int fd = events[i].data.fd;
            if (_listeners.find(fd) != _listeners.end())
                _acceptClients(fd);
            else if (_cgiPipeRefs.find(fd) != _cgiPipeRefs.end())
                _handleCgiEvent(fd, events[i].events);
            else if (_clients.find(fd) != _clients.end())
                _handleClientEvent(fd, events[i].events);
            else
            {
                epoll_ctl(_epollFd, EPOLL_CTL_DEL, fd, NULL);
                close(fd);
            }
        }

        _reapCgiProcesses();
        _checkTimeouts();
    }
}

void Server::_openListener(const ServerConfig& server)
{
    struct addrinfo hints;
    struct addrinfo* addresses = NULL;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    const std::string port = _intToString(server.port);
    if (getaddrinfo(server.host.c_str(), port.c_str(), &hints, &addresses) != 0)
        throw std::runtime_error("Invalid listen host: " + server.host);

    int listenerFd = -1;
    for (struct addrinfo* address = addresses; address != NULL;
         address = address->ai_next)
    {
        listenerFd = socket(address->ai_family, address->ai_socktype,
            address->ai_protocol);
        if (listenerFd < 0)
            continue;

        int reuseAddress = 1;
        if (setsockopt(listenerFd, SOL_SOCKET, SO_REUSEADDR,
                &reuseAddress, sizeof(reuseAddress)) < 0 ||
            !_setNonBlocking(listenerFd) ||
            bind(listenerFd, address->ai_addr, address->ai_addrlen) < 0)
        {
            close(listenerFd);
            listenerFd = -1;
            continue;
        }
        break;
    }
    freeaddrinfo(addresses);

    if (listenerFd < 0)
        throw std::runtime_error("Could not bind listener on " + server.host +
            ":" + port);

    if (listen(listenerFd, SOMAXCONN) < 0)
    {
        close(listenerFd);
        throw std::runtime_error("Could not start listener on " + server.host +
            ":" + port);
    }

    struct epoll_event event;
    std::memset(&event, 0, sizeof(event));
    event.events = EPOLLIN;
    event.data.fd = listenerFd;
    if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, listenerFd, &event) < 0)
    {
        close(listenerFd);
        throw std::runtime_error("Could not register listener in epoll");
    }

    ListenerState listener;
    listener.host = server.host;
    listener.port = server.port;
    _listeners[listenerFd] = listener;
    _listenFds.push_back(listenerFd);
    std::cout << "Listening on " << server.host << ":" << server.port << std::endl;
}

void Server::_acceptClients(int listenerFd)
{
    const std::map<int, ListenerState>::const_iterator listener =
        _listeners.find(listenerFd);
    if (listener == _listeners.end())
        return;

    while (true)
    {
        const int clientFd = accept(listenerFd, NULL, NULL);
        if (clientFd < 0)
        {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return;
            return;
        }

        if (!_setNonBlocking(clientFd))
        {
            close(clientFd);
            continue;
        }

        struct epoll_event event;
        std::memset(&event, 0, sizeof(event));
        event.events = EPOLLIN | EPOLLRDHUP;
        event.data.fd = clientFd;
        if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, clientFd, &event) < 0)
        {
            close(clientFd);
            continue;
        }

        ClientState state;
        state.listenPort = listener->second.port;
        state.listenHost = listener->second.host;
        state.lastActivity = std::time(NULL);

        bool unlimited = false;
        std::size_t maximumBodySize = 0;
        for (std::size_t i = 0; i < _servers.size(); ++i)
        {
            if (_servers[i].port != state.listenPort ||
                _servers[i].host != state.listenHost)
                continue;
            if (_servers[i].client_max_body_size == 0)
            {
                unlimited = true;
                break;
            }
            const std::size_t limit = static_cast<std::size_t>(
                _servers[i].client_max_body_size);
            if (limit > maximumBodySize)
                maximumBodySize = limit;
        }
        if (!unlimited && maximumBodySize > 0)
            state.request.setMaxBodySize(maximumBodySize);
        _clients[clientFd] = state;
    }
}

void Server::_handleClientEvent(int clientFd, unsigned int events)
{
    if (_clients.find(clientFd) == _clients.end())
        return;

    if ((events & EPOLLIN) && !_clients[clientFd].processing)
        _handleClientReadable(clientFd);

    if (_clients.find(clientFd) == _clients.end())
        return;

    if ((events & EPOLLOUT) &&
        _pendingResponses.find(clientFd) != _pendingResponses.end())
        _handleClientWritable(clientFd);

    if (_clients.find(clientFd) == _clients.end())
        return;

    if (events & (EPOLLERR | EPOLLHUP))
        _closeConnection(clientFd);
    else if ((events & EPOLLRDHUP) &&
             _clients.find(clientFd) != _clients.end() &&
             !_clients[clientFd].processing)
        _closeConnection(clientFd);
}

void Server::_handleClientReadable(int clientFd)
{
    std::map<int, ClientState>::iterator client = _clients.find(clientFd);
    if (client == _clients.end())
        return;

    char buffer[8192];
    while (true)
    {
        const ssize_t received = recv(clientFd, buffer, sizeof(buffer), 0);
        if (received > 0)
        {
            client->second.lastActivity = std::time(NULL);
            const bool complete = client->second.request.parse(
                std::string(buffer, static_cast<std::size_t>(received)));
            _updateClientBodyLimit(clientFd);
            client = _clients.find(clientFd);
            if (client == _clients.end())
                return;

            if (complete || client->second.request.isParsed())
            {
                client->second.processing = true;
                const Request request = client->second.request;
                _processRequest(clientFd, request, client->second.listenPort,
                    client->second.listenHost);
                return;
            }
            continue;
        }
        if (received == 0)
        {
            _closeConnection(clientFd);
            return;
        }
        if (errno == EINTR)
            continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return;
        _closeConnection(clientFd);
        return;
    }
}

void Server::_handleClientWritable(int clientFd)
{
    if (_flushResponse(clientFd))
        _closeConnection(clientFd);
}

void Server::_handleCgiEvent(int pipeFd, unsigned int events)
{
    const std::map<int, CgiPipeRef>::const_iterator reference =
        _cgiPipeRefs.find(pipeFd);
    if (reference == _cgiPipeRefs.end())
        return;

    const bool isInput = reference->second.isInput;
    if (isInput && (events & EPOLLOUT))
        _handleCgiWritable(pipeFd);
    else if (!isInput && (events & EPOLLIN))
        _handleCgiReadable(pipeFd);

    if (_cgiPipeRefs.find(pipeFd) == _cgiPipeRefs.end())
        return;

    if (events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
    {
        if (!isInput)
            _handleCgiReadable(pipeFd);
        if (_cgiPipeRefs.find(pipeFd) != _cgiPipeRefs.end())
            _closeCgiPipe(pipeFd);
    }
}

void Server::_handleCgiWritable(int pipeFd)
{
    const std::map<int, CgiPipeRef>::const_iterator reference =
        _cgiPipeRefs.find(pipeFd);
    if (reference == _cgiPipeRefs.end())
        return;
    std::map<pid_t, CgiState>::iterator cgi =
        _cgiByPid.find(reference->second.childPid);
    if (cgi == _cgiByPid.end())
    {
        _closeCgiPipe(pipeFd);
        return;
    }

    while (cgi->second.inputOffset < cgi->second.input.size())
    {
        const ssize_t written = write(pipeFd,
            cgi->second.input.data() + cgi->second.inputOffset,
            cgi->second.input.size() - cgi->second.inputOffset);
        if (written > 0)
        {
            cgi->second.inputOffset += static_cast<std::size_t>(written);
            continue;
        }
        if (written < 0 && errno == EINTR)
            continue;
        if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return;
        _closeCgiPipe(pipeFd);
        _tryFinalizeCgi(cgi->first);
        return;
    }

    const pid_t pid = cgi->first;
    _closeCgiPipe(pipeFd);
    _tryFinalizeCgi(pid);
}

void Server::_handleCgiReadable(int pipeFd)
{
    const std::map<int, CgiPipeRef>::const_iterator reference =
        _cgiPipeRefs.find(pipeFd);
    if (reference == _cgiPipeRefs.end())
        return;
    const pid_t pid = reference->second.childPid;
    std::map<pid_t, CgiState>::iterator cgi = _cgiByPid.find(pid);
    if (cgi == _cgiByPid.end())
    {
        _closeCgiPipe(pipeFd);
        return;
    }

    char buffer[8192];
    while (true)
    {
        const ssize_t count = read(pipeFd, buffer, sizeof(buffer));
        if (count > 0)
        {
            const std::size_t received = static_cast<std::size_t>(count);
            if (received > CGI_MAX_OUTPUT_SIZE ||
                cgi->second.output.size() > CGI_MAX_OUTPUT_SIZE - received)
            {
                cgi->second.outputTooLarge = true;
                _terminateCgi(pid, false);
                return;
            }
            cgi->second.output.append(buffer, received);
            continue;
        }
        if (count == 0)
        {
            _closeCgiPipe(pipeFd);
            _tryFinalizeCgi(pid);
            return;
        }
        if (errno == EINTR)
            continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return;
        _closeCgiPipe(pipeFd);
        _tryFinalizeCgi(pid);
        return;
    }
}

void Server::_closeConnection(int fd)
{
    if (_clients.find(fd) == _clients.end() &&
        _pendingResponses.find(fd) == _pendingResponses.end())
        return;

    epoll_ctl(_epollFd, EPOLL_CTL_DEL, fd, NULL);
    close(fd);
    _pendingResponses.erase(fd);
    _clients.erase(fd);

    std::vector<pid_t> related;
    for (std::map<pid_t, CgiState>::const_iterator it = _cgiByPid.begin();
         it != _cgiByPid.end(); ++it)
    {
        if (it->second.clientFd == fd)
            related.push_back(it->first);
    }
    for (std::size_t i = 0; i < related.size(); ++i)
    {
        std::map<pid_t, CgiState>::iterator cgi = _cgiByPid.find(related[i]);
        if (cgi != _cgiByPid.end())
            cgi->second.clientFd = -1;
        _terminateCgi(related[i], false);
    }
}

void Server::_queueResponse(int clientFd, const std::string& response)
{
    if (_clients.find(clientFd) == _clients.end())
        return;

    PendingResponse pending;
    pending.data = response;
    _pendingResponses[clientFd] = pending;
    _clients[clientFd].processing = true;

    struct epoll_event event;
    std::memset(&event, 0, sizeof(event));
    event.events = EPOLLOUT | EPOLLRDHUP;
    event.data.fd = clientFd;
    if (epoll_ctl(_epollFd, EPOLL_CTL_MOD, clientFd, &event) < 0)
        _closeConnection(clientFd);
}

bool Server::_flushResponse(int clientFd)
{
    std::map<int, PendingResponse>::iterator pending =
        _pendingResponses.find(clientFd);
    if (pending == _pendingResponses.end())
        return true;

    while (pending->second.offset < pending->second.data.size())
    {
        const ssize_t sent = send(clientFd,
            pending->second.data.data() + pending->second.offset,
            pending->second.data.size() - pending->second.offset, 0);
        if (sent > 0)
        {
            pending->second.offset += static_cast<std::size_t>(sent);
            if (_clients.find(clientFd) != _clients.end())
                _clients[clientFd].lastActivity = std::time(NULL);
            continue;
        }
        if (sent < 0 && errno == EINTR)
            continue;
        if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return false;
        return true;
    }

    _pendingResponses.erase(pending);
    return true;
}

void Server::_updateClientBodyLimit(int clientFd)
{
    std::map<int, ClientState>::iterator client = _clients.find(clientFd);
    if (client == _clients.end() || !client->second.request.headersComplete())
        return;

    const ServerConfig* server = _selectServerConfig(client->second.listenPort,
        client->second.listenHost, client->second.request);
    if (server != NULL && server->client_max_body_size > 0)
        client->second.request.setMaxBodySize(
            static_cast<std::size_t>(server->client_max_body_size));
}

void Server::_processRequest(int clientFd, const Request& request,
    int listenPort, const std::string& listenHost)
{
    const ServerConfig* server =
        _selectServerConfig(listenPort, listenHost, request);
    if (request.getErrorCode() != 0)
    {
        _sendErrorResponse(clientFd, request.getErrorCode(), server, NULL);
        return;
    }
    if (server == NULL)
    {
        _sendErrorResponse(clientFd, 404, NULL, NULL);
        return;
    }

    const LocationConfig* location = _matchLocation(*server, request.getPath());
    if (location == NULL)
    {
        _sendErrorResponse(clientFd, 404, server, NULL);
        return;
    }
    if (!_isMethodAllowed(location, request.getMethod()))
    {
        _sendErrorResponse(clientFd, 405, server, location);
        return;
    }
    if (_hasPathTraversal(request.getPath()))
    {
        _sendErrorResponse(clientFd, 403, server, location);
        return;
    }
    if (server->client_max_body_size > 0 &&
        request.getBody().size() >
            static_cast<std::size_t>(server->client_max_body_size))
    {
        _sendErrorResponse(clientFd, 413, server, location);
        return;
    }

    if (location->return_code != 0)
    {
        std::map<std::string, std::string> headers;
        headers["Location"] = location->return_url;
        _queueResponse(clientFd, _buildResponse(location->return_code,
            _statusText(location->return_code), "text/plain", "", headers));
        return;
    }

    const std::string fullPath = _resolvePath(*server, location, request.getPath());
    struct stat fileStat;
    std::string interpreter;
    if (stat(fullPath.c_str(), &fileStat) == 0 && S_ISREG(fileStat.st_mode) &&
        _findCgiInterpreter(location, fullPath, interpreter))
    {
        if (!_startCgi(clientFd, request, *server, location, fullPath))
            _sendErrorResponse(clientFd, 500, server, location);
        return;
    }

    std::string response;
    if (request.getMethod() == "GET")
        response = _handleGet(*server, request, location);
    else if (request.getMethod() == "POST")
        response = _handlePost(*server, request, location);
    else
        response = _handleDelete(*server, request, location);
    _queueResponse(clientFd, response);
}

bool Server::_startCgi(int clientFd, const Request& request,
    const ServerConfig& server, const LocationConfig* location,
    const std::string& fullPath)
{
    std::string interpreter;
    if (!_findCgiInterpreter(location, fullPath, interpreter) ||
        access(fullPath.c_str(), R_OK) != 0)
        return false;

    CGIHandler handler(fullPath, interpreter);
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
    outputEvent.events = EPOLLIN | EPOLLRDHUP;
    outputEvent.data.fd = process.stdoutFd;
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
        inputEvent.events = EPOLLOUT | EPOLLRDHUP;
        inputEvent.data.fd = process.stdinFd;
        if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, process.stdinFd, &inputEvent) < 0)
        {
            _cgiByPid[process.pid].clientFd = -1;
            _terminateCgi(process.pid, false);
            return false;
        }
    }

    return true;
}

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
    }
}

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

void Server::_eraseCgiState(pid_t childPid)
{
    _cgiByPid.erase(childPid);
}

const ServerConfig* Server::_selectServerConfig(int listenPort,
    const std::string& listenHost, const Request& request) const
{
    std::string host = request.getHeaderValue("host");
    const std::size_t colon = host.find(':');
    if (colon != std::string::npos)
        host.erase(colon);
    host = _toLower(_trim(host));

    const ServerConfig* fallback = NULL;
    for (std::size_t i = 0; i < _servers.size(); ++i)
    {
        if (_servers[i].port != listenPort || _servers[i].host != listenHost)
            continue;
        if (fallback == NULL)
            fallback = &_servers[i];
        if (!host.empty() && _toLower(_servers[i].server_name) == host)
            return &_servers[i];
    }
    return fallback;
}

const ServerConfig* Server::_selectDefaultServer(int listenPort,
    const std::string& listenHost) const
{
    for (std::size_t i = 0; i < _servers.size(); ++i)
    {
        if (_servers[i].port == listenPort && _servers[i].host == listenHost)
            return &_servers[i];
    }
    return NULL;
}

const LocationConfig* Server::_matchLocation(const ServerConfig& server,
    const std::string& path) const
{
    const LocationConfig* best = NULL;
    std::size_t bestLength = 0;
    for (std::size_t i = 0; i < server.locations.size(); ++i)
    {
        const std::string& prefix = server.locations[i].path;
        if (path.compare(0, prefix.size(), prefix) != 0)
            continue;
        const bool boundary = prefix == "/" ||
            (!prefix.empty() && prefix[prefix.size() - 1] == '/') ||
            path.size() == prefix.size() ||
            (path.size() > prefix.size() && path[prefix.size()] == '/');
        if (boundary && prefix.size() > bestLength)
        {
            best = &server.locations[i];
            bestLength = prefix.size();
        }
    }
    return best;
}

bool Server::_isMethodAllowed(const LocationConfig* location,
    const std::string& method) const
{
    if (location == NULL)
        return false;
    for (std::size_t i = 0; i < location->allowed_methods.size(); ++i)
    {
        if (location->allowed_methods[i] == method)
            return true;
    }
    return false;
}

std::string Server::_resolvePath(const ServerConfig& server,
    const LocationConfig* location, const std::string& requestPath) const
{
    const std::string root = location != NULL && !location->root_directory.empty()
        ? location->root_directory : server.root_directory;
    std::string relative = requestPath;
    if (location != NULL && relative.compare(0, location->path.size(),
            location->path) == 0)
        relative.erase(0, location->path.size());
    while (!relative.empty() && relative[0] == '/')
        relative.erase(0, 1);

    std::string result = root;
    if (!result.empty() && result[result.size() - 1] != '/')
        result += '/';
    result += relative;
    return result;
}

bool Server::_hasPathTraversal(const std::string& path) const
{
    std::string decoded;
    for (std::size_t i = 0; i < path.size(); ++i)
    {
        if (path[i] == '%' && i + 2 < path.size())
        {
            const std::string hex = path.substr(i + 1, 2);
            char* end = NULL;
            const long value = std::strtol(hex.c_str(), &end, 16);
            if (end != NULL && *end == '\0')
            {
                decoded += static_cast<char>(value);
                i += 2;
                continue;
            }
        }
        decoded += path[i];
    }
    if (decoded.find('\0') != std::string::npos)
        return true;

    std::stringstream stream(decoded);
    std::string segment;
    while (std::getline(stream, segment, '/'))
    {
        if (segment == "..")
            return true;
    }
    return false;
}

bool Server::_findCgiInterpreter(const LocationConfig* location,
    const std::string& fullPath, std::string& interpreter) const
{
    if (location == NULL)
        return false;
    const std::size_t slash = fullPath.find_last_of('/');
    const std::size_t dot = fullPath.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return false;
    const std::string extension = fullPath.substr(dot);
    const std::map<std::string, std::string>::const_iterator it =
        location->cgi_interpreters.find(extension);
    if (it == location->cgi_interpreters.end())
        return false;
    interpreter = it->second;
    return true;
}

std::string Server::_handleGet(const ServerConfig& server,
    const Request& request, const LocationConfig* location) const
{
    std::string fullPath = _resolvePath(server, location, request.getPath());
    struct stat fileStat;
    if (stat(fullPath.c_str(), &fileStat) != 0)
        return _buildErrorResponse(404, &server, location);

    if (S_ISDIR(fileStat.st_mode))
    {
        bool foundIndex = false;
        for (std::size_t i = 0; i < location->index_files.size(); ++i)
        {
            std::string indexPath = fullPath;
            if (!indexPath.empty() && indexPath[indexPath.size() - 1] != '/')
                indexPath += '/';
            indexPath += location->index_files[i];
            struct stat indexStat;
            if (stat(indexPath.c_str(), &indexStat) == 0 &&
                S_ISREG(indexStat.st_mode))
            {
                fullPath = indexPath;
                foundIndex = true;
                break;
            }
        }
        if (!foundIndex)
        {
            if (location->autoindex)
                return _buildResponse(200, _statusText(200), "text/html",
                    _buildAutoindexPage(fullPath, request.getPath()));
            return _buildErrorResponse(403, &server, location);
        }
    }
    else if (!S_ISREG(fileStat.st_mode))
        return _buildErrorResponse(403, &server, location);

    std::ifstream file(fullPath.c_str(), std::ios::binary);
    if (!file.is_open())
        return _buildErrorResponse(403, &server, location);
    std::ostringstream content;
    content << file.rdbuf();
    return _buildResponse(200, _statusText(200), _getMimeType(fullPath),
        content.str());
}

std::string Server::_handlePost(const ServerConfig& server,
    const Request& request, const LocationConfig* location) const
{
    if (location->upload_path.empty())
        return _buildErrorResponse(403, &server, location);

    const std::string contentType = _toLower(request.getHeaderValue("content-type"));
    std::vector<std::string> savedNames;
    bool saved = false;
    if (contentType.find("multipart/form-data") == 0)
        saved = _saveMultipartUpload(request, location, savedNames);
    else
    {
        std::string name;
        saved = _saveRawUpload(request, location, name);
        if (saved)
            savedNames.push_back(name);
    }

    if (!saved)
        return _buildErrorResponse(400, &server, location);

    std::ostringstream body;
    body << "Uploaded";
    for (std::size_t i = 0; i < savedNames.size(); ++i)
        body << (i == 0 ? ": " : ", ") << savedNames[i];
    body << "\n";

    std::map<std::string, std::string> headers;
    headers["Location"] = request.getPath();
    return _buildResponse(201, _statusText(201), "text/plain", body.str(), headers);
}

std::string Server::_handleDelete(const ServerConfig& server,
    const Request& request, const LocationConfig* location) const
{
    const std::string fullPath = _resolvePath(server, location, request.getPath());
    struct stat fileStat;
    if (stat(fullPath.c_str(), &fileStat) != 0)
        return _buildErrorResponse(404, &server, location);
    if (!S_ISREG(fileStat.st_mode))
        return _buildErrorResponse(403, &server, location);
    if (std::remove(fullPath.c_str()) != 0)
        return _buildErrorResponse(500, &server, location);
    return _buildResponse(204, _statusText(204), "text/plain", "");
}

bool Server::_saveRawUpload(const Request& request,
    const LocationConfig* location, std::string& savedName) const
{
    std::string relative = request.getPath();
    if (relative.compare(0, location->path.size(), location->path) == 0)
        relative.erase(0, location->path.size());
    const std::size_t slash = relative.find_last_of('/');
    savedName = _safeFileName(slash == std::string::npos
        ? relative : relative.substr(slash + 1));
    if (savedName.empty())
        savedName = "upload_" + _intToString(static_cast<long>(std::time(NULL))) + ".bin";

    std::string path = location->upload_path;
    if (!path.empty() && path[path.size() - 1] != '/')
        path += '/';
    path += savedName;

    std::ofstream file(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!file.is_open())
        return false;
    file.write(request.getBody().data(),
        static_cast<std::streamsize>(request.getBody().size()));
    return file.good();
}

bool Server::_saveMultipartUpload(const Request& request,
    const LocationConfig* location, std::vector<std::string>& savedNames) const
{
    const std::string contentType = request.getHeaderValue("content-type");
    const std::string boundaryValue =
        _extractMultipartParameter(contentType, "boundary");
    if (boundaryValue.empty())
        return false;

    const std::string boundary = "--" + boundaryValue;
    const std::string& body = request.getBody();
    std::size_t position = 0;
    bool wroteFile = false;

    while (true)
    {
        std::size_t partStart = body.find(boundary, position);
        if (partStart == std::string::npos)
            break;
        partStart += boundary.size();
        if (body.compare(partStart, 2, "--") == 0)
            break;
        if (body.compare(partStart, 2, "\r\n") == 0)
            partStart += 2;
        else if (body.compare(partStart, 1, "\n") == 0)
            partStart += 1;

        std::size_t headerEnd = body.find("\r\n\r\n", partStart);
        std::size_t separatorSize = 4;
        if (headerEnd == std::string::npos)
        {
            headerEnd = body.find("\n\n", partStart);
            separatorSize = 2;
        }
        if (headerEnd == std::string::npos)
            return false;

        const std::string headers = body.substr(partStart, headerEnd - partStart);
        std::string disposition;
        std::istringstream headerStream(headers);
        std::string line;
        while (std::getline(headerStream, line))
        {
            if (!line.empty() && line[line.size() - 1] == '\r')
                line.erase(line.size() - 1);
            const std::size_t colon = line.find(':');
            if (colon != std::string::npos &&
                _toLower(_trim(line.substr(0, colon))) == "content-disposition")
                disposition = _trim(line.substr(colon + 1));
        }

        const std::string rawFileName =
            _extractMultipartParameter(disposition, "filename");
        const std::size_t dataStart = headerEnd + separatorSize;
        std::size_t nextBoundary = body.find("\r\n" + boundary, dataStart);
        std::size_t prefixSize = 2;
        if (nextBoundary == std::string::npos)
        {
            nextBoundary = body.find("\n" + boundary, dataStart);
            prefixSize = 1;
        }
        if (nextBoundary == std::string::npos)
            return false;

        if (!rawFileName.empty())
        {
            const std::string safeName = _safeFileName(rawFileName);
            if (safeName.empty())
                return false;
            std::string path = location->upload_path;
            if (!path.empty() && path[path.size() - 1] != '/')
                path += '/';
            path += safeName;

            std::ofstream file(path.c_str(), std::ios::binary | std::ios::trunc);
            if (!file.is_open())
                return false;
            file.write(body.data() + dataStart,
                static_cast<std::streamsize>(nextBoundary - dataStart));
            if (!file.good())
                return false;
            savedNames.push_back(safeName);
            wroteFile = true;
        }
        position = nextBoundary + prefixSize;
    }
    return wroteFile;
}

std::string Server::_safeFileName(const std::string& value)
{
    std::string name = value;
    const std::size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos)
        name.erase(0, slash + 1);
    if (name.empty() || name == "." || name == "..")
        return "";
    for (std::size_t i = 0; i < name.size(); ++i)
    {
        const unsigned char c = static_cast<unsigned char>(name[i]);
        if (c < 32 || name[i] == '/' || name[i] == '\\')
            name[i] = '_';
    }
    return name;
}

std::string Server::_extractMultipartParameter(const std::string& header,
    const std::string& parameter)
{
    const std::string lower = _toLower(header);
    const std::string needle = _toLower(parameter) + "=";
    std::size_t position = lower.find(needle);
    if (position == std::string::npos)
        return "";
    position += needle.size();
    if (position >= header.size())
        return "";
    if (header[position] == '"')
    {
        const std::size_t end = header.find('"', position + 1);
        if (end == std::string::npos)
            return "";
        return header.substr(position + 1, end - position - 1);
    }
    const std::size_t end = header.find(';', position);
    return _trim(header.substr(position,
        end == std::string::npos ? std::string::npos : end - position));
}

void Server::_sendErrorResponse(int clientFd, int code,
    const ServerConfig* server, const LocationConfig* location)
{
    _queueResponse(clientFd, _buildErrorResponse(code, server, location));
}

std::string Server::_buildErrorResponse(int code, const ServerConfig* server,
    const LocationConfig* location) const
{
    return _buildResponse(code, _statusText(code), "text/html",
        _loadErrorBody(code, server, location));
}

std::string Server::_loadErrorBody(int code, const ServerConfig* server,
    const LocationConfig* location) const
{
    std::string configuredPath;
    std::string root;
    if (location != NULL)
    {
        const std::map<int, std::string>::const_iterator it =
            location->error_pages.find(code);
        if (it != location->error_pages.end())
        {
            configuredPath = it->second;
            root = location->root_directory;
        }
    }
    if (configuredPath.empty() && server != NULL)
    {
        const std::map<int, std::string>::const_iterator it =
            server->error_pages.find(code);
        if (it != server->error_pages.end())
        {
            configuredPath = it->second;
            root = server->root_directory;
        }
    }

    if (!configuredPath.empty())
    {
        if (configuredPath[0] == '/')
            configuredPath = root + configuredPath;
        std::ifstream file(configuredPath.c_str(), std::ios::binary);
        if (file.is_open())
        {
            std::ostringstream content;
            content << file.rdbuf();
            return content.str();
        }
    }
    return _defaultErrorBody(code, _statusText(code));
}

std::string Server::_buildCgiHttpResponse(const std::string& rawOutput) const
{
    int statusCode = 200;
    std::string contentType = "text/plain";
    std::map<std::string, std::string> extraHeaders;
    bool hasLocation = false;
    std::string body = rawOutput;

    std::size_t headerEnd = rawOutput.find("\r\n\r\n");
    std::size_t separatorSize = 4;
    if (headerEnd == std::string::npos)
    {
        headerEnd = rawOutput.find("\n\n");
        separatorSize = 2;
    }

    if (headerEnd != std::string::npos)
    {
        const std::string headerBlock = rawOutput.substr(0, headerEnd);
        body = rawOutput.substr(headerEnd + separatorSize);
        std::istringstream stream(headerBlock);
        std::string line;
        while (std::getline(stream, line))
        {
            if (!line.empty() && line[line.size() - 1] == '\r')
                line.erase(line.size() - 1);
            const std::size_t colon = line.find(':');
            if (colon == std::string::npos)
                continue;
            const std::string name = _trim(line.substr(0, colon));
            const std::string lowerName = _toLower(name);
            const std::string value = _trim(line.substr(colon + 1));
            if (lowerName == "status")
            {
                std::istringstream status(value);
                status >> statusCode;
            }
            else if (lowerName == "content-type")
                contentType = value;
            else if (lowerName != "content-length" && lowerName != "connection")
            {
                extraHeaders[name] = value;
                if (lowerName == "location")
                    hasLocation = true;
            }
        }
        if (statusCode == 200 && hasLocation)
            statusCode = 302;
    }

    return _buildResponse(statusCode, _statusText(statusCode), contentType,
        body, extraHeaders);
}

std::string Server::_buildResponse(int code, const std::string& statusText,
    const std::string& contentType, const std::string& body,
    const std::map<std::string, std::string>& extraHeaders) const
{
    std::ostringstream response;
    response << "HTTP/1.1 " << code << " " << statusText << "\r\n";
    response << "Content-Type: " << contentType << "\r\n";
    response << "Content-Length: " << body.size() << "\r\n";
    response << "Connection: close\r\n";
    for (std::map<std::string, std::string>::const_iterator it =
             extraHeaders.begin(); it != extraHeaders.end(); ++it)
        response << it->first << ": " << it->second << "\r\n";
    response << "\r\n" << body;
    return response.str();
}

std::string Server::_defaultErrorBody(int code,
    const std::string& statusText) const
{
    std::ostringstream body;
    body << "<!doctype html><html><head><meta charset=\"utf-8\">"
         << "<title>" << code << " " << statusText << "</title></head>"
         << "<body><h1>" << code << " " << statusText
         << "</h1></body></html>";
    return body.str();
}

std::string Server::_statusText(int code) const
{
    switch (code)
    {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 303: return "See Other";
        case 307: return "Temporary Redirect";
        case 308: return "Permanent Redirect";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 408: return "Request Timeout";
        case 411: return "Length Required";
        case 413: return "Payload Too Large";
        case 414: return "URI Too Long";
        case 431: return "Request Header Fields Too Large";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 504: return "Gateway Timeout";
        case 505: return "HTTP Version Not Supported";
        default: return "Error";
    }
}

std::string Server::_getMimeType(const std::string& path) const
{
    const std::size_t dot = path.find_last_of('.');
    if (dot == std::string::npos)
        return "application/octet-stream";
    const std::string extension = _toLower(path.substr(dot + 1));
    if (extension == "html" || extension == "htm") return "text/html";
    if (extension == "css") return "text/css";
    if (extension == "js") return "application/javascript";
    if (extension == "json") return "application/json";
    if (extension == "png") return "image/png";
    if (extension == "jpg" || extension == "jpeg") return "image/jpeg";
    if (extension == "gif") return "image/gif";
    if (extension == "svg") return "image/svg+xml";
    if (extension == "ico") return "image/x-icon";
    if (extension == "txt") return "text/plain";
    if (extension == "pdf") return "application/pdf";
    return "application/octet-stream";
}

std::string Server::_buildAutoindexPage(const std::string& directoryPath,
    const std::string& requestPath) const
{
    std::ostringstream page;
    page << "<!doctype html><html><head><meta charset=\"utf-8\">"
         << "<title>Index of " << requestPath << "</title></head><body>"
         << "<h1>Index of " << requestPath << "</h1><ul>";

    DIR* directory = opendir(directoryPath.c_str());
    if (directory != NULL)
    {
        struct dirent* entry;
        while ((entry = readdir(directory)) != NULL)
        {
            const std::string name = entry->d_name;
            if (name == ".")
                continue;
            std::string link = requestPath;
            if (link.empty() || link[link.size() - 1] != '/')
                link += '/';
            link += name;
            page << "<li><a href=\"" << link << "\">" << name
                 << "</a></li>";
        }
        closedir(directory);
    }
    page << "</ul></body></html>";
    return page.str();
}
