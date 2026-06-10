#include "../includes/ConfigParser.hpp"
#include <iostream>

int main(int argc, char** argv)
{
    std::string configPath;

    // Command-line parameter evaluation fallback
    if (argc == 1)
    {
        // No explicit parameter provided; look for default workspace path
        configPath = "config/webserv.conf";
        std::cout << "[INFO] No config file specified. Using default: " << configPath << std::endl;
    }
    else if (argc == 2)
    {
        // Utilize the custom configuration path supplied by the evaluator
        configPath = argv[1];
    }
    else
    {
        std::cout << "Usage: ./webserv [config_file_path]" << std::endl;
        return (1);
    }

    ConfigParser parser;
    
    std::cout << "==================================================" << std::endl;
    std::cout << "LAUNCHING CONFIG PARSER DEBUGGER..." << std::endl;
    std::cout << "==================================================" << std::endl;

    std::vector<ServerConfig> servers = parser.parseFile(configPath);

    std::cout << "\n[SUCCESS] Total Servers Parsed: " << servers.size() << "\n" << std::endl;

    // Secure iterations looping over completely extracted structural data
    for (size_t i = 0; i < servers.size(); ++i)
    {
        std::cout << "--------------------------------------------------" << std::endl;
        std::cout << "SERVER BLOCK [" << i << "]" << std::endl;
        std::cout << "--------------------------------------------------" << std::endl;
        std::cout << "  server_name : " << servers[i].server_name << std::endl;
        std::cout << "  port        : " << servers[i].port << std::endl;
        std::cout << "  root        : " << servers[i].root_directory << std::endl;
        std::cout << "  max_body    : " << servers[i].client_max_body_size << " bytes" << std::endl;

        // Display configured high-level global server error pages
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

        // Trace and dump localized nested sub-routes (Locations)
        std::cout << "  locations   : (" << servers[i].locations.size() << " defined)" << std::endl;
        for (size_t j = 0; j < servers[i].locations.size(); ++j)
        {
            const LocationConfig& loc = servers[i].locations[j];
            std::cout << "    └── path: " << loc.path << std::endl;
            
            if (!loc.root_directory.empty())
            {
                std::cout << "        ├── root: " << loc.root_directory << std::endl;
            }
            
            // HTTP Request Methods validation printing
            std::cout << "        ├── allowed_methods: [ ";
            for (size_t m = 0; m < loc.allowed_methods.size(); ++m)
            {
                std::cout << loc.allowed_methods[m] << " ";
            }
            std::cout << "]" << std::endl;

            // Directory Listing switch parameter
            std::cout << "        ├── autoindex: " << (loc.autoindex ? "on" : "off") << std::endl;

            // Target default index file arrays
            std::cout << "        ├── index_files: [ ";
            for (size_t idx = 0; idx < loc.index_files.size(); ++idx)
            {
                std::cout << loc.index_files[idx] << " ";
            }
            std::cout << "]" << std::endl;

            // Internal redirection triggers
            if (loc.return_code != 0)
            {
                std::cout << "        ├── return (redirect): " << loc.return_code << " -> " << loc.return_url << std::endl;
            }

            // Client data submittal target storage folders
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