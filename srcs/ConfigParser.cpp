#include "../includes/ConfigParser.hpp"
#include "../includes/ConfigValidator.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

ConfigParser::ConfigParser() {}
ConfigParser::~ConfigParser() {}

std::string ConfigParser::_trim(const std::string& str)
{
    const std::size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return "";
    const std::size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}

std::vector<std::string> ConfigParser::_split(const std::string& str)
{
    std::vector<std::string> tokens;
    std::stringstream stream(str);
    std::string token;
    while (stream >> token)
        tokens.push_back(token);
    return tokens;
}

std::string ConfigParser::_withoutSemicolon(const std::string& value)
{
    if (!value.empty() && value[value.size() - 1] == ';')
        return value.substr(0, value.size() - 1);
    return value;
}

std::vector<ServerConfig> ConfigParser::parseFile(const std::string& filename)
{
    std::ifstream file(filename.c_str());
    if (!file.is_open())
        throw std::runtime_error("Could not open config file: " + filename);

    std::vector<ServerConfig> servers;
    ServerConfig currentServer;
    LocationConfig currentLocation;
    ParsingState state = GLOBAL;
    std::string line;

    while (std::getline(file, line))
    {
        const std::size_t comment = line.find('#');
        if (comment != std::string::npos)
            line.erase(comment);
        line = _trim(line);
        if (line.empty())
            continue;

        const std::vector<std::string> tokens = _split(line);
        if (tokens.empty())
            continue;

        if (state == GLOBAL)
            _handleGlobal(state, tokens, line);
        else if (state == SERVER)
            _handleServer(state, currentServer, currentLocation, tokens, line, servers);
        else
            _handleLocation(state, currentServer, currentLocation, tokens, line);
    }

    if (state != GLOBAL)
        throw std::runtime_error("Unclosed curly brace at end of config file");

    ConfigValidator validator;
    validator.validateAndNormalize(servers);
    return servers;
}

void ConfigParser::_handleGlobal(ParsingState& state,
    const std::vector<std::string>& tokens, const std::string& line)
{
    if ((tokens.size() == 2 && tokens[0] == "server" && tokens[1] == "{") ||
        (tokens.size() == 1 && tokens[0] == "server"))
    {
        state = SERVER;
        return;
    }
    throw std::runtime_error("Config outside server block: " + line);
}

void ConfigParser::_handleServer(ParsingState& state, ServerConfig& currentServer,
    LocationConfig& currentLocation, const std::vector<std::string>& tokens,
    const std::string& line, std::vector<ServerConfig>& servers)
{
    if (tokens[0] == "}")
    {
        if (tokens.size() != 1)
            throw std::runtime_error("Unexpected token after server closing brace: " + line);
        servers.push_back(currentServer);
        currentServer = ServerConfig();
        state = GLOBAL;
        return;
    }

    if (tokens[0] == "location")
    {
        if (tokens.size() != 3 || tokens[2] != "{")
            throw std::runtime_error("Invalid location syntax: " + line);
        currentLocation = LocationConfig();
        currentLocation.path = tokens[1];
        state = LOCATION;
        return;
    }

    _processServerLine(currentServer, line);
}

void ConfigParser::_handleLocation(ParsingState& state, ServerConfig& currentServer,
    LocationConfig& currentLocation, const std::vector<std::string>& tokens,
    const std::string& line)
{
    if (tokens[0] == "}")
    {
        if (tokens.size() != 1)
            throw std::runtime_error("Unexpected token after location closing brace: " + line);
        currentServer.locations.push_back(currentLocation);
        currentLocation = LocationConfig();
        state = SERVER;
        return;
    }
    _processLocationLine(currentLocation, line);
}

void ConfigParser::_processServerLine(ServerConfig& server,
    const std::string& line)
{
    const std::vector<std::string> tokens = _split(line);
    if (tokens.empty())
        return;
    if (tokens[tokens.size() - 1].empty() ||
        tokens[tokens.size() - 1][tokens[tokens.size() - 1].size() - 1] != ';')
        throw std::runtime_error("Missing ';' at end of directive: " + line);

    if (tokens[0] == "listen" && tokens.size() == 2)
        _parseListen(server, tokens[1]);
    else if (tokens[0] == "server_name" && tokens.size() == 2)
        server.server_name = _withoutSemicolon(tokens[1]);
    else if (tokens[0] == "root" && tokens.size() == 2)
        server.root_directory = _withoutSemicolon(tokens[1]);
    else if (tokens[0] == "client_max_body_size" && tokens.size() == 2)
        server.client_max_body_size = _parseLimit(tokens[1]);
    else if (tokens[0] == "error_page")
        _parseErrorPage(server.error_pages, line);
    else
        throw std::runtime_error("Unknown or invalid server directive: " + tokens[0]);
}

void ConfigParser::_processLocationLine(LocationConfig& location,
    const std::string& line)
{
    const std::vector<std::string> tokens = _split(line);
    if (tokens.empty())
        return;
    if (tokens[tokens.size() - 1].empty() ||
        tokens[tokens.size() - 1][tokens[tokens.size() - 1].size() - 1] != ';')
        throw std::runtime_error("Missing ';' at end of directive: " + line);

    if (tokens[0] == "allowed_methods")
        _parseLocationMethods(location, tokens);
    else if (tokens[0] == "return")
        _parseLocationReturn(location, tokens);
    else if (tokens[0] == "root" || tokens[0] == "index" ||
             tokens[0] == "autoindex" || tokens[0] == "upload_path")
        _parseLocationBasic(location, tokens);
    else if (tokens[0] == "error_page")
        _parseErrorPage(location.error_pages, line);
    else if (tokens[0] == "cgi_extension")
        _parseCgiExtension(location, tokens);
    else
        throw std::runtime_error("Unknown or invalid location directive: " + tokens[0]);
}

