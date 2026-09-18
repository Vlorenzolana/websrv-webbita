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

Server::Server(const std::vector<ServerConfig>& servers)
    : _servers(servers), _epollFd(-1)
{
}

// Closes CGI pipes, clients, listeners, and the epoll descriptor.
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

// Enables non-blocking mode so socket calls return instead of waiting forever.
bool Server::_setNonBlocking(int fd)
{
    const int flags = fcntl(fd, F_GETFL, 0);
    return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) >= 0;
}

// Prevents descriptors from being inherited by a program started with execve().
bool Server::_setCloseOnExec(int fd)
{
    const int flags = fcntl(fd, F_GETFD, 0);
    return flags >= 0 && fcntl(fd, F_SETFD, flags | FD_CLOEXEC) >= 0;
}

std::string Server::_trim(const std::string& value)
{
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return "";
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

// Converts ASCII uppercase letters to lowercase for case-insensitive matching.
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

// Converts a number to text for log and error messages.
std::string Server::_intToString(long value)
{
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

// Creates the epoll monitor and opens one listening socket per host/port pair.
void Server::init()
{
    // epoll_create() creates the event monitor; fcntl() adds FD_CLOEXEC below.
    _epollFd = epoll_create(1);
    if (_epollFd < 0 || !_setCloseOnExec(_epollFd))
    {
        if (_epollFd >= 0)
            close(_epollFd);
        throw std::runtime_error("Failed to create epoll instance: " +
            std::string(std::strerror(errno)));
    }

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

// Waits for activity and dispatches each ready descriptor to the right handler.
void Server::run()
{
    struct epoll_event events[128];
    while (true)
    {
        // Wait up to 1000 ms; the timeout lets periodic cleanup still run.
        const int count = epoll_wait(_epollFd, events, 128, 1000);
        if (count < 0)
        {
            if (errno == EINTR)
                continue;
            throw std::runtime_error("epoll_wait failed: " +
                std::string(std::strerror(errno)));
        }

        for (int i = 0; i < count; ++i)
        {
            const int fd = events[i].data.fd;
            // A listener with EPOLLIN means a new client can be accepted.
            if (_listeners.find(fd) != _listeners.end())
                _acceptClients(fd);
            // CGI pipes use EPOLLIN/EPOLLOUT to exchange data with the child.
            else if (_cgiPipeRefs.find(fd) != _cgiPipeRefs.end())
                _handleCgiEvent(fd, events[i].events);
            // Client sockets use the event flags to read requests or write responses.
            else if (_clients.find(fd) != _clients.end())
                _handleClientEvent(fd, events[i].events);
            else
            {
                // Unknown descriptors are removed from epoll and closed.
                epoll_ctl(_epollFd, EPOLL_CTL_DEL, fd, NULL);
                close(fd);
            }
        }

        _reapCgiProcesses();
        _checkTimeouts();
    }
}