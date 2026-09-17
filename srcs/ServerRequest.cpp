#include "../includes/Server.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

void Server::_processRequest(int clientFd, const Request& request,
    int listenPort, const std::string& listenHost)
{
    const ServerConfig* server =
        _selectServerConfig(listenPort, listenHost, request);
    if (request.getErrorCode() != 0)
    {
        _sendErrorResponse(clientFd, request.getErrorCode(), server, NULL);
        return;
    }
    if (server == NULL)
    {
        _sendErrorResponse(clientFd, 404, NULL, NULL);
        return;
    }

    const LocationConfig* location = _matchLocation(*server, request.getPath());
    if (location == NULL)
    {
        _sendErrorResponse(clientFd, 404, server, NULL);
        return;
    }
    if (server->client_max_body_size > 0 &&
        request.getBody().size() >
            static_cast<std::size_t>(server->client_max_body_size))
    {
        _sendErrorResponse(clientFd, 413, server, location);
        return;
    }
    if (!_isMethodAllowed(location, request.getMethod()))
    {
        _sendErrorResponse(clientFd, 405, server, location);
        return;
    }
    if (_hasPathTraversal(request.getPath()))
    {
        _sendErrorResponse(clientFd, 403, server, location);
        return;
    }

    if (location->return_code != 0)
    {
        std::map<std::string, std::string> headers;
        headers["Location"] = location->return_url;
        _queueResponse(clientFd, _buildResponse(location->return_code,
            _statusText(location->return_code), "text/plain", "", headers));
        return;
    }

    const std::string fullPath = _resolvePath(*server, location, request.getPath());
    struct stat fileStat;
    std::string interpreter;
    if (stat(fullPath.c_str(), &fileStat) == 0 && S_ISREG(fileStat.st_mode) &&
        _findCgiInterpreter(location, fullPath, interpreter))
    {
        if (_cgiByPid.size() >= CGI_MAX_PROCESSES)
        {
            _sendErrorResponse(clientFd, 503, server, location);
            return;
        }
        if (!_startCgi(clientFd, request, *server, location, fullPath))
            _sendErrorResponse(clientFd, 500, server, location);
        return;
    }

    std::string response;

    if (request.getMethod() == "GET" || request.getMethod() == "HEAD")
        response = _handleGet(*server, request, location);
    else if (request.getMethod() == "POST")
        response = _handlePost(*server, request, location);
    else
        response = _handleDelete(*server, request, location);

    _queueResponse(clientFd, response);
}

const ServerConfig *Server::_selectServerConfig(int listenPort,
    const std::string& listenHost, const Request& request) const
{
    std::string host = request.getHeaderValue("host");
    const std::size_t colon = host.find(':');
    if (colon != std::string::npos)
        host.erase(colon);
    host = _toLower(_trim(host));

    const ServerConfig* fallback = NULL;
    for (std::size_t i = 0; i < _servers.size(); ++i)
    {
        if (_servers[i].port != listenPort || _servers[i].host != listenHost)
            continue;
        if (fallback == NULL)
            fallback = &_servers[i];
        if (!host.empty() && _toLower(_servers[i].server_name) == host)
            return &_servers[i];
    }
    return fallback;
}

const ServerConfig* Server::_selectDefaultServer(int listenPort,
    const std::string& listenHost) const
{
    for (std::size_t i = 0; i < _servers.size(); ++i)
    {
        if (_servers[i].port == listenPort && _servers[i].host == listenHost)
            return &_servers[i];
    }
    return NULL;
}

const LocationConfig* Server::_matchLocation(const ServerConfig& server,
    const std::string& path) const
{
    const LocationConfig* best = NULL;
    std::size_t bestLength = 0;
    for (std::size_t i = 0; i < server.locations.size(); ++i)
    {
        const std::string& prefix = server.locations[i].path;
        if (path.compare(0, prefix.size(), prefix) != 0)
            continue;
        const bool boundary = prefix == "/" ||
            (!prefix.empty() && prefix[prefix.size() - 1] == '/') ||
            path.size() == prefix.size() ||
            (path.size() > prefix.size() && path[prefix.size()] == '/');
        if (boundary && prefix.size() > bestLength)
        {
            best = &server.locations[i];
            bestLength = prefix.size();
        }
    }
    return best;
}

bool Server::_isMethodAllowed(const LocationConfig* location,
    const std::string& method) const
{
    if (location == NULL)
        return false;
    for (std::size_t i = 0; i < location->allowed_methods.size(); ++i)
    {
        if (location->allowed_methods[i] == method)
            return true;
    }
    return false;
}

std::string Server::_resolvePath(const ServerConfig& server,
    const LocationConfig* location, const std::string& requestPath) const
{
    const std::string root = location != NULL && !location->root_directory.empty()
        ? location->root_directory : server.root_directory;
    std::string relative = requestPath;
    if (location != NULL && relative.compare(0, location->path.size(),
            location->path) == 0)
        relative.erase(0, location->path.size());
    while (!relative.empty() && relative[0] == '/')
        relative.erase(0, 1);

    std::string result = root;
    if (!result.empty() && result[result.size() - 1] != '/')
        result += '/';
    result += relative;
    return result;
}

bool Server::_hasPathTraversal(const std::string& path) const
{
    std::string decoded;
    for (std::size_t i = 0; i < path.size(); ++i)
    {
        if (path[i] == '%' && i + 2 < path.size())
        {
            const std::string hex = path.substr(i + 1, 2);
            char* end = NULL;
            const long value = std::strtol(hex.c_str(), &end, 16);
            if (end != NULL && *end == '\0')
            {
                decoded += static_cast<char>(value);
                i += 2;
                continue;
            }
        }
        decoded += path[i];
    }
    if (decoded.find('\0') != std::string::npos)
        return true;

    std::stringstream stream(decoded);
    std::string segment;
    while (std::getline(stream, segment, '/'))
    {
        if (segment == "..")
            return true;
    }
    return false;
}

bool Server::_findCgiInterpreter(const LocationConfig* location,
    const std::string& fullPath, std::string& interpreter) const
{
    if (location == NULL)
        return false;
    const std::size_t slash = fullPath.find_last_of('/');
    const std::size_t dot = fullPath.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return false;
    const std::map<std::string, std::string>::const_iterator it =
        location->cgi_interpreters.find(fullPath.substr(dot));
    if (it == location->cgi_interpreters.end())
        return false;
    interpreter = it->second;
    return true;
}

