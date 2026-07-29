#ifndef CONFIG_PARSER_HPP
#define CONFIG_PARSER_HPP

#include "Config.hpp"
#include <map>
#include <string>
#include <vector>

class ConfigParser
{
public:
    ConfigParser();
    ~ConfigParser();

    std::vector<ServerConfig> parseFile(const std::string& filename);

private:
    enum ParsingState
    {
        GLOBAL,
        SERVER,
        LOCATION
    };

    static std::string _trim(const std::string& str);
    static std::vector<std::string> _split(const std::string& str);
    static std::string _withoutSemicolon(const std::string& value);

    void _handleGlobal(ParsingState& state, const std::vector<std::string>& tokens,
        const std::string& line);
    void _handleServer(ParsingState& state, ServerConfig& currentServer,
        LocationConfig& currentLocation, const std::vector<std::string>& tokens,
        const std::string& line, std::vector<ServerConfig>& servers);
    void _handleLocation(ParsingState& state, ServerConfig& currentServer,
        LocationConfig& currentLocation, const std::vector<std::string>& tokens,
        const std::string& line);

    void _processServerLine(ServerConfig& server, const std::string& line);
    void _processLocationLine(LocationConfig& location, const std::string& line);

    void _parseListen(ServerConfig& server, const std::string& value);
    void _parseLocationMethods(LocationConfig& location,
        const std::vector<std::string>& tokens);
    void _parseLocationReturn(LocationConfig& location,
        const std::vector<std::string>& tokens);
    void _parseLocationBasic(LocationConfig& location,
        const std::vector<std::string>& tokens);
    void _parseCgiExtension(LocationConfig& location,
        const std::vector<std::string>& tokens);

    int _parsePort(const std::string& value);
    long long _parseLimit(const std::string& value);
    void _parseErrorPage(std::map<int, std::string>& errorPages,
        const std::string& line);
};

#endif