void ConfigParser::_parseListen(ServerConfig& server, const std::string& value)
{
    const std::string clean = _withoutSemicolon(value);
    const std::size_t colon = clean.rfind(':');
    if (colon == std::string::npos)
    {
        server.port = _parsePort(clean);
        return;
    }

    const std::string host = clean.substr(0, colon);
    const std::string port = clean.substr(colon + 1);
    if (host.empty() || port.empty())
        throw std::runtime_error("Invalid listen directive: " + value);
    server.host = host;
    server.port = _parsePort(port);
}

void ConfigParser::_parseLocationMethods(LocationConfig& location,
    const std::vector<std::string>& tokens)
{
    if (tokens.size() < 2)
        throw std::runtime_error("Empty allowed_methods directive");

    for (std::size_t i = 1; i < tokens.size(); ++i)
    {
        const std::string method = _withoutSemicolon(tokens[i]);
        if (method != "GET" && method != "POST" && method != "DELETE")
            throw std::runtime_error("Invalid HTTP method: " + method);
        location.allowed_methods.push_back(method);
    }
}

void ConfigParser::_parseLocationReturn(LocationConfig& location,
    const std::vector<std::string>& tokens)
{
    if (tokens.size() != 3)
        throw std::runtime_error("Invalid return directive syntax");
    std::stringstream stream(tokens[1]);
    if (!(stream >> location.return_code) || !stream.eof())
        throw std::runtime_error("Invalid return status code: " + tokens[1]);
    location.return_url = _withoutSemicolon(tokens[2]);
}

void ConfigParser::_parseLocationBasic(LocationConfig& location,
    const std::vector<std::string>& tokens)
{
    if (tokens[0] == "root" && tokens.size() == 2)
        location.root_directory = _withoutSemicolon(tokens[1]);
    else if (tokens[0] == "upload_path" && tokens.size() == 2)
        location.upload_path = _withoutSemicolon(tokens[1]);
    else if (tokens[0] == "autoindex" && tokens.size() == 2)
    {
        const std::string value = _withoutSemicolon(tokens[1]);
        if (value == "on")
            location.autoindex = true;
        else if (value == "off")
            location.autoindex = false;
        else
            throw std::runtime_error("autoindex must be 'on' or 'off'");
    }
    else if (tokens[0] == "index" && tokens.size() >= 2)
    {
        for (std::size_t i = 1; i < tokens.size(); ++i)
            location.index_files.push_back(_withoutSemicolon(tokens[i]));
    }
    else
        throw std::runtime_error("Invalid location directive syntax: " + tokens[0]);
}

void ConfigParser::_parseCgiExtension(LocationConfig& location,
    const std::vector<std::string>& tokens)
{
    if (tokens.size() != 3)
        throw std::runtime_error("cgi_extension syntax: cgi_extension .ext /path/interpreter;");

    std::string extension = tokens[1];
    if (extension.empty())
        throw std::runtime_error("Empty CGI extension");
    if (extension[0] != '.')
        extension = "." + extension;

    const std::string interpreter = _withoutSemicolon(tokens[2]);
    if (interpreter.empty())
        throw std::runtime_error("Empty CGI interpreter path");
    location.cgi_interpreters[extension] = interpreter;
}

int ConfigParser::_parsePort(const std::string& value)
{
    std::stringstream stream(_withoutSemicolon(value));
    int port = 0;
    if (!(stream >> port) || !stream.eof() || port < 1 || port > 65535)
        throw std::runtime_error("Invalid port value: " + value);
    return port;
}

long long ConfigParser::_parseLimit(const std::string& value)
{
    std::string clean = _withoutSemicolon(value);
    if (clean.empty())
        throw std::runtime_error("Invalid client_max_body_size: " + value);

    long long multiplier = 1;
    const char suffix = clean[clean.size() - 1];
    if (suffix == 'k' || suffix == 'K')
    {
        multiplier = 1024;
        clean.erase(clean.size() - 1);
    }
    else if (suffix == 'm' || suffix == 'M')
    {
        multiplier = 1024 * 1024;
        clean.erase(clean.size() - 1);
    }

    std::stringstream stream(clean);
    long long limit = 0;
    if (!(stream >> limit) || !stream.eof() || limit < 0)
        throw std::runtime_error("Invalid client_max_body_size: " + value);
    if (limit > 2147483647LL / multiplier)
        throw std::runtime_error("client_max_body_size is too large: " + value);
    return limit * multiplier;
}

void ConfigParser::_parseErrorPage(std::map<int, std::string>& errorPages,
    const std::string& line)
{
    const std::vector<std::string> tokens = _split(line);
    if (tokens.size() < 3)
        throw std::runtime_error("Invalid error_page directive: " + line);

    const std::string path = _withoutSemicolon(tokens[tokens.size() - 1]);
    for (std::size_t i = 1; i + 1 < tokens.size(); ++i)
    {
        std::stringstream stream(tokens[i]);
        int code = 0;
        if (!(stream >> code) || !stream.eof() || code < 300 || code > 599)
            throw std::runtime_error("Invalid error status code: " + tokens[i]);
        errorPages[code] = path;
    }
}
