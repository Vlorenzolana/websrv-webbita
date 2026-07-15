#ifndef CONFIG_VALIDATOR_HPP
#define CONFIG_VALIDATOR_HPP

#include "Config.hpp"
#include <vector>
#include <map>
#include <string>

class ConfigValidator
{
public:
    ConfigValidator();
    ~ConfigValidator();

    // Valida la semántica de todos los servers y completa valores por defecto.
    void validateAndNormalize(std::vector<ServerConfig>& servers);

private:
    // Subvalidaciones internas separadas por responsabilidad.
    void _hydrateAndCheckServer(ServerConfig& server);
    void _checkDuplicateServers(const std::vector<ServerConfig>& servers, size_t currentIndex);
    void _validateAndNormalizeLocations(ServerConfig& server);
    void _validateLocationRedirection(const LocationConfig& loc);
};

#endif