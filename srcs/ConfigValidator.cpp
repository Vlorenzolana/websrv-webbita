#include "../includes/ConfigValidator.hpp"
#include <arpa/inet.h>
#include <sstream>
#include <stdexcept>
#include <unistd.h>

ConfigValidator::ConfigValidator() {}
ConfigValidator::~ConfigValidator() {}

// Iterates through all server configurations to perform semantic validation
void ConfigValidator::validateAndNormalize(std::vector<ServerConfig>& servers)
{
    if (servers.empty())
        throw std::runtime_error("No server configurations found");

    for (std::size_t i = 0; i < servers.size(); ++i)
    {
        _hydrateAndCheckServer(servers[i]);
        _checkDuplicateServers(servers, i);
        _validateAndNormalizeLocations(servers[i]);
    }
}

// Populates default server values, normalizes hostnames, and validates root access
void ConfigValidator::_hydrateAndCheckServer(ServerConfig& server)
{
    if (server.server_name.empty())
        server.server_name = "localhost";
    if (server.root_directory.empty())
        server.root_directory = "./www";

    // Normalize localhost alias to loopback address
    if (server.host == "localhost")
        server.host = "127.0.0.1";

    struct in_addr address;
    if (inet_pton(AF_INET, server.host.c_str(), &address) != 1)
        throw std::runtime_error("Invalid IPv4 listen host: " + server.host);

    if (access(server.root_directory.c_str(), F_OK) != 0 ||
        access(server.root_directory.c_str(), R_OK) != 0)
        throw std::runtime_error("Server root is missing or unreadable: " + server.root_directory);

    _validateErrorPages(server.error_pages, server.root_directory);
}

// Ensures no two server blocks listen on the exact same host, port, and server_name combination
void ConfigValidator::_checkDuplicateServers(
    const std::vector<ServerConfig>& servers, std::size_t currentIndex)
{
    const ServerConfig& current = servers[currentIndex];
    for (std::size_t i = 0; i < currentIndex; ++i)
    {
        if (servers[i].host == current.host &&
            servers[i].port == current.port &&
            servers[i].server_name == current.server_name)
        {
            std::ostringstream message;
            message << "Duplicate server block on " << current.host << ":"
                    << current.port << " with server_name '"
                    << current.server_name << "'";
            throw std::runtime_error(message.str());
        }
    }
}

// Normalizes location defaults and validates routes, permissions, and CGI paths
void ConfigValidator::_validateAndNormalizeLocations(ServerConfig& server)
{
    if (server.locations.empty())
    {
        LocationConfig root;
        root.path = "/";
        server.locations.push_back(root);
    }

    for (std::size_t i = 0; i < server.locations.size(); ++i)
    {
        LocationConfig& location = server.locations[i];
        if (location.path.empty() || location.path[0] != '/')
            throw std::runtime_error("Location path must start with '/': " + location.path);

        for (std::size_t j = 0; j < i; ++j)
        {
            if (server.locations[j].path == location.path)
                throw std::runtime_error("Duplicate location: " + location.path);
        }

        if (location.allowed_methods.empty())
            location.allowed_methods.push_back("GET");

        if (location.root_directory.empty())
            location.root_directory = server.root_directory;

        if (access(location.root_directory.c_str(), F_OK) != 0 ||
            access(location.root_directory.c_str(), R_OK) != 0)
            throw std::runtime_error("Location root is missing or unreadable: " + location.root_directory);

        if (!location.upload_path.empty() &&
            (access(location.upload_path.c_str(), F_OK) != 0 ||
             access(location.upload_path.c_str(), W_OK) != 0))
            throw std::runtime_error("Upload path is missing or not writable: " + location.upload_path);

        if (location.return_code != 0)
            _validateLocationRedirection(location);

        for (std::map<std::string, std::string>::const_iterator it =
                 location.cgi_interpreters.begin();
             it != location.cgi_interpreters.end(); ++it)
        {
            if (access(it->second.c_str(), X_OK) != 0)
                throw std::runtime_error("CGI interpreter is missing or not executable: " + it->second);
        }

        _validateErrorPages(location.error_pages, location.root_directory);
    }
}

// Checks if the HTTP redirection status code falls in the 3xx range
void ConfigValidator::_validateLocationRedirection(
    const LocationConfig& location)
{
    if (location.return_code < 300 || location.return_code > 399)
        throw std::runtime_error("return status must be a 3xx code");
    if (location.return_url.empty())
        throw std::runtime_error("return directive requires a target URL");
}

// Verifies that configured custom error pages exist and are readable on disk
void ConfigValidator::_validateErrorPages(
    const std::map<int, std::string>& errorPages,
    const std::string& rootDirectory)
{
    for (std::map<int, std::string>::const_iterator it = errorPages.begin();
         it != errorPages.end(); ++it)
    {
        std::string path = it->second;
        if (!path.empty() && path[0] == '/')
            path = rootDirectory + path;
        if (access(path.c_str(), R_OK) != 0)
            throw std::runtime_error("Configured error page is unreadable: " + path);
    }
}