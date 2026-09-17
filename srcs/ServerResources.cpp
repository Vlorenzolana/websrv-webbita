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

std::string Server::_handleGet(const ServerConfig& server,
    const Request& request, const LocationConfig* location) const
{
    std::string fullPath =
        _resolvePath(server, location, request.getPath());

    struct stat fileStat;
    if (stat(fullPath.c_str(), &fileStat) != 0)
        return _buildErrorResponse(404, &server, location);

    if (S_ISDIR(fileStat.st_mode))
    {
        if (!request.getPath().empty() && request.getPath()[request.getPath().size() - 1] != '/')
        {
            std::map<std::string, std::string> headers;
            std::string host = request.getHeaderValue("host");
            if (host.empty())
                host = server.host + ":" + _intToString(server.port);
            
            headers["Location"] = "http://" + host + request.getPath() + "/";
            return _buildResponse(301, _statusText(301), "text/html",
                _defaultErrorBody(301, _statusText(301)), headers);
        }

        bool foundIndex = false;
        for (std::size_t i = 0; i < location->index_files.size(); ++i)
        {
            std::string indexPath = fullPath;
            if (!indexPath.empty() && indexPath[indexPath.size() - 1] != '/')
                indexPath += '/';
            indexPath += location->index_files[i];
            
            struct stat indexStat;
            if (stat(indexPath.c_str(), &indexStat) == 0 &&
                S_ISREG(indexStat.st_mode))
            {
                fullPath = indexPath;
                foundIndex = true;
                break;
            }
        }
        if (!foundIndex)
        {
            if (location->autoindex)
                return _buildResponse(200, _statusText(200), "text/html",
                    _buildAutoindexPage(fullPath, request.getPath()));
            return _buildErrorResponse(404, &server, location);
        }
    }
    else if (!S_ISREG(fileStat.st_mode))
        return _buildErrorResponse(403, &server, location);

    std::ifstream file(fullPath.c_str(), std::ios::binary);
    if (!file.is_open())
        return _buildErrorResponse(403, &server, location);
    std::ostringstream content;
    content << file.rdbuf();
    return _buildResponse(200, _statusText(200), _getMimeType(fullPath),
        content.str());
}

std::string Server::_handlePost(const ServerConfig& server,
    const Request& request, const LocationConfig* location) const
{
    if (location->upload_path.empty())
    {
        const std::string fullPath =
            _resolvePath(server, location, request.getPath());
        struct stat fileStat;
        if (stat(fullPath.c_str(), &fileStat) != 0)
            return _buildErrorResponse(404, &server, location);
        return _buildErrorResponse(403, &server, location);
    }

    const std::string contentType = _toLower(request.getHeaderValue("content-type"));
    std::vector<std::string> savedNames;
    bool saved = false;
    if (contentType.find("multipart/form-data") == 0)
        saved = _saveMultipartUpload(request, location, savedNames);
    else
    {
        std::string name;
        saved = _saveRawUpload(request, location, name);
        if (saved)
            savedNames.push_back(name);
    }

    if (!saved)
        return _buildErrorResponse(400, &server, location);

    std::ostringstream body;
    body << "Uploaded";
    for (std::size_t i = 0; i < savedNames.size(); ++i)
        body << (i == 0 ? ": " : ", ") << savedNames[i];
    body << "\n";

    std::map<std::string, std::string> headers;
    headers["Location"] = request.getPath();
    return _buildResponse(201, _statusText(201), "text/plain", body.str(), headers);
}

