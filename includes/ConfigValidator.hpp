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

    // Main entry point for semantic validation
    void validateAndNormalize(std::vector<ServerConfig>& servers);

private:
    // Modular sub-validations
    void _hydrateAndCheckServer(ServerConfig& server);
    void _checkDuplicateServers(const std::vector<ServerConfig>& servers, size_t currentIndex);
    void _validateAndNormalizeLocations(ServerConfig& server);
    void _validateLocationRedirection(const LocationConfig& loc);
};

#endif