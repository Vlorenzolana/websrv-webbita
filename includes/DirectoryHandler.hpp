#ifndef DIRECTORYHANDLER_HPP
#define DIRECTORYHANDLER_HPP

#include "Response.hpp"
#include "Request.hpp"
#include "Config.hpp"
#include "Client.hpp"
#include "Logger.hpp"
#include <fstream>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

class DirectoryHandler
{
	private:
		static bool	_isDirectory(const std::string& path);
		static Response	_generateAutoindexPage(const std::string& path, const std::string& directory);
		static std::string _readFile(const std::string& path);

	public:
		static Response handle(Client& client, const LocationConfig* location);

};

#endif // DIRECTORYHANDLER_HPP