std::string Server::_handleDelete(const ServerConfig& server,
    const Request& request, const LocationConfig* location) const
{
    const std::string fullPath = _resolvePath(server, location, request.getPath());
    const int fileFd = open(fullPath.c_str(), O_RDONLY | O_NOFOLLOW);
    if (fileFd < 0)
        return _buildErrorResponse(errno == ELOOP ? 403 : 404, &server, location);
    close(fileFd);

    struct stat fileStat;
    if (stat(fullPath.c_str(), &fileStat) != 0)
        return _buildErrorResponse(404, &server, location);

    if (!S_ISREG(fileStat.st_mode) || S_ISLNK(fileStat.st_mode))
        return _buildErrorResponse(403, &server, location);

    if (std::remove(fullPath.c_str()) != 0)
        return _buildErrorResponse(500, &server, location);

    return _buildResponse(204, _statusText(204), "text/plain", "");
}

bool Server::_saveRawUpload(const Request& request,
    const LocationConfig* location, std::string& savedName) const
{
    std::string relative = request.getPath();
    if (relative.compare(0, location->path.size(), location->path) == 0)
        relative.erase(0, location->path.size());
    const std::size_t slash = relative.find_last_of('/');
    savedName = _safeFileName(slash == std::string::npos
        ? relative : relative.substr(slash + 1));
    if (savedName.empty())
        savedName = "upload_" + _intToString(static_cast<long>(std::time(NULL))) + ".bin";

    std::string path = location->upload_path;
    if (!path.empty() && path[path.size() - 1] != '/')
        path += '/';
    path += savedName;

    std::ofstream file(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!file.is_open())
        return false;
    file.write(request.getBody().data(),
        static_cast<std::streamsize>(request.getBody().size()));
    return file.good();
}

bool Server::_saveMultipartUpload(const Request& request,
    const LocationConfig* location, std::vector<std::string>& savedNames) const
{
    const std::string contentType = request.getHeaderValue("content-type");
    const std::string boundaryValue =
        _extractMultipartParameter(contentType, "boundary");
    if (boundaryValue.empty())
        return false;

    const std::string boundary = "--" + boundaryValue;
    const std::string& body = request.getBody();
    std::size_t position = 0;
    bool wroteFile = false;

    while (true)
    {
        std::size_t partStart = body.find(boundary, position);
        if (partStart == std::string::npos)
            break;
        partStart += boundary.size();
        if (body.compare(partStart, 2, "--") == 0)
            break;
        if (body.compare(partStart, 2, "\r\n") == 0)
            partStart += 2;
        else if (body.compare(partStart, 1, "\n") == 0)
            partStart += 1;

        std::size_t headerEnd = body.find("\r\n\r\n", partStart);
        std::size_t separatorSize = 4;
        if (headerEnd == std::string::npos)
        {
            headerEnd = body.find("\n\n", partStart);
            separatorSize = 2;
        }
        if (headerEnd == std::string::npos)
            return false;

        const std::string headers = body.substr(partStart, headerEnd - partStart);
        std::string disposition;
        std::istringstream headerStream(headers);
        std::string line;
        while (std::getline(headerStream, line))
        {
            if (!line.empty() && line[line.size() - 1] == '\r')
                line.erase(line.size() - 1);
            const std::size_t colon = line.find(':');
            if (colon != std::string::npos &&
                _toLower(_trim(line.substr(0, colon))) == "content-disposition")
                disposition = _trim(line.substr(colon + 1));
        }

        const std::string rawFileName =
            _extractMultipartParameter(disposition, "filename");
        const std::size_t dataStart = headerEnd + separatorSize;
        std::size_t nextBoundary = body.find("\r\n" + boundary, dataStart);
        std::size_t prefixSize = 2;
        if (nextBoundary == std::string::npos)
        {
            nextBoundary = body.find("\n" + boundary, dataStart);
            prefixSize = 1;
        }
        if (nextBoundary == std::string::npos)
            return false;

        if (!rawFileName.empty())
        {
            const std::string safeName = _safeFileName(rawFileName);
            if (safeName.empty())
                return false;
            std::string path = location->upload_path;
            if (!path.empty() && path[path.size() - 1] != '/')
                path += '/';
            path += safeName;

            std::ofstream file(path.c_str(), std::ios::binary | std::ios::trunc);
            if (!file.is_open())
                return false;
            file.write(body.data() + dataStart,
                static_cast<std::streamsize>(nextBoundary - dataStart));
            if (!file.good())
                return false;
            savedNames.push_back(safeName);
            wroteFile = true;
        }
        position = nextBoundary + prefixSize;
    }
    return wroteFile;
}

