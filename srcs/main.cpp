#include "../includes/ConfigParser.hpp"
#include "../includes/Server.hpp"

#include <csignal>
#include <iostream>
#include <vector>

int main(int argc, char** argv)
{
    const char* configPath = argc >= 2 ? argv[1] : "config/webserv.conf";
    std::signal(SIGPIPE, SIG_IGN);

    try
    {
        ConfigParser parser;
        const std::vector<ServerConfig> servers = parser.parseFile(configPath);
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
