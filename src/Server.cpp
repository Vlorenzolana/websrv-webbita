#include "Server.hpp"
#include <iostream>
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>

Server::Server(int port) : _serverFd(-1), _epollFd(-1), _port(port) {}

Server::~Server()
{
    if (_serverFd >= 0) close(_serverFd);
    if (_epollFd >= 0) close(_epollFd);
}

void Server::init()
{
    int opt = 1;
    sockaddr_in addr;

    _serverFd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(_serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    fcntl(_serverFd, F_SETFL, O_NONBLOCK);

    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(_port);

    bind(_serverFd, (sockaddr *)&addr, sizeof(addr));
    listen(_serverFd, SOMAXCONN);

    _epollFd = epoll_create1(0);

    epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = _serverFd;

    epoll_ctl(_epollFd, EPOLL_CTL_ADD, _serverFd, &ev);
}

void Server::acceptClient()
{
    int clientFd = accept(_serverFd, NULL, NULL);
    if (clientFd < 0)
        return;

    fcntl(clientFd, F_SETFL, O_NONBLOCK);

    epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = clientFd;
    epoll_ctl(_epollFd, EPOLL_CTL_ADD, clientFd, &ev);

    std::cout << "Client connected: " << clientFd << std::endl;
}

void Server::handleClient(int clientFd)
{
    char buf[4096];
    ssize_t n = recv(clientFd, buf, sizeof(buf) - 1, 0);
    if (n <= 0)
    {
        epoll_ctl(_epollFd, EPOLL_CTL_DEL, clientFd, NULL);
        close(clientFd);
        return;
    }

    const char *body = "Hello, World!\n";
    char response[256];
    int len = std::snprintf(response, sizeof(response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        std::strlen(body), body);

    send(clientFd, response, len, 0);
    epoll_ctl(_epollFd, EPOLL_CTL_DEL, clientFd, NULL);
    close(clientFd);
}

void Server::run()
{
    epoll_event events[64];

    while (true)
    {
        int n = epoll_wait(_epollFd, events, 64, -1);

        for (int i = 0; i < n; ++i)
        {
            if (events[i].data.fd == _serverFd)
                acceptClient();
            else
                handleClient(events[i].data.fd);
        }
    }
}
