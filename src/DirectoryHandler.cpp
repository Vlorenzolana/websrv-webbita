#include "../includes/DirectoryHandler.hpp"

Response DirectoryHandler::handle(Client& client, const LocationConfig* location)
{
	const Request& request = client.getRequest();
	const ServerConfig& config = client.getConfig();

	std::string full_path = config.root_directory + request.getPath();
	Response response;

	if (full_path[full_path.length() - 1] != '/')
	{
		response.buildErrorResponse(301, config, location); // 301 Moved Permanently
		return (response);
	}

	// 1. Iterar sobre la lista de archivos index definidos en la config
	for (size_t i = 0; i < location->index_files.size(); ++i)
	{
		std::string index_path = full_path + "/" + location->index_files[i];
		if (access(index_path.c_str(), F_OK) == 0)
		{
			response.buildCustomResponse("200", "OK", _readFile(index_path));
			response.addHeader("Content-Type", "text/html");
			return (response);
		}
	}

	// 2. Si no se encontró ningún archivo index, comprobar autoindex
	bool	autoindex_on = config.autoindex;
	if (location)
		autoindex_on = location->autoindex;

	if (autoindex_on)
	{
		Logger::log(TRACE, "Autoindex on for " + request.getPath() + ". Generating directory listing.");
		return (_generateAutoindexPage(request.getPath(), full_path));
	}

	response.buildErrorResponse(403, config, location); // 403 Forbidden
	return (response);
}

// --- Implementacion de los metodos privados ---

Response DirectoryHandler::_generateAutoindexPage(const std::string& path, const std::string& directory)
{
	Response response;
	std::stringstream body_stream;

	body_stream << "<html><head><title>Index of " << path << "</title></head>";
	body_stream << "<body><h1>Index of " << path << "</h1><hr><ul>";

	DIR* dir = opendir(directory.c_str()); // La funcion opendir funcioan con un tipo de dato DIR
	if (dir != NULL)
	{
		struct dirent* entry;
		while ((entry = readdir(dir)) != NULL) // Lo mismo, readdir funciona con una estructura dirent
		{
			std::string name = entry->d_name;

			// Construir la ruta completa en el sistema de archivos para poder verificar si es un directorio
 			std::string full_fs_path = directory;
			if (full_fs_path[full_fs_path.length() - 1] != '/')
				full_fs_path += '/';
			full_fs_path += name;

			// Preparar el nombre del enlace y el texto a mostrar
			std::string display_name = name;
			std::string href_path = path;
			if (href_path[href_path.length() - 1] != '/')
				href_path += '/';
			href_path += name;

			// Si es un directorio (y no es el directorio actual "."), añadimos la barra al final
			if (_isDirectory(full_fs_path) && name != ".")
			{
				display_name += "/";
				href_path += "/";
			}
			body_stream << "<li><a href=\"" << href_path << "\">" << display_name << "</a></li>";
		}
		closedir(dir);
	}
	body_stream << "</ul><hr></body></html>";

	response.buildCustomResponse("200", "OK", body_stream.str()); // Crea un response con el body que se ha ido creando
	return (response);
}

// Funcion para saber si es un directorio
bool	DirectoryHandler::_isDirectory(const std::string& path)
{
	struct stat statbuf;
	if (stat(path.c_str(), &statbuf) != 0)
		return (false);
	return (S_ISDIR(statbuf.st_mode)); // Devuelve true si es un directorio
}

// Funciona para leer un archivo
std::string DirectoryHandler::_readFile(const std::string& path)
{
	std::ifstream file(path.c_str());
	if (!file.is_open())
		return ("");
	std::stringstream buffer;
	buffer << file.rdbuf();
	return (buffer.str());
}
