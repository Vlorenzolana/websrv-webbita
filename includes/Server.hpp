#ifndef SERVER_HPP
#define SERVER_HPP
extern int g_epoll_fd;


#include <unistd.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdio>
#include <stdexcept>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <map>
#include <algorithm>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include "Client.hpp"
#include "Config.hpp"
#include "Response.hpp"
#include "User.hpp"

#define MAX_FDS 200

class Server
{
	private:
		const Config& _config;
		std::map<int, Client> _clients;
		std::vector<int> _listenSockets;
		std::map<int, ServerConfig> _listener_configs;
		std::map<unsigned int, User> _registered_users;

		void acceptNewConnection(int listener_fd, int epoll_fd);
		void handleClientRequest(int client_fd, int epoll_fd);
		void handleClientResponse(int client_fd, int epoll_fd);
		void closeClientConnection(int client_fd, int epoll_fd);
		void handleCGIEvent(Client& client, int cgi_fd, int epoll_fd);
		void checkCGITimeouts();

	public:
		Server(const Config& config);
		~Server();
		void run();
        std::map<unsigned int, User> getRegisteredUsers();

        bool addUser(const std::string& name, const std::string& password , const std::string& email);
        User* findUser(unsigned int id);
        const std::map<unsigned int, User>& getRegisteredUsersRef() const;
        std::map<unsigned int, User>& getRegisteredUsersRefNoConst();
		const User* findUserByName(const std::string& name) const;
	};

#endif // SERVER_HPP
