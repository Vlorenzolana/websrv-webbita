#include "../includes/ConfigParser.hpp"
#include "../includes/ConfigValidator.hpp"
#include "../includes/Server.hpp"
#include <iostream>
#include <csignal>
#include <vector>
#include <pthread.h>

void* runServer(void* arg)
{
    Server* server = static_cast<Server*>(arg);

    server->init();
    server->run();
    return NULL;
}

int main(int argc, char** argv)
{
    const char* path = (argc >= 2) ? argv[1] : "www/webserv.conf";
    std::signal(SIGPIPE, SIG_IGN);

    try
    {
        ConfigParser parser;
        std::vector<ServerConfig> servers_cfg = parser.parseFile(path);
        
        if (servers_cfg.empty())
            throw std::runtime_error("No server configurations found.");

        std::cout << "=== Starting " << servers_cfg.size() << " virtual host(s) ===" << std::endl;

        std::vector<Server*> servers;
        std::vector<pthread_t> threads;

        // Creamos e iniciamos un hilo independiente para cada servidor configurado
        for (size_t i = 0; i < servers_cfg.size(); ++i)
        {
            Server* server = new Server(servers_cfg[i]);
            servers.push_back(server);

            pthread_t thread_id;
            if (pthread_create(&thread_id, NULL, runServer, server) != 0)
                std::cerr << "Error creating thread for server on port " << servers_cfg[i].port << std::endl;

            else
            {
                threads.push_back(thread_id);
                std::cout << "Virtual Host initialized - listening on port " << servers_cfg[i].port << std::endl;
            }
        }

        // Esperamos a que los hilos terminen (bucle infinito de ejecución)
        for (size_t i = 0; i < threads.size(); ++i)
            pthread_join(threads[i], NULL);

        // Liberación limpia de memoria en caso de salida
        for (size_t i = 0; i < servers.size(); ++i)
            delete servers[i];

    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}