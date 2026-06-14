#include "../includes/ConfigParser.hpp"
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv)
{
    std::string configPath;

    if (argc == 1)
    {
        configPath = "config/webserv.conf";
        std::cout << "[INFO] No config file specified. Using default: " << configPath << std::endl;
    }
    else if (argc == 2)
    {
        configPath = argv[1];
    }
    else
    {
        std::cout << "Usage: ./webserv [config_file_path]" << std::endl;
        return (1);
    }

    ConfigParser parser;
    std::vector<ServerConfig> servers;

    std::cout << "==================================================" << std::endl;
    std::cout << "LAUNCHING CONFIG PARSER DEBUGGER..." << std::endl;
    std::cout << "==================================================" << std::endl;

    // Protecting parsing and semantic verification inside try/catch blocks
    try
    {
        servers = parser.parseFile(configPath);
    }
    catch (const std::exception& e)
    {
        std::cerr << "\n[CRITICAL ERROR] Configuration loading aborted." << std::endl;
        std::cerr << "Reason: " << e.what() << std::endl;
        std::cout << "==================================================" << std::endl;
        return (1);
    }

    // Execution flow continues normally only if no exceptions were thrown
    std::cout << "\n[SUCCESS] Total Servers Parsed: " << servers.size() << "\n" << std::endl;

    for (size_t i = 0; i < servers.size(); ++i)
    {
        std::cout << "--------------------------------------------------" << std::endl;
        std::cout << "SERVER BLOCK [" << i << "]" << std::endl;
        std::cout << "--------------------------------------------------" << std::endl;
        std::cout << "  server_name : " << servers[i].server_name << std::endl;
        std::cout << "  port        : " << servers[i].port << std::endl;
        std::cout << "  root        : " << servers[i].root_directory << std::endl;
        std::cout << "  max_body    : " << servers[i].client_max_body_size << " bytes" << std::endl;

        std::cout << "  error_pages :" << std::endl;
        if (servers[i].error_pages.empty())
        {
            std::cout << "    (None defined, using defaults)" << std::endl;
        }
        else
        {
            std::map<int, std::string>::const_iterator err_it;
            for (err_it = servers[i].error_pages.begin(); err_it != servers[i].error_pages.end(); ++err_it)
            {
                std::cout << "    [" << err_it->first << "] -> " << err_it->second << std::endl;
            }
        }

        std::cout << "  locations   : (" << servers[i].locations.size() << " defined)" << std::endl;
        for (size_t j = 0; j < servers[i].locations.size(); ++j)
        {
            const LocationConfig& loc = servers[i].locations[j];
            std::cout << "    └── path: " << loc.path << std::endl;
            std::cout << "        ├── root: " << loc.root_directory << std::endl;
            
            std::cout << "        ├── allowed_methods: [ ";
            for (size_t m = 0; m < loc.allowed_methods.size(); ++m)
            {
                std::cout << loc.allowed_methods[m] << " ";
            }
            std::cout << "]" << std::endl;

            std::cout << "        ├── autoindex: " << (loc.autoindex ? "on" : "off") << std::endl;

            std::cout << "        ├── index_files: [ ";
            for (size_t idx = 0; idx < loc.index_files.size(); ++idx)
            {
                std::cout << loc.index_files[idx] << " ";
            }
            std::cout << "]" << std::endl;

            if (loc.return_code != 0)
            {
                std::cout << "        ├── return (redirect): " << loc.return_code << " -> " << loc.return_url << std::endl;
            }

            if (!loc.upload_path.empty())
            {
                std::cout << "        └── upload_path: " << loc.upload_path << std::endl;
            }
            std::cout << "        " << std::endl;
        }
        std::cout << std::endl;
    }

    std::cout << "==================================================" << std::endl;
    std::cout << "END OF DEBUG DISPLAY" << std::endl;
    std::cout << "==================================================" << std::endl;

    return (0);
}