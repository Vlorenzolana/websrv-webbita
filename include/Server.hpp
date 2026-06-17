#ifndef SERVER_HPP
#define SERVER_HPP

#include <sys/epoll.h>

class Server
{
private:
    int _serverFd;
    int _epollFd;
    int _port;

public:
    Server(int port);
    ~Server();

    void init();
    void run();

private:
    void acceptClient();
    void handleClient(int clientFd);
};

#endif
