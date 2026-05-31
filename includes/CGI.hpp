#include "Server.hpp"
#include "Client.hpp"
#include "Response.hpp"
#include "MethodHandler.hpp"
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <sstream>
#include <cstring>
#include <map>
#include <algorithm>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include "Client.hpp"
#include "Config.hpp"
#include "Response.hpp"

#ifndef CGI_HPP
#define CGI_HPP

extern volatile sig_atomic_t g_shutdown_flag;

void signalHandler(int signum);

void setupSignalHandler();

int manageCGI(Client &client, Response &response);

#endif // CGI_HPP
