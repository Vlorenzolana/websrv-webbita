#ifndef CONFIG_PARSER_HPP
#define CONFIG_PARSER_HPP

#include "Config.hpp"
#include <string>
#include <vector>
#include <map>

class ConfigParser
{
public:
    ConfigParser();
    ~ConfigParser();

    std::vector<ServerConfig> parseFile(const std::string& filename);

private:
    // Core engine for the parsing state machine
    enum ParsingState
    {
        GLOBAL,
        SERVER,
        LOCATION
    };

    // Pure static utilities for string manipulation
    static std::string _trim(const std::string& str);
    static std::vector<std::string> _split(const std::string& str);

    // Main context/scope managers for the state machine
    void _handleGLOBAL(ParsingState& state, const std::vector<std::string>& tokens, const std::string& line);
    void _handleSERVER(ParsingState& state, ServerConfig& current_server, LocationConfig& current_location, const std::vector<std::string>& tokens, const std::string& line, std::vector<ServerConfig>& servers);
    void _handleLOCATION(ParsingState& state, ServerConfig& current_server, LocationConfig& current_location, const std::vector<std::string>& tokens, const std::string& line);

    // Internal directive token processors
    void _processServerLine(ServerConfig& server, const std::string& line);
    void _processLocationLine(LocationConfig& location, const std::string& line);

    // Context-specific modular location parsers
    void _parseLocationMethods(LocationConfig& location, const std::vector<std::string>& tokens);
    void _parseLocationReturn(LocationConfig& location, const std::vector<std::string>& tokens);
    void _parseLocationBasic(LocationConfig& location, const std::vector<std::string>& tokens);

    // Type converters and semantic validators
    int _parsePort(const std::string& value);
    long long _parseLimit(const std::string& value);
    void _parseErrorPage(std::map<int, std::string>& error_pages, const std::string& line);
};

#endif