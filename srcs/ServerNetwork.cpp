#include "../includes/Server.hpp"

#include <algorithm>
#include <netdb.h>
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

void Server::_openListener(const ServerConfig& server)
{
    const int listenerFd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenerFd < 0)
        throw std::runtime_error("Failed to create listening socket: " +
            std::string(std::strerror(errno)));

    int reuseAddress = 1;
    if (setsockopt(listenerFd, SOL_SOCKET, SO_REUSEADDR,
            &reuseAddress, sizeof(reuseAddress)) < 0 ||
        !_setNonBlocking(listenerFd) || !_setCloseOnExec(listenerFd))
    {
        const std::string reason = std::strerror(errno);
        close(listenerFd);
        throw std::runtime_error("Failed to configure listening socket: " + reason);
    }

    struct addrinfo hints = {};
    struct addrinfo* addressInfo = NULL;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    const std::string port = _intToString(server.port);
    const int addressStatus = getaddrinfo(
        server.host.c_str(), port.c_str(), &hints, &addressInfo);
    if (addressStatus != 0 || addressInfo == NULL)
    {
        if (addressInfo != NULL)
            freeaddrinfo(addressInfo);
        close(listenerFd);
        throw std::runtime_error("Invalid listen host " + server.host + ": " +
            gai_strerror(addressStatus));
    }

    if (bind(listenerFd, addressInfo->ai_addr, addressInfo->ai_addrlen) < 0)
    {
        const std::string reason = std::strerror(errno);
        freeaddrinfo(addressInfo);
        close(listenerFd);
        throw std::runtime_error("Failed to bind " + server.host + ":" +
            _intToString(server.port) + ": " + reason);
    }
    freeaddrinfo(addressInfo);

    if (listen(listenerFd, SOMAXCONN) < 0)
    {
        const std::string reason = std::strerror(errno);
        close(listenerFd);
        throw std::runtime_error("Failed to listen on " + server.host + ":" +
            _intToString(server.port) + ": " + reason);
    }

    struct epoll_event event = {};
    event.events = EPOLLIN;
    event.data.fd = listenerFd;
    if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, listenerFd, &event) < 0)
    {
        const std::string reason = std::strerror(errno);
        close(listenerFd);
        throw std::runtime_error("Failed to register listener in epoll: " + reason);
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

        if (!_setNonBlocking(clientFd) || !_setCloseOnExec(clientFd))
        {
            close(clientFd);
            continue;
        }

        struct epoll_event event = {};
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
        }
        return;
    }
    if (received == 0)
        _closeConnection(clientFd);
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
    if (isInput)
    {
        if (events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
            _closeCgiPipe(pipeFd);
        else if (events & EPOLLOUT)
            _handleCgiWritable(pipeFd);
    }
    else if (events & (EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP))
        _handleCgiReadable(pipeFd);
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

    if (cgi->second.inputOffset < cgi->second.input.size())
    {
        const ssize_t written = write(pipeFd,
            cgi->second.input.data() + cgi->second.inputOffset,
            cgi->second.input.size() - cgi->second.inputOffset);
        if (written > 0)
            cgi->second.inputOffset += static_cast<std::size_t>(written);
    }
    if (cgi->second.inputOffset < cgi->second.input.size())
        return;

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
    const ssize_t count = read(pipeFd, buffer, sizeof(buffer));
    if (count > 0)
    {
        const std::size_t received = static_cast<std::size_t>(count);
        if (received > CGI_MAX_OUTPUT_SIZE ||
            cgi->second.output.size() > CGI_MAX_OUTPUT_SIZE - received)
        {
            cgi->second.outputTooLarge = true;
            _terminateCgi(pid, false);
        }
        else
            cgi->second.output.append(buffer, received);
        return;
    }
    if (count == 0)
    {
        _closeCgiPipe(pipeFd);
        _tryFinalizeCgi(pid);
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

    std::string finalResponse = response;
    if (_clients[clientFd].request.getMethod() == "HEAD")
    {
        std::size_t headerEnd = finalResponse.find("\r\n\r\n");
        if (headerEnd != std::string::npos)
            finalResponse.erase(headerEnd + 4);
    }

    PendingResponse pending;
    pending.data = finalResponse;
    _pendingResponses[clientFd] = pending;
    _clients[clientFd].processing = true;

    struct epoll_event event = {};
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

    if (pending->second.offset < pending->second.data.size())
    {
        const ssize_t sent = send(clientFd,
            pending->second.data.data() + pending->second.offset,
            pending->second.data.size() - pending->second.offset, 0);
        if (sent > 0)
        {
            pending->second.offset += static_cast<std::size_t>(sent);
            if (_clients.find(clientFd) != _clients.end())
                _clients[clientFd].lastActivity = std::time(NULL);
        }
    }

    if (pending->second.offset < pending->second.data.size())
        return false;

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