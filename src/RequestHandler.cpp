#include "../includes/RequestHandler.hpp"
#include "../includes/MethodHandler.hpp"
#include "../includes/DirectoryHandler.hpp"
#include "../includes/Logger.hpp"
#include <sys/stat.h>
#include <algorithm>

const LocationConfig* RequestHandler::_findLocationForPath(const std::string& path, const ServerConfig& config)
{
	const LocationConfig* best_match = NULL;
	size_t	longest_match_len = 0;

	for (size_t i = 0; i < config.locations.size(); i++)
	{
		const LocationConfig& loc = config.locations[i];
		if (path.rfind(loc.path, 0) == 0)
		{
			if (loc.path.length() > longest_match_len)
			{
				longest_match_len = loc.path.length();
				best_match = &loc;
			}
		}
	}
	return (best_match);
}

Response RequestHandler::handle(Client& client)
{
	Request& request = client.getRequest();
	const ServerConfig& config = client.getConfig();
	Response response;

	std::stringstream ss_log;
	ss_log << "Request from 0.0.0.0:" << config.port << " - " << request.getMethod() << " " << request.getPath();
	Logger::log(TRACE, ss_log.str());

	// 1. Encontrar la location que corresponse
	const LocationConfig* location = _findLocationForPath(request.getPath(), config);

	// 2. Comprobar si hay una redireccion
	if (location && location->return_code != 0)
	{
		std::stringstream ss_log;
		ss_log << "Redirecting " << request.getPath() << " to " << location->return_url << " with status " << location->return_code;
		Logger::log(TRACE, ss_log.str());

		// Si la location tiene un return_code, es una redirección.
		response.buildRedirectResponse(location->return_code, location->return_url);
		return (response);
	}

	// 3. Si el metodo esta permitido, continuamos
	const std::vector<std::string>* methods_to_check= &config.allowed_methods;

	if (location && !location->allowed_methods.empty())
		methods_to_check = &location->allowed_methods;

	if (!methods_to_check->empty())
	{
		if (std::find(methods_to_check->begin(), methods_to_check->end(), request.getMethod()) == methods_to_check->end())
			return (response.buildErrorResponse(405, config, NULL), response);
	}

	// 4. Construir la ruta completa al recurso (full_path)
	// Es la calve para soportar 'root' en una 'location'
	std::string root_dir;
	std::string request_uri = request.getPath();

	if (location && !location->root_directory.empty())
	{
		root_dir = location->root_directory;
		// Ajusto la URI para que sea relativa al nuevo root
		// Ej. location /img/ con request /img/logo.png -> la uri a servir es /logo.png
		if (request_uri.rfind(location->path, 0) == 0)
			request_uri = request_uri.substr(location->path.length());
	}
	else
		root_dir = config.root_directory;

	// Unimos el root y la uri de forma segura para evitar "//"
	std::string full_path = root_dir;
	// Asegurarse de que el root_dir termina con una barra '/'
	if (!full_path.empty() && full_path[full_path.length() - 1] != '/')
		full_path += '/';

	// Asegurarse de que la request_uri no empieza con una barra '/'
	if (!request_uri.empty() && request_uri[0] == '/')
		request_uri.erase(0, 1);

	full_path += request_uri;

	request.setFullPath(full_path);

	// 5. Si es un directorio y tengo permisos en la config, gestiono autoindex
	if (_isDirectory(full_path) && request.getMethod() == "GET")
		return (DirectoryHandler::handle(client, location));
	else
		return (MethodHandler::handle(client, location));
}

bool	RequestHandler::_isDirectory(const std::string& path)
{
	struct stat statbuf;
	if (stat(path.c_str(), &statbuf) != 0)
		return (false);
	return (S_ISDIR(statbuf.st_mode));
}

