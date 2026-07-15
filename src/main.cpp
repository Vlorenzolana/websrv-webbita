#include "../includes/ConfigParser.hpp"
#include "../includes/ConfigValidator.hpp"
#include "../includes/Server.hpp"

#include <iostream>
#include <csignal>
#include <vector>
#include <pthread.h>
#include <unistd.h>

// Estructura para pasar datos al thread
struct ServerThreadData
{
	Server* server;
	int port;
};

// Función que ejecuta en cada thread
void* runServer(void* arg)
{
	ServerThreadData* data = static_cast<ServerThreadData*>(arg);
	std::cout << "Thread started for port " << data->port << " (Thread ID: " << pthread_self() << ")" << std::endl;
	
	try {
		data->server->run();
	}
	catch (const std::exception& e) {
		std::cerr << "Server error on port " << data->port << ": " << e.what() << std::endl;
	}
	
	delete data;
	return NULL;
}

int main(int argc, char** argv)
{
	const char* path = (argc >= 2) ? argv[1] : "www/webserv.conf";

	std::signal(SIGPIPE, SIG_IGN);

	try {
		ConfigParser parser;
		std::vector<ServerConfig> servers = parser.parseFile(path);

		// Si el config solo define 1 server, se crea 1 instancia y 1 thread.
		// Si define varios server blocks, se crea una instancia/thread por cada uno.
		if (servers.empty())
		{
			std::cerr << "Error: No server configurations found" << std::endl;
			return 1;
		}

		std::vector<Server*> serverInstances;
		std::vector<pthread_t> threads;

		// Crear una instancia de Server por cada bloque server del config.
		for (size_t i = 0; i < servers.size(); ++i)
		{
			Server* srv = new Server(servers[i]);
			srv->init();
			serverInstances.push_back(srv);
			std::cout << "Virtual Host " << i << " initialized - listening on port " 
					  << servers[i].port << " (Server name: " << servers[i].server_name << ")" << std::endl;
		}

		std::cout << "\n=== Starting " << serverInstances.size() << " virtual host(s) ===" << std::endl;

		// Cada Server corre en su propio thread para poder escuchar varios puertos a la vez.
		for (size_t i = 0; i < serverInstances.size(); ++i)
		{
			ServerThreadData* data = new ServerThreadData();
			data->server = serverInstances[i];
			data->port = servers[i].port;

			pthread_t thread;
			if (pthread_create(&thread, NULL, runServer, data) != 0)
			{
				std::cerr << "Error creating thread for server " << i << std::endl;
				delete data;
				continue;
			}
			threads.push_back(thread);
		}

		// Esperar a que terminen todos los threads: con 1 server, espera uno;
		// con varios, espera a todos.
		for (size_t i = 0; i < threads.size(); ++i)
		{
			pthread_join(threads[i], NULL);
		}

		// Limpiar memoria
		for (size_t i = 0; i < serverInstances.size(); ++i)
			delete serverInstances[i];

		std::cout << "\nAll servers stopped." << std::endl;
	}
	catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}

/* #include "../includes/Server.hpp"

int main()
{
	Server server(3000);
	server.init();
	server.run();
	return 0;
}
 */
/*
#include "../includes/ConfigParser.hpp"
#include "../includes/ConfigValidator.hpp"
#include "../includes/Server.hpp"

#include <iostream>
#include <csignal>
#include <vector>

int main(int argc, char** argv)
{
	const char* path = (argc >= 2) ? argv[1] : "www/webserv.conf";

	std::signal(SIGPIPE, SIG_IGN);

	try {
		ConfigParser parser;
		std::vector<ServerConfig> servers = parser.parseFile(path);
		ConfigValidator validator;
		validator.validateAndNormalize(servers);

		// MVP: usamos solo el primer server block
		Server server(servers[0]);
		server.init();
		std::cout << "webserv listening on port " << servers[0].port << std::endl;
		server.run();
	}
	catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}*/