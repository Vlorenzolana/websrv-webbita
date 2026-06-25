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

	// Map to array
	char **_mapToEnvp();

public:
	CGIHandler(const std::string& scriptPath, const std::string& interpreterPath);
	~CGIHandler();

	int execute(const Request& request, const std::string& uploadPath);
};

#endif