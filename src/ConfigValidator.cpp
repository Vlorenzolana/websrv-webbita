#include "../includes/ConfigValidator.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cstdlib>
#include <unistd.h> // Required for access() (F_OK, R_OK)

ConfigValidator::ConfigValidator()
{
}

ConfigValidator::~ConfigValidator()
{
}

void ConfigValidator::validateAndNormalize(std::vector<ServerConfig>& servers)
{
    if (servers.empty())
    {
        throw std::runtime_error("No server configurations found.");
    }

    for (size_t i = 0; i < servers.size(); ++i)
    {
        _hydrateAndCheckServer(servers[i]);
        _checkDuplicateServers(servers, i);
        _validateAndNormalizeLocations(servers[i]);
    }
}

void ConfigValidator::_hydrateAndCheckServer(ServerConfig& server)
{
    // Hydrate default values if missing
    if (server.server_name.empty())
    {
        server.server_name = "localhost";
    }
    if (server.root_directory.empty())
    {
        server.root_directory = "./www";
    }

    // Accessibility checks for Server Root
    if (access(server.root_directory.c_str(), F_OK) != 0)
    {
        throw std::runtime_error("Server root directory does not exist: " + server.root_directory);
    }
    if (access(server.root_directory.c_str(), R_OK) != 0)
    {
        throw std::runtime_error("Server root directory is not readable: " + server.root_directory);
    }
}

void ConfigValidator::_checkDuplicateServers(const std::vector<ServerConfig>& servers, size_t currentIndex)
{
    const ServerConfig& current = servers[currentIndex];
    for (size_t j = 0; j < currentIndex; ++j)
    {
        if (servers[j].port == current.port && servers[j].server_name == current.server_name)
        {
            std::stringstream ss;
            ss << "Duplicate server block detected on port " << current.port 
               << " with server_name '" << current.server_name << "'.";
            throw std::runtime_error(ss.str());
        }
    }
}

void ConfigValidator::_validateAndNormalizeLocations(ServerConfig& server)
{
    for (size_t k = 0; k < server.locations.size(); ++k)
    {
        LocationConfig& loc = server.locations[k];

        // Hydrate default HTTP methods if block is empty
        if (loc.allowed_methods.empty())
        {
            loc.allowed_methods.push_back("GET");
        }

        // Route root synchronization: Inherit from server root if undefined
        if (loc.root_directory.empty())
        {
            loc.root_directory = server.root_directory;
        }
        else
        {
            if (access(loc.root_directory.c_str(), F_OK) != 0 || access(loc.root_directory.c_str(), R_OK) != 0)
            {
                throw std::runtime_error("Location root path invalid or inaccessible: " + loc.root_directory);
            }
        }

        // Check upload folder directory availability if specified
        if (!loc.upload_path.empty())
        {
            if (access(loc.upload_path.c_str(), F_OK) != 0)
            {
                throw std::runtime_error("Location upload path does not exist: " + loc.upload_path);
            }
        }

        // Validate redirections
        if (loc.return_code != 0)
        {
            _validateLocationRedirection(loc);
        }
    }
}

void ConfigValidator::_validateLocationRedirection(const LocationConfig& loc)
{
    if (loc.return_code < 300 || loc.return_code > 399)
    {
        std::stringstream ss;
        ss << "Invalid HTTP redirection code: " << loc.return_code << " (Must be a 3xx status code).";
        throw std::runtime_error(ss.str());
    }
    if (loc.return_url.empty())
    {
        std::stringstream ss;
        ss << "Redirection code " << loc.return_code << " requires a valid target URL.";
        throw std::runtime_error(ss.str());
    }
}