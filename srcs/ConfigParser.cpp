#include "../includes/ConfigParser.hpp"
#include "../includes/ConfigValidator.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <set>

ConfigParser::ConfigParser() {}
ConfigParser::~ConfigParser() {}

// Removes leading and trailing whitespace and newline characters
std::string ConfigParser::_trim(const std::string& str)
{
    const std::size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return "";
    const std::size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}

// Pads structural delimiters ({, }, ;) with spaces to simplify tokenization
std::string ConfigParser::_normalizeLine(const std::string& str)
{
    std::string result;
    for (std::size_t i = 0; i < str.size(); ++i)
    {
        if (str[i] == '{' || str[i] == '}' || str[i] == ';')
        {
            result += ' ';
            result += str[i];
            result += ' ';
        }
        else
        {
            result += str[i];
        }
    }
    return result;
}

// Splits a normalized string into individual tokens by whitespace
std::vector<std::string> ConfigParser::_split(const std::string& str)
{
    std::vector<std::string> tokens;
    std::stringstream stream(str);
    std::string token;
    while (stream >> token)
        tokens.push_back(token);
    return tokens;
}

// Strips a trailing semicolon from a token if present
std::string ConfigParser::_withoutSemicolon(const std::string& value)
{
    if (!value.empty() && value[value.size() - 1] == ';')
        return value.substr(0, value.size() - 1);
    return value;
}

// Reads and parses the configuration file line by line using an FSM
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

    std::set<std::string> serverDirectives;
    std::set<std::string> locationDirectives;

    while (std::getline(file, line))
    {
        // Strip out comments
        const std::size_t comment = line.find('#');
        if (comment != std::string::npos)
            line.erase(comment);
        line = _trim(line);
        if (line.empty())
            continue;

        const std::string normalized = _normalizeLine(line);
        const std::vector<std::string> tokens = _split(normalized);
        if (tokens.empty())
            continue;

        if (state == GLOBAL)
        {
            _handleGlobal(state, tokens, line);
            serverDirectives.clear();
        }
        else if (state == SERVER)
        {
            _handleServer(state, currentServer, currentLocation, tokens, line, servers, serverDirectives);
        }
        else
        {
            _handleLocation(state, currentServer, currentLocation, tokens, line, locationDirectives);
        }
    }

    if (state != GLOBAL)
        throw std::runtime_error("Unclosed curly brace at end of config file");

    // Semantic validation and post-processing
    ConfigValidator validator;
    validator.validateAndNormalize(servers);
    return servers;
}

// Handles transitions from the global context into a server block
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

