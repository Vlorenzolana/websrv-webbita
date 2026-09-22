#ifndef CONFIG_PARSER_HPP
#define CONFIG_PARSER_HPP

#include "Config.hpp"
#include <map>
#include <set>
#include <string>
#include <vector>

class ConfigParser
{
public:
    ConfigParser();
    ~ConfigParser();

    // Main parsing entry point
    std::vector<ServerConfig> parseFile(const std::string& filename);

private:
    // Finite state machine parser states
    enum ParsingState
    {
        GLOBAL,
        SERVER,
        LOCATION
    };

    // String manipulation and normalization helpers
    static std::string _trim(const std::string& str);
    static std::string _normalizeLine(const std::string& str);
    static std::vector<std::string> _split(const std::string& str);
    static std::string _withoutSemicolon(const std::string& value);

    // State machine context handlers
    void _handleGlobal(ParsingState& state, const std::vector<std::string>& tokens,
        const std::string& line);
    void _handleServer(ParsingState& state, ServerConfig& currentServer,
        LocationConfig& currentLocation, const std::vector<std::string>& tokens,
        const std::string& line, std::vector<ServerConfig>& servers,
        std::set<std::string>& serverDirectives);
    void _handleLocation(ParsingState& state, ServerConfig& currentServer,
        LocationConfig& currentLocation, const std::vector<std::string>& tokens,
        const std::string& line, std::set<std::string>& locationDirectives);

    // Line dispatchers
    void _processServerLine(ServerConfig& server, const std::vector<std::string>& tokens,
        const std::string& line, std::set<std::string>& serverDirectives);
    void _processLocationLine(LocationConfig& location, const std::vector<std::string>& tokens,
        const std::string& line, std::set<std::string>& locationDirectives);

    // Directive parsers
    void _parseListen(ServerConfig& server, const std::string& value);
    void _parseLocationMethods(LocationConfig& location,
        const std::vector<std::string>& tokens);
    void _parseLocationReturn(LocationConfig& location,
        const std::vector<std::string>& tokens);
    void _parseLocationBasic(LocationConfig& location,
        const std::vector<std::string>& tokens);
    void _parseCgiExtension(LocationConfig& location,
        const std::vector<std::string>& tokens);

    // Value converters and validators
    int _parsePort(const std::string& value);
    long long _parseLimit(const std::string& value);
    void _parseErrorPage(std::map<int, std::string>& errorPages,
        const std::vector<std::string>& tokens, const std::string& line);
};

#endif