std::string Server::_safeFileName(const std::string& value)
{
    std::string name = value;
    const std::size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos)
        name.erase(0, slash + 1);
    if (name.empty() || name == "." || name == "..")
        return "";
    for (std::size_t i = 0; i < name.size(); ++i)
    {
        const unsigned char c = static_cast<unsigned char>(name[i]);
        if (c < 32 || name[i] == '/' || name[i] == '\\')
            name[i] = '_';
    }
    return name;
}

std::string Server::_extractMultipartParameter(const std::string& header,
    const std::string& parameter)
{
    const std::string lower = _toLower(header);
    const std::string needle = _toLower(parameter) + "=";
    std::size_t position = lower.find(needle);
    if (position == std::string::npos)
        return "";
    position += needle.size();
    if (position >= header.size())
        return "";
    if (header[position] == '"')
    {
        const std::size_t end = header.find('"', position + 1);
        if (end == std::string::npos)
            return "";
        return header.substr(position + 1, end - position - 1);
    }
    const std::size_t end = header.find(';', position);
    return _trim(header.substr(position,
        end == std::string::npos ? std::string::npos : end - position));
}

void Server::_sendErrorResponse(int clientFd, int code,
    const ServerConfig* server, const LocationConfig* location)
{
    _queueResponse(clientFd, _buildErrorResponse(code, server, location));
}

std::string Server::_buildErrorResponse(int code, const ServerConfig* server,
    const LocationConfig* location) const
{
    return _buildResponse(code, _statusText(code), "text/html",
        _loadErrorBody(code, server, location));
}

std::string Server::_loadErrorBody(int code, const ServerConfig* server,
    const LocationConfig* location) const
{
    std::string configuredPath;
    std::string root;
    if (location != NULL)
    {
        const std::map<int, std::string>::const_iterator it =
            location->error_pages.find(code);
        if (it != location->error_pages.end())
        {
            configuredPath = it->second;
            root = location->root_directory;
        }
    }
    if (configuredPath.empty() && server != NULL)
    {
        const std::map<int, std::string>::const_iterator it =
            server->error_pages.find(code);
        if (it != server->error_pages.end())
        {
            configuredPath = it->second;
            root = server->root_directory;
        }
    }

    if (!configuredPath.empty())
    {
        if (configuredPath[0] == '/')
            configuredPath = root + configuredPath;
        std::ifstream file(configuredPath.c_str(), std::ios::binary);
        if (file.is_open())
        {
            std::ostringstream content;
            content << file.rdbuf();
            return content.str();
        }
    }
    return _defaultErrorBody(code, _statusText(code));
}

std::string Server::_buildCgiHttpResponse(const std::string& rawOutput) const
{
    int statusCode = 200;
    std::string contentType = "text/plain";
    std::map<std::string, std::string> extraHeaders;
    bool hasLocation = false;
    std::string body = rawOutput;

    std::size_t headerEnd = rawOutput.find("\r\n\r\n");
    std::size_t separatorSize = 4;
    if (headerEnd == std::string::npos)
    {
        headerEnd = rawOutput.find("\n\n");
        separatorSize = 2;
    }

    if (headerEnd != std::string::npos)
    {
        const std::string headerBlock = rawOutput.substr(0, headerEnd);
        body = rawOutput.substr(headerEnd + separatorSize);
        std::istringstream stream(headerBlock);
        std::string line;
        while (std::getline(stream, line))
        {
            if (!line.empty() && line[line.size() - 1] == '\r')
                line.erase(line.size() - 1);
            const std::size_t colon = line.find(':');
            if (colon == std::string::npos)
                continue;
            const std::string name = _trim(line.substr(0, colon));
            const std::string lowerName = _toLower(name);
            const std::string value = _trim(line.substr(colon + 1));
            if (lowerName == "status")
            {
                std::istringstream status(value);
                status >> statusCode;
            }
            else if (lowerName == "content-type")
                contentType = value;
            else if (lowerName != "content-length" &&
                     lowerName != "connection")
            {
                extraHeaders[name] = value;
                if (lowerName == "location")
                    hasLocation = true;
            }
        }
        if (statusCode == 200 && hasLocation)
            statusCode = 302;
    }

    return _buildResponse(statusCode, _statusText(statusCode), contentType,
        body, extraHeaders);
}

