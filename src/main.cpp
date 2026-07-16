#include "../includes/ConfigParser.hpp"
#include "../includes/ConfigValidator.hpp"
#include "../includes/Server.hpp"
#include <iostream>
#include <csignal>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char** argv)
{
    const char* path = (argc >= 2) ? argv[1] : "config/minimal_webserv.conf";
    std::signal(SIGPIPE, SIG_IGN);

    try
    {
        ConfigParser parser;
        std::vector<ServerConfig> servers_cfg = parser.parseFile(path);
        
        if (servers_cfg.empty())
            throw std::runtime_error("No server configurations found.");

        std::cout << "=== Starting " << servers_cfg.size() << " virtual host(s) ===" << std::endl;

        std::vector<pid_t> pids;

        // Creamos un proceso independiente mediante fork() para cada servidor configurado
        for (size_t i = 0; i < servers_cfg.size(); ++i)
        {
            pid_t pid = fork();
            if (pid < 0)
                std::cerr << "Error creating process for server on port " << servers_cfg[i].port << std::endl;

            else if (pid == 0)
            {
                // Código del proceso hijo: Instancia y arranca su propio servidor aislado
                Server server(servers_cfg[i]);
                server.init();
                std::cout << "Virtual Host initialized - listening on port " << servers_cfg[i].port << std::endl;
                server.run();
                return 0;
            }
            else
                pids.push_back(pid);

        }

        // El proceso padre espera activamente a que todos sus procesos hijos terminen
        for (size_t i = 0; i < pids.size(); ++i)
            waitpid(pids[i], NULL, 0);

    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}