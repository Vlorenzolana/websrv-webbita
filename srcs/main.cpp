#include "../includes/ConfigParser.hpp"
#include "../includes/Server.hpp"

#include <csignal>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

static int parsePortArgument(const char* value)
{
    std::stringstream stream(value);
    int port = 0;
    char extra = '\0';

    if (!(stream >> port) || (stream >> extra) || port < 1 || port > 65535)
        throw std::runtime_error("Invalid command-line port (1-65535): " +
            std::string(value));
    return port;
}

int main(int argc, char** argv)
{
    if (argc > 3)
    {
        std::cerr << "Usage: " << argv[0] << " [config_file] [port]" << std::endl;
        return 1;
    }

    const char* configPath = argc >= 2 ? argv[1] : "config/webserv.conf";
    std::signal(SIGPIPE, SIG_IGN);

    try
    {
        ConfigParser parser;
        std::vector<ServerConfig> servers = parser.parseFile(configPath);
        if (argc == 3 && std::string(argv[2]) == "PORT+1")
        {
            for (std::size_t i = 0; i < servers.size(); ++i)
            {
                if (servers[i].port >= 65535)
                    throw std::runtime_error("PORT+1 exceeds port range");
                ++servers[i].port;
            }
        }
        else if (argc == 3)
        {
            const int port = parsePortArgument(argv[2]);
            for (std::size_t i = 0; i < servers.size(); ++i)
                servers[i].port = port;
        }
        Server server(servers);
        server.init();
        server.run();
    }
    catch (const std::exception& exception)
    {
        std::cerr << "webserv: " << exception.what() << std::endl;
        return 1;
    }
    return 0;
}
