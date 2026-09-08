#ifndef CONFIG_VALIDATOR_HPP
#define CONFIG_VALIDATOR_HPP

#include "Config.hpp"
#include <cstddef>
#include <vector>

class ConfigValidator
{
public:
    // Orthodox Canonical Form
    ConfigValidator();
    ~ConfigValidator();

    // Semantic validation entry point
    void validateAndNormalize(std::vector<ServerConfig>& servers);

private:
    // Server and location level validators
    void _hydrateAndCheckServer(ServerConfig& server);
    void _checkDuplicateServers(const std::vector<ServerConfig>& servers,
        std::size_t currentIndex);
    void _validateAndNormalizeLocations(ServerConfig& server);
    void _validateLocationRedirection(const LocationConfig& location);
    void _validateErrorPages(const std::map<int, std::string>& errorPages,
        const std::string& rootDirectory);
};

#endif