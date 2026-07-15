#ifndef CGI_HANDLER_HPP
#define CGI_HANDLER_HPP

#include "Request.hpp"
#include "Config.hpp"
#include <string>
#include <map>

class CGIHandler
{
private:
	std::string _script_path;
	std::string _interpreter_path;
	std::map<std::string, std::string> _env_map;

	// Convierte el mapa de variables de entorno en un array tipo `char**`
	// para poder pasarlo a `execve()`.
	char **_mapToEnvp();

public:
	CGIHandler(const std::string& scriptPath, const std::string& interpreterPath);
	~CGIHandler();

	int execute(const Request& request, const std::string& uploadPath);
};

#endif