// Processes server-level directives and transitions to location or global state
void ConfigParser::_handleServer(ParsingState& state, ServerConfig& currentServer,
    LocationConfig& currentLocation, const std::vector<std::string>& tokens,
    const std::string& line, std::vector<ServerConfig>& servers,
    std::set<std::string>& serverDirectives)
{
    if (tokens[0] == "}")
    {
        if (tokens.size() != 1)
            throw std::runtime_error("Unexpected token after server closing brace: " + line);
        servers.push_back(currentServer);
        currentServer = ServerConfig();
        serverDirectives.clear();
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

    _processServerLine(currentServer, tokens, line, serverDirectives);
}

// Processes location-level directives and transitions back to server state on closing brace
void ConfigParser::_handleLocation(ParsingState& state, ServerConfig& currentServer,
    LocationConfig& currentLocation, const std::vector<std::string>& tokens,
    const std::string& line, std::set<std::string>& locationDirectives)
{
    if (tokens[0] == "}")
    {
        if (tokens.size() != 1)
            throw std::runtime_error("Unexpected token after location closing brace: " + line);
        currentServer.locations.push_back(currentLocation);
        currentLocation = LocationConfig();
        locationDirectives.clear();
        state = SERVER;
        return;
    }
    _processLocationLine(currentLocation, tokens, line, locationDirectives);
}

// Validates syntax and assigns server-level directives
void ConfigParser::_processServerLine(ServerConfig& server,
    const std::vector<std::string>& tokens, const std::string& line,
    std::set<std::string>& serverDirectives)
{
    if (tokens.empty())
        return;

    if (tokens.back() != ";")
        throw std::runtime_error("Missing ';' at end of directive: " + line);

    std::vector<std::string> cleanTokens;
    for (std::size_t i = 0; i < tokens.size() - 1; ++i)
        cleanTokens.push_back(tokens[i]);

    if (cleanTokens.empty())
        throw std::runtime_error("Empty directive: " + line);

    const std::string& directive = cleanTokens[0];

    // Prevent duplicate configuration keys in the same block
    if (directive != "error_page" && serverDirectives.count(directive))
        throw std::runtime_error("Duplicate directive '" + directive + "' in server block");

    if (directive == "listen" && cleanTokens.size() == 2)
    {
        _parseListen(server, cleanTokens[1]);
        serverDirectives.insert("listen");
    }
    else if (directive == "server_name" && cleanTokens.size() == 2)
    {
        server.server_name = cleanTokens[1];
        serverDirectives.insert("server_name");
    }
    else if (directive == "root" && cleanTokens.size() == 2)
    {
        server.root_directory = cleanTokens[1];
        serverDirectives.insert("root");
    }
    else if (directive == "client_max_body_size" && cleanTokens.size() == 2)
    {
        server.client_max_body_size = _parseLimit(cleanTokens[1]);
        serverDirectives.insert("client_max_body_size");
    }
    else if (directive == "error_page")
    {
        _parseErrorPage(server.error_pages, cleanTokens, line);
    }
    else
    {
        throw std::runtime_error("Unknown or invalid server directive: " + line);
    }
}

// Validates syntax and assigns route/location-level directives
void ConfigParser::_processLocationLine(LocationConfig& location,
    const std::vector<std::string>& tokens, const std::string& line,
    std::set<std::string>& locationDirectives)
{
    if (tokens.empty())
        return;

    if (tokens.back() != ";")
        throw std::runtime_error("Missing ';' at end of directive: " + line);

    std::vector<std::string> cleanTokens;
    for (std::size_t i = 0; i < tokens.size() - 1; ++i)
        cleanTokens.push_back(tokens[i]);

    if (cleanTokens.empty())
        throw std::runtime_error("Empty directive: " + line);

    const std::string& directive = cleanTokens[0];

    if (directive != "cgi_extension" && directive != "error_page" && locationDirectives.count(directive))
        throw std::runtime_error("Duplicate directive '" + directive + "' in location block");

    if (directive == "allowed_methods")
    {
        _parseLocationMethods(location, cleanTokens);
        locationDirectives.insert("allowed_methods");
    }
    else if (directive == "return")
    {
        _parseLocationReturn(location, cleanTokens);
        locationDirectives.insert("return");
    }
    else if (directive == "root" || directive == "index" ||
             directive == "autoindex" || directive == "upload_path")
    {
        _parseLocationBasic(location, cleanTokens);
        locationDirectives.insert(directive);
    }
    else if (directive == "error_page")
    {
        _parseErrorPage(location.error_pages, cleanTokens, line);
    }
    else if (directive == "cgi_extension")
    {
        _parseCgiExtension(location, cleanTokens);
    }
    else
    {
        throw std::runtime_error("Unknown or invalid location directive: " + line);
    }
}

// Parses host and port configurations (e.g., "127.0.0.1:8080" or "8080")
void ConfigParser::_parseListen(ServerConfig& server, const std::string& value)
{
    const std::size_t colon = value.rfind(':');
    if (colon == std::string::npos)
    {
        server.port = _parsePort(value);
        return;
    }

    const std::string host = value.substr(0, colon);
    const std::string port = value.substr(colon + 1);
    if (host.empty() || port.empty())
        throw std::runtime_error("Invalid listen directive: " + value);
    server.host = host;
    server.port = _parsePort(port);
}

// Parses allowed HTTP methods list and rejects unsupported or duplicate verbs
void ConfigParser::_parseLocationMethods(LocationConfig& location,
    const std::vector<std::string>& tokens)
{
    if (tokens.size() < 2)
        throw std::runtime_error("Empty allowed_methods directive");

    std::set<std::string> seen;
    for (std::size_t i = 1; i < tokens.size(); ++i)
    {
        const std::string& method = tokens[i];
        if (method != "GET" && method != "POST" && method != "DELETE")
            throw std::runtime_error("Invalid HTTP method: " + method);
        if (seen.count(method))
            throw std::runtime_error("Duplicate method in allowed_methods: " + method);
        seen.insert(method);
        location.allowed_methods.push_back(method);
    }
}

// Parses HTTP redirect directive (status code and target URL)
void ConfigParser::_parseLocationReturn(LocationConfig& location,
    const std::vector<std::string>& tokens)
{
    if (tokens.size() != 3)
        throw std::runtime_error("Invalid return directive syntax");

    for (std::size_t i = 0; i < tokens[1].size(); ++i)
    {
        if (tokens[1][i] < '0' || tokens[1][i] > '9')
            throw std::runtime_error("Invalid return status code: " + tokens[1]);
    }

    std::stringstream stream(tokens[1]);
    if (!(stream >> location.return_code) || !stream.eof())
        throw std::runtime_error("Invalid return status code: " + tokens[1]);
    location.return_url = tokens[2];
}

// Parses basic single or multi-value location options (root, autoindex, upload, index)
void ConfigParser::_parseLocationBasic(LocationConfig& location,
    const std::vector<std::string>& tokens)
{
    if (tokens[0] == "root" && tokens.size() == 2)
        location.root_directory = tokens[1];
    else if (tokens[0] == "upload_path" && tokens.size() == 2)
        location.upload_path = tokens[1];
    else if (tokens[0] == "autoindex" && tokens.size() == 2)
    {
        const std::string& value = tokens[1];
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
            location.index_files.push_back(tokens[i]);
    }
    else
        throw std::runtime_error("Invalid location directive syntax: " + tokens[0]);
}

// Parses CGI extension and corresponding binary interpreter path
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

    const std::string& interpreter = tokens[2];
    if (interpreter.empty())
        throw std::runtime_error("Empty CGI interpreter path");
    location.cgi_interpreters[extension] = interpreter;
}

// Converts a string port to integer and validates the valid range (1..65535)
int ConfigParser::_parsePort(const std::string& value)
{
    if (value.empty())
        throw std::runtime_error("Empty port value");

    for (std::size_t i = 0; i < value.size(); ++i)
    {
        if (value[i] < '0' || value[i] > '9')
            throw std::runtime_error("Invalid port value (non-digit): " + value);
    }

    std::stringstream stream(value);
    int port = 0;
    if (!(stream >> port) || !stream.eof() || port < 1 || port > 65535)
        throw std::runtime_error("Invalid port range (1-65535): " + value);
    return port;
}

// Parses numeric client max body size supporting standard suffixes ('k', 'm')
long long ConfigParser::_parseLimit(const std::string& value)
{
    std::string clean = value;
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

    if (clean.empty())
        throw std::runtime_error("Invalid client_max_body_size: " + value);

    for (std::size_t i = 0; i < clean.size(); ++i)
    {
        if (clean[i] < '0' || clean[i] > '9')
            throw std::runtime_error("Invalid client_max_body_size (non-digit): " + value);
    }

    std::stringstream stream(clean);
    long long limit = 0;
    if (!(stream >> limit) || !stream.eof() || limit < 0)
        throw std::runtime_error("Invalid client_max_body_size: " + value);
    if (limit > 2147483647LL / multiplier)
        throw std::runtime_error("client_max_body_size is too large: " + value);
    return limit * multiplier;
}

// Maps custom error page paths for one or more HTTP error status codes
void ConfigParser::_parseErrorPage(std::map<int, std::string>& errorPages,
    const std::vector<std::string>& tokens, const std::string& line)
{
    if (tokens.size() < 3)
        throw std::runtime_error("Invalid error_page directive: " + line);

    const std::string& path = tokens[tokens.size() - 1];
    for (std::size_t i = 1; i < tokens.size() - 1; ++i)
    {
        for (std::size_t j = 0; j < tokens[i].size(); ++j)
        {
            if (tokens[i][j] < '0' || tokens[i][j] > '9')
                throw std::runtime_error("Invalid error status code: " + tokens[i]);
        }

        std::stringstream stream(tokens[i]);
        int code = 0;
        if (!(stream >> code) || !stream.eof() || code < 300 || code > 599)
            throw std::runtime_error("Invalid error status code: " + tokens[i]);
        
        if (errorPages.find(code) != errorPages.end())
            throw std::runtime_error("Duplicate error_page status code: " + tokens[i]);

        errorPages[code] = path;
    }
}