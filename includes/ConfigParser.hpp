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

    // Lee el archivo de configuración y devuelve todos los bloques `server`.
    std::vector<ServerConfig> parseFile(const std::string& filename);

private:
    // Estados del parser: fuera de server, dentro de server, o dentro de location.
    enum ParsingState
    {
        GLOBAL,
        SERVER,
        LOCATION
    };

    // Utilidades básicas de limpieza y tokenización.
    static std::string _trim(const std::string& str);
    static std::vector<std::string> _split(const std::string& str);

    // Controlan cómo se interpretan las líneas según el contexto actual.
    void _handleGLOBAL(ParsingState& state, const std::vector<std::string>& tokens, const std::string& line);
    void _handleSERVER(ParsingState& state, ServerConfig& current_server, LocationConfig& current_location, const std::vector<std::string>& tokens, const std::string& line, std::vector<ServerConfig>& servers);
    void _handleLOCATION(ParsingState& state, ServerConfig& current_server, LocationConfig& current_location, const std::vector<std::string>& tokens, const std::string& line);

    // Procesan directivas individuales de server y location.
    void _processServerLine(ServerConfig& server, const std::string& line);
    void _processLocationLine(LocationConfig& location, const std::string& line);

    // Parsers especializados para directivas de location.
    void _parseLocationMethods(LocationConfig& location, const std::vector<std::string>& tokens);
    void _parseLocationReturn(LocationConfig& location, const std::vector<std::string>& tokens);
    void _parseLocationBasic(LocationConfig& location, const std::vector<std::string>& tokens);

    // Conversores numéricos y validaciones de sintaxis.
    int _parsePort(const std::string& value);
    long long _parseLimit(const std::string& value);
    void _parseErrorPage(std::map<int, std::string>& error_pages, const std::string& line);
};

#endif
