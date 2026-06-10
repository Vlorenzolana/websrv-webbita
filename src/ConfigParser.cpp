#include "../includes/ConfigParser.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdlib>

ConfigParser::ConfigParser()
{
}

ConfigParser::~ConfigParser()
{
}

std::string ConfigParser::_trim(const std::string& str)
{
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
    {
        return "";
    }
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

std::vector<std::string> ConfigParser::_split(const std::string& str)
{
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    
    while (ss >> token)
    {
        tokens.push_back(token);
    }
    return tokens;
}

std::vector<ServerConfig> ConfigParser::parseFile(const std::string& filename)
{
    std::vector<ServerConfig> servers;
    std::ifstream file(filename.c_str());

    if (!file.is_open())
    {
        std::cerr << "Error: Could not open config file: " << filename << std::endl;
        std::exit(1);
    }

    std::string line;
    ParsingState state = GLOBAL;

    ServerConfig current_server;
    LocationConfig current_location;

    while (std::getline(file, line))
    {
        line = _trim(line);
        
        if (line.empty() || line[0] == '#')
        {
            continue;
        }

        std::vector<std::string> tokens = _split(line);
        if (tokens.empty())
        {
            continue;
        }

        if (state == GLOBAL)
        {
            _handleGLOBAL(state, tokens, line);
        }
        else if (state == SERVER)
        {
            _handleSERVER(state, current_server, current_location, tokens, line, servers);
        }
        else if (state == LOCATION)
        {
            _handleLOCATION(state, current_server, current_location, tokens, line);
        }
    }

    if (state != GLOBAL)
    {
        std::cerr << "Error: Unclosed curly braces at the end of file." << std::endl;
        file.close();
        std::exit(1);
    }

    file.close();
    return servers;
}

void ConfigParser::_handleGLOBAL(ParsingState& state, const std::vector<std::string>& tokens, const std::string& line)
{
    if (tokens[0] == "server" && tokens.size() == 2 && tokens[1] == "{")
    {
        state = SERVER;
    }
    else if (tokens[0] == "server" && tokens.size() == 1)
    {
        state = SERVER;
    }
    else
    {
        std::cerr << "Error: Config outside server block: " << line << std::endl;
        std::exit(1);
    }
}

void ConfigParser::_handleSERVER(ParsingState& state, ServerConfig& current_server, LocationConfig& current_location, const std::vector<std::string>& tokens, const std::string& line, std::vector<ServerConfig>& servers)
{
    if (tokens[0] == "}")
    {
        servers.push_back(current_server);
        current_server = ServerConfig();
        state = GLOBAL;
    }
    else if (tokens[0] == "location")
    {
        if (tokens.size() < 3 || tokens[tokens.size() - 1] != "{")
        {
            std::cerr << "Error: Invalid location syntax: " << line << std::endl;
            std::exit(1);
        }
        current_location = LocationConfig();
        current_location.path = tokens[1];
        state = LOCATION;
    }
    else
    {
        _processServerLine(current_server, line);
    }
}

void ConfigParser::_handleLOCATION(ParsingState& state, ServerConfig& current_server, LocationConfig& current_location, const std::vector<std::string>& tokens, const std::string& line)
{
    if (tokens[0] == "}")
    {
        current_server.locations.push_back(current_location);
        state = SERVER;
    }
    else
    {
        _processLocationLine(current_location, line);
    }
}

void ConfigParser::_processServerLine(ServerConfig& server, const std::string& line)
{
    std::vector<std::string> tokens = _split(line);
    if (tokens.empty())
    {
        return;
    }

    std::string lastToken = tokens[tokens.size() - 1];
    if (lastToken[lastToken.size() - 1] != ';')
    {
        std::cerr << "Error: Missing ';' at the end of directive: " << line << std::endl;
        std::exit(1);
    }

    if (tokens[0] == "listen" && tokens.size() == 2)
    {
        server.port = _parsePort(tokens[1]);
    }
    else if (tokens[0] == "server_name" && tokens.size() == 2)
    {
        server.server_name = tokens[1].substr(0, tokens[1].size() - 1);
    }
    else if (tokens[0] == "root" && tokens.size() == 2)
    {
        server.root_directory = tokens[1].substr(0, tokens[1].size() - 1);
    }
    else if (tokens[0] == "client_max_body_size" && tokens.size() == 2)
    {
        server.client_max_body_size = _parseLimit(tokens[1]);
    }
    else if (tokens[0] == "error_page")
    {
        _parseErrorPage(server.error_pages, line);
    }
    else
    {
        std::cerr << "Error: Unknown or invalid server directive: " << tokens[0] << std::endl;
        std::exit(1);
    }
}

void ConfigParser::_processLocationLine(LocationConfig& location, const std::string& line)
{
    std::vector<std::string> tokens = _split(line);
    if (tokens.empty())
    {
        return;
    }

    std::string lastToken = tokens[tokens.size() - 1];
    if (lastToken[lastToken.size() - 1] != ';')
    {
        std::cerr << "Error: Missing ';' at the end of directive: " << line << std::endl;
        std::exit(1);
    }

    if (tokens[0] == "allowed_methods")
    {
        _parseLocationMethods(location, tokens);
    }
    else if (tokens[0] == "return")
    {
        _parseLocationReturn(location, tokens);
    }
    else if (tokens[0] == "root" || tokens[0] == "index" || tokens[0] == "autoindex" || tokens[0] == "upload_path")
    {
        _parseLocationBasic(location, tokens);
    }
    else
    {
        std::cerr << "Error: Unknown or invalid location directive: " << tokens[0] << std::endl;
        std::exit(1);
    }
}

void ConfigParser::_parseLocationMethods(LocationConfig& location, const std::vector<std::string>& tokens)
{
    if (tokens.size() < 2)
    {
        std::cerr << "Error: Empty allowed_methods directive." << std::endl;
        std::exit(1);
    }
    
    for (size_t i = 1; i < tokens.size(); ++i)
    {
        std::string method = tokens[i];
        if (i == tokens.size() - 1)
        {
            method = method.substr(0, method.size() - 1);
        }
        
        if (method != "GET" && method != "POST" && method != "DELETE")
        {
            std::cerr << "Error: Invalid HTTP method: " << method << std::endl;
            std::exit(1);
        }
        location.allowed_methods.push_back(method);
    }
}

void ConfigParser::_parseLocationReturn(LocationConfig& location, const std::vector<std::string>& tokens)
{
    if (tokens.size() != 3)
    {
        std::cerr << "Error: Invalid return directive syntax." << std::endl;
        std::exit(1);
    }

    std::stringstream ss(tokens[1]);
    if (!(ss >> location.return_code))
    {
        std::cerr << "Error: Invalid redirection code." << std::endl;
        std::exit(1);
    }
    location.return_url = tokens[2].substr(0, tokens[2].size() - 1);
}

void ConfigParser::_parseLocationBasic(LocationConfig& location, const std::vector<std::string>& tokens)
{
    if (tokens[0] == "root" && tokens.size() == 2)
    {
        location.root_directory = tokens[1].substr(0, tokens[1].size() - 1);
    }
    else if (tokens[0] == "upload_path" && tokens.size() == 2)
    {
        location.upload_path = tokens[1].substr(0, tokens[1].size() - 1);
    }
    else if (tokens[0] == "autoindex" && tokens.size() == 2)
    {
        std::string val = tokens[1].substr(0, tokens[1].size() - 1);
        if (val == "on")
        {
            location.autoindex = true;
        }
        else if (val == "off")
        {
            location.autoindex = false;
        }
        else
        {
            std::cerr << "Error: Invalid autoindex value (use on/off): " << val << std::endl;
            std::exit(1);
        }
    }
    else if (tokens[0] == "index")
    {
        for (size_t i = 1; i < tokens.size(); ++i)
        {
            std::string idx = tokens[i];
            if (i == tokens.size() - 1)
            {
                idx = idx.substr(0, idx.size() - 1);
            }
            location.index_files.push_back(idx);
        }
    }
    else
    {
        std::cerr << "Error: Invalid arguments count for directive: " << tokens[0] << std::endl;
        std::exit(1);
    }
}

int ConfigParser::_parsePort(const std::string& value)
{
    std::string cleanValue = value;
    if (!cleanValue.empty() && cleanValue[cleanValue.size() - 1] == ';')
    {
        cleanValue = cleanValue.substr(0, cleanValue.size() - 1);
    }

    std::stringstream ss(cleanValue);
    int port;
    if (!(ss >> port) || !ss.eof() || port < 1 || port > 65535)
    {
        std::cerr << "Error: Invalid port value: " << value << std::endl;
        std::exit(1);
    }
    return port;
}

long long ConfigParser::_parseLimit(const std::string& value)
{
    std::string cleanValue = value;
    if (!cleanValue.empty() && cleanValue[cleanValue.size() - 1] == ';')
    {
        cleanValue = cleanValue.substr(0, cleanValue.size() - 1);
    }

    std::stringstream ss(cleanValue);
    long long limit;
    if (!(ss >> limit) || !ss.eof() || limit < 0)
    {
        std::cerr << "Error: Invalid client_max_body_size value: " << value << std::endl;
        std::exit(1);
    }
    return limit;
}

void ConfigParser::_parseErrorPage(std::map<int, std::string>& error_pages, const std::string& line)
{
    std::vector<std::string> tokens = _split(line);
    if (tokens.size() < 3)
    {
        std::cerr << "Error: Invalid error_page directive: " << line << std::endl;
        std::exit(1);
    }

    std::string path = tokens[tokens.size() - 1];
    if (!path.empty() && path[path.size() - 1] == ';')
    {
        path = path.substr(0, path.size() - 1);
    }

    for (size_t i = 1; i < tokens.size() - 1; ++i)
    {
        std::stringstream ss(tokens[i]);
        int code;
        if (!(ss >> code) || code < 300 || code > 599)
        {
            std::cerr << "Error: Invalid error code: " << tokens[i] << std::endl;
            std::exit(1);
        }
        error_pages[code] = path;
    }
}