std::string Server::_buildResponse(int code, const std::string& statusText,
    const std::string& contentType, const std::string& body,
    const std::map<std::string, std::string>& extraHeaders) const
{
    std::ostringstream response;
    response << "HTTP/1.1 " << code << " " << statusText << "\r\n";
    response << "Content-Type: " << contentType << "\r\n";
    response << "Content-Length: " << body.size() << "\r\n";
    response << "Connection: close\r\n";
    for (std::map<std::string, std::string>::const_iterator it =
             extraHeaders.begin(); it != extraHeaders.end(); ++it)
        response << it->first << ": " << it->second << "\r\n";
    response << "\r\n" << body;
    return response.str();
}

std::string Server::_defaultErrorBody(int code,
    const std::string& statusText) const
{
    std::ostringstream body;
    body << "<!doctype html><html><head><meta charset=\"utf-8\">"
         << "<title>" << code << " " << statusText << "</title></head>"
         << "<body><h1>" << code << " " << statusText
         << "</h1></body></html>";
    return body.str();
}

std::string Server::_statusText(int code) const
{
    switch (code)
    {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 303: return "See Other";
        case 307: return "Temporary Redirect";
        case 308: return "Permanent Redirect";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 408: return "Request Timeout";
        case 411: return "Length Required";
        case 413: return "Payload Too Large";
        case 414: return "URI Too Long";
        case 431: return "Request Header Fields Too Large";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        case 504: return "Gateway Timeout";
        case 505: return "HTTP Version Not Supported";
        default: return "Error";
    }
}

std::string Server::_getMimeType(const std::string& path) const
{
    const std::size_t dot = path.find_last_of('.');
    if (dot == std::string::npos)
        return "application/octet-stream";
    const std::string extension = _toLower(path.substr(dot + 1));
    if (extension == "html" || extension == "htm") return "text/html";
    if (extension == "css") return "text/css";
    if (extension == "js") return "application/javascript";
    if (extension == "json") return "application/json";
    if (extension == "png") return "image/png";
    if (extension == "jpg" || extension == "jpeg") return "image/jpeg";
    if (extension == "gif") return "image/gif";
    if (extension == "svg") return "image/svg+xml";
    if (extension == "ico") return "image/x-icon";
    if (extension == "txt") return "text/plain";
    if (extension == "pdf") return "application/pdf";
    return "application/octet-stream";
}

std::string Server::_buildAutoindexPage(const std::string& directoryPath,
    const std::string& requestPath) const
{
    std::ostringstream page;
    page << "<!doctype html><html><head><meta charset=\"utf-8\">"
         << "<title>Index of " << requestPath << "</title></head><body>"
         << "<h1>Index of " << requestPath << "</h1><ul>";

    DIR* directory = opendir(directoryPath.c_str());
    if (directory != NULL)
    {
        struct dirent* entry;
        while ((entry = readdir(directory)) != NULL)
        {
            const std::string name = entry->d_name;
            if (name == ".")
                continue;
            std::string link = requestPath;
            if (link.empty() || link[link.size() - 1] != '/')
                link += '/';
            link += name;
            page << "<li><a href=\"" << link << "\">" << name
                 << "</a></li>";
        }
        closedir(directory);
    }
    page << "</ul></body></html>";
    return page.str();
}