#ifndef REQUESTHANDLER_HPP
#define REQUESTHANDLER_HPP

#include "Request.hpp"
#include "Response.hpp"
#include "Config.hpp"
#include "Client.hpp"
#include <fstream>
#include <unistd.h>
#include <dirent.h>
#include <vector>
#include <sys/stat.h>

class RequestHandler
{
	private:
		static bool _isDirectory(const std::string& path);
		static const LocationConfig* _findLocationForPath(const std::string& path, const ServerConfig& config);

	public:
		static Response handle(Client& client);
		
};

#endif // REQUESTHANDLER_HPP
