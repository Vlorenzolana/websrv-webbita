
#include "../includes/Server.hpp"
#include "../includes/Request.hpp"
#include "../includes/CGIHandler.hpp"
#include <iostream>
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <ctime>
#include <string>
#include <fstream>
#include <sstream>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>

Server::Server(const ServerConfig& config)
	: _serverFd(-1), _epollFd(-1), _port(config.port), _config(config) {}

Server::~Server()
{
	if (_serverFd >= 0) close(_serverFd);
	if (_epollFd >= 0) close(_epollFd);
}

void Server::init()
{
	int opt = 1;
	sockaddr_in addr;

	_serverFd = socket(AF_INET, SOCK_STREAM, 0);
	setsockopt(_serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	fcntl(_serverFd, F_SETFL, O_NONBLOCK);

	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(_port);

	bind(_serverFd, (sockaddr *)&addr, sizeof(addr));
	listen(_serverFd, SOMAXCONN);

	_epollFd = epoll_create1(0);

	epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = _serverFd;

	epoll_ctl(_epollFd, EPOLL_CTL_ADD, _serverFd, &ev);
}

void Server::acceptClient()
{
	int clientFd = accept(_serverFd, NULL, NULL);
	if (clientFd < 0)
		return;

	fcntl(clientFd, F_SETFL, O_NONBLOCK);

	epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = clientFd;
	epoll_ctl(_epollFd, EPOLL_CTL_ADD, clientFd, &ev);

	std::cout << "Client connected: " << clientFd << std::endl;
}

// ---------------------------------------------------------------------
// Routing: encuentra el location cuyo "path" case mejor (prefijo mas largo)
// ---------------------------------------------------------------------
const LocationConfig* Server::_matchLocation(const std::string& path) const
{
	const LocationConfig* best = NULL;
	size_t bestLen = 0;

	for (size_t i = 0; i < _config.locations.size(); ++i)
	{
		const std::string& locPath = _config.locations[i].path;

		if (path.compare(0, locPath.size(), locPath) == 0)
		{
			if (locPath.size() > bestLen)
			{
				bestLen = locPath.size();
				best = &_config.locations[i];
			}
		}
	}
	return best;
}

bool Server::_isMethodAllowed(const LocationConfig* loc, const std::string& method) const
{
	for (size_t i = 0; i < loc->allowed_methods.size(); ++i)
	{
		if (loc->allowed_methods[i] == method)
			return true;
	}
	return false;
}

bool Server::_hasPathTraversal(const std::string& path) const
{
	return path.find("..") != std::string::npos;
}

// Convierte el path de la URL en una ruta de fichero real,
// combinando el root del location (o el del server si no tiene) con
// lo que queda de la URL tras quitar el prefijo del location.
std::string Server::_resolvePath(const LocationConfig* loc, const std::string& reqPath) const
{
	std::string root;
	if (!loc->upload_path.empty())
		root = loc->upload_path;
	else if (!loc->root_directory.empty())
		root = loc->root_directory;
	else
		root = _config.root_directory;
	std::string relative = reqPath.substr(loc->path.size());

	if (!relative.empty() && relative[0] == '/')
		relative.erase(0, 1);

	std::string fullPath = root;
	if (!fullPath.empty() && fullPath[fullPath.size() - 1] != '/')
		fullPath += "/";
	fullPath += relative;

	return fullPath;
}

// ---------------------------------------------------------------------
// GET: sirve ficheros estaticos, indices y listado de directorio
// AHORA TAMBIÉN: detecta y ejecuta scripts CGI
// ---------------------------------------------------------------------
void Server::_handleGet(int clientFd, const Request& request, const LocationConfig* loc)
{
	std::string fullPath = _resolvePath(loc, request.getPath());

	struct stat st;
	if (stat(fullPath.c_str(), &st) != 0)
	{
		_sendErrorResponse(clientFd, 404);
		return;
	}

	// Detectar si es un script CGI ANTES de tratar como directorio
	if (S_ISREG(st.st_mode) && _isCGIRequest(fullPath))
	{
		_handleCGI(clientFd, request, loc, fullPath);
		return;
	}

	if (S_ISDIR(st.st_mode))
	{
		bool found_index = false;

		for (size_t i = 0; i < loc->index_files.size() && !found_index; ++i)
		{
			std::string indexPath = fullPath;
			if (!indexPath.empty() && indexPath[indexPath.size() - 1] != '/')
				indexPath += "/";
			indexPath += loc->index_files[i];

			struct stat idxSt;
			if (stat(indexPath.c_str(), &idxSt) == 0 && S_ISREG(idxSt.st_mode))
			{
				fullPath = indexPath;
				found_index = true;
			}
		}

		if (!found_index)
		{
			if (loc->autoindex)
			{
				std::string page = _buildAutoindexPage(fullPath, request.getPath());
				_sendResponse(clientFd, 200, "OK", "text/html", page);
			}
			else
			{
				_sendErrorResponse(clientFd, 403);
			}
			return;
		}
	}

	std::ifstream file(fullPath.c_str(), std::ios::binary);
	if (!file.is_open())
	{
		_sendErrorResponse(clientFd, 403);
		return;
	}

	std::ostringstream contents;
	contents << file.rdbuf();

	_sendResponse(clientFd, 200, "OK", _getMimeType(fullPath), contents.str());
}

// ---------------------------------------------------------------------
// POST: soporta CGI scripts o guarda el body en upload_path
// ---------------------------------------------------------------------
void Server::_handlePost(int clientFd, const Request& request, const LocationConfig* loc)
{
	std::string fullPath = _resolvePath(loc, request.getPath());

	struct stat st;
	if (stat(fullPath.c_str(), &st) == 0 && S_ISREG(st.st_mode) && _isCGIRequest(fullPath))
	{
		// Si es un script CGI, ejecutarlo
		_handleCGI(clientFd, request, loc, fullPath);
		return;
	}

	// Si no es CGI, guardar el body en upload_path
	if (loc->upload_path.empty())
	{
		_sendErrorResponse(clientFd, 403);
		return;
	}

	if (_config.client_max_body_size > 0 &&
		static_cast<long long>(request.getBody().size()) > _config.client_max_body_size)
	{
		_sendErrorResponse(clientFd, 413);
		return;
	}

	// Intentamos sacar un nombre de fichero del final de la URL;
	// si no hay, generamos uno basado en timestamp.
	std::string relative = request.getPath().substr(loc->path.size());
	std::string filename;

	size_t lastSlash = relative.find_last_of('/');
	if (!relative.empty() && relative[relative.size() - 1] != '/')
	{
		if (lastSlash == std::string::npos)
			filename = relative;
		else if (lastSlash + 1 < relative.size())
			filename = relative.substr(lastSlash + 1);
	}

	if (filename.empty())
	{
		std::ostringstream oss;
		oss << "upload_" << static_cast<long>(std::time(NULL));
		filename = oss.str();
	}

	std::string uploadPath = loc->upload_path;
	if (!uploadPath.empty() && uploadPath[uploadPath.size() - 1] != '/')
		uploadPath += "/";
	uploadPath += filename;

	std::ofstream outFile(uploadPath.c_str(), std::ios::binary | std::ios::trunc);
	if (!outFile.is_open())
	{
		_sendErrorResponse(clientFd, 500);
		return;
	}

	outFile.write(request.getBody().c_str(), request.getBody().size());
	outFile.close();

	std::string body = "File uploaded successfully: " + filename + "\n";
	std::map<std::string, std::string> headers;
	headers["Location"] = request.getPath();

	_sendResponse(clientFd, 201, "Created", "text/plain", body, headers);
}

// ---------------------------------------------------------------------
// DELETE: elimina un fichero regular del filesystem
// ---------------------------------------------------------------------
void Server::_handleDelete(int clientFd, const Request& request, const LocationConfig* loc)
{
	std::string fullPath = _resolvePath(loc, request.getPath());

	struct stat st;
	if (stat(fullPath.c_str(), &st) != 0)
	{
		_sendErrorResponse(clientFd, 404);
		return;
	}

	if (!S_ISREG(st.st_mode))
	{
		// Por seguridad, no borramos directorios.
		_sendErrorResponse(clientFd, 403);
		return;
	}

	if (std::remove(fullPath.c_str()) != 0)
	{
		_sendErrorResponse(clientFd, 500);
		return;
	}

	_sendResponse(clientFd, 200, "OK", "text/plain", "Resource deleted successfully\n");
}

// ---------------------------------------------------------------------
// Helpers de respuesta HTTP
// ---------------------------------------------------------------------
void Server::_sendResponse(int clientFd, int code, const std::string& statusText,
	const std::string& contentType, const std::string& body,
	const std::map<std::string, std::string>& extraHeaders)
{
	std::ostringstream response;
	response << "HTTP/1.1 " << code << " " << statusText << "\r\n";
	response << "Content-Type: " << contentType << "\r\n";
	response << "Content-Length: " << body.size() << "\r\n";
	response << "Connection: close\r\n";

	for (std::map<std::string, std::string>::const_iterator it = extraHeaders.begin();
		it != extraHeaders.end(); ++it)
	{
		response << it->first << ": " << it->second << "\r\n";
	}

	response << "\r\n" << body;

	std::string responseStr = response.str();
	send(clientFd, responseStr.c_str(), responseStr.size(), 0);
}

void Server::_sendErrorResponse(int clientFd, int code)
{
	std::string statusText = _statusText(code);
	std::map<int, std::string>::const_iterator it = _config.error_pages.find(code);

	if (it != _config.error_pages.end())
	{
		// Probamos primero la ruta tal cual viene en la config,
		// y si falla, relativa al root del server.
		std::ifstream file(it->second.c_str(), std::ios::binary);

		if (!file.is_open())
		{
			std::string path = _config.root_directory;
			if (!path.empty() && path[path.size() - 1] != '/')
				path += "/";
			path += it->second;
			file.open(path.c_str(), std::ios::binary);
		}

		if (file.is_open())
		{
			std::ostringstream contents;
			contents << file.rdbuf();
			_sendResponse(clientFd, code, statusText, "text/html", contents.str());
			return;
		}
	}

	_sendResponse(clientFd, code, statusText, "text/html", _defaultErrorBody(code, statusText));
}

std::string Server::_defaultErrorBody(int code, const std::string& statusText) const
{
	std::ostringstream oss;
	oss << "<html><head><title>" << code << " " << statusText << "</title></head>"
		<< "<body><h1>" << code << " " << statusText << "</h1></body></html>";
	return oss.str();
}

std::string Server::_statusText(int code) const
{
	switch (code)
	{
		case 200: return "OK";
		case 201: return "Created";
		case 204: return "No Content";
		case 301: return "Moved Permanently";
		case 302: return "Found";
		case 400: return "Bad Request";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 405: return "Method Not Allowed";
		case 413: return "Payload Too Large";
		case 500: return "Internal Server Error";
		default:  return "Error";
	}
}

std::string Server::_getMimeType(const std::string& path) const
{
	size_t dotPos = path.find_last_of('.');
	if (dotPos == std::string::npos)
		return "application/octet-stream";

	std::string ext = path.substr(dotPos + 1);

	if (ext == "html" || ext == "htm") return "text/html";
	if (ext == "css")                  return "text/css";
	if (ext == "js")                   return "application/javascript";
	if (ext == "json")                 return "application/json";
	if (ext == "png")                  return "image/png";
	if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
	if (ext == "gif")                  return "image/gif";
	if (ext == "svg")                  return "image/svg+xml";
	if (ext == "ico")                  return "image/x-icon";
	if (ext == "txt")                  return "text/plain";
	if (ext == "pdf")                  return "application/pdf";

	return "application/octet-stream";
}

// ---------------------------------------------------------------------
// CGI: detecta si es un script ejecutable
// ---------------------------------------------------------------------
bool Server::_isCGIRequest(const std::string& fullPath) const
{
	size_t dotPos = fullPath.find_last_of('.');
	if (dotPos == std::string::npos)
		return false;

	std::string ext = fullPath.substr(dotPos + 1);
	
	// Soportar extensiones comunes de CGI
	if (ext == "py" || ext == "sh" || ext == "pl" || ext == "cgi")
		return true;

	return false;
}

// ---------------------------------------------------------------------
// CGI: ejecuta un script y devuelve su output
// ---------------------------------------------------------------------
void Server::_handleCGI(int clientFd, const Request& request, const LocationConfig* loc, const std::string& fullPath)
{
	std::string interpreter;
	size_t dotPos = fullPath.find_last_of('.');
	if (dotPos == std::string::npos)
	{
		_sendErrorResponse(clientFd, 500);
		return;
	}

	std::string ext = fullPath.substr(dotPos + 1);

	// Determinar el intérprete según la extensión
	if (ext == "py")
		interpreter = "/usr/bin/python3";
	else if (ext == "sh" || ext == "cgi")
		interpreter = "/bin/bash";
	else if (ext == "pl")
		interpreter = "/usr/bin/perl";
	else
	{
		_sendErrorResponse(clientFd, 500);
		return;
	}

	// Verificar que el script existe y es ejecutable
	struct stat st;
	if (stat(fullPath.c_str(), &st) != 0)
	{
		_sendErrorResponse(clientFd, 404);
		return;
	}

	if (!S_ISREG(st.st_mode))
	{
		_sendErrorResponse(clientFd, 403);
		return;
	}

	// Ejecutar el script usando CGIHandler
	try
	{
		CGIHandler cgiHandler(fullPath, interpreter);
		int result = cgiHandler.execute(request, loc->upload_path);

		if (result < 0)
		{
			_sendErrorResponse(clientFd, 500);
			return;
		}

		// Leer el output del script (simplificado: aquí deberías capturar stdout)
		std::string output = "CGI script executed successfully\n";
		_sendResponse(clientFd, 200, "OK", "text/plain", output);
	}
	catch (const std::exception& e)
	{
		std::cerr << "CGI Error: " << e.what() << std::endl;
		_sendErrorResponse(clientFd, 500);
	}
}

std::string Server::_buildAutoindexPage(const std::string& dirPath, const std::string& reqPath) const
{
	std::ostringstream page;
	page << "<html><head><title>Index of " << reqPath << "</title></head><body>";
	page << "<h1>Index of " << reqPath << "</h1><ul>";

	DIR* dir = opendir(dirPath.c_str());
	if (dir)
	{
		struct dirent* entry;
		while ((entry = readdir(dir)) != NULL)
		{
			std::string name = entry->d_name;
			if (name == ".")
				continue;

			std::string link = reqPath;
			if (!link.empty() && link[link.size() - 1] != '/')
				link += "/";
			link += name;

			page << "<li><a href=\"" << link << "\">" << name << "</a></li>";
		}
		closedir(dir);
	}

	page << "</ul></body></html>";
	return page.str();
}

// ---------------------------------------------------------------------
// Punto de entrada por conexion: parsea y despacha al metodo correcto
// ---------------------------------------------------------------------
void Server::handleClient(int clientFd)
{
	char buf[4096];
	ssize_t n = recv(clientFd, buf, sizeof(buf), 0);
	if (n <= 0)
	{
		epoll_ctl(_epollFd, EPOLL_CTL_DEL, clientFd, NULL);
		close(clientFd);
		return;
	}

	Request request;
	request.parse(std::string(buf, n));

	if (request.getErrorCode() != 0)
	{
		_sendErrorResponse(clientFd, request.getErrorCode());
		epoll_ctl(_epollFd, EPOLL_CTL_DEL, clientFd, NULL);
		close(clientFd);
		return;
	}

	std::cout << "Request: " << request.getMethod() << " " << request.getPath() << std::endl;

	const LocationConfig* loc = _matchLocation(request.getPath());
	if (!loc)
	{
		_sendErrorResponse(clientFd, 404);
		epoll_ctl(_epollFd, EPOLL_CTL_DEL, clientFd, NULL);
		close(clientFd);
		return;
	}

	if (!_isMethodAllowed(loc, request.getMethod()))
	{
		_sendErrorResponse(clientFd, 405);
		epoll_ctl(_epollFd, EPOLL_CTL_DEL, clientFd, NULL);
		close(clientFd);
		return;
	}

	if (loc->return_code != 0)
	{
		std::map<std::string, std::string> headers;
		headers["Location"] = loc->return_url;
		_sendResponse(clientFd, loc->return_code, _statusText(loc->return_code), "text/plain", "", headers);
		epoll_ctl(_epollFd, EPOLL_CTL_DEL, clientFd, NULL);
		close(clientFd);
		return;
	}

	if (_hasPathTraversal(request.getPath()))
	{
		_sendErrorResponse(clientFd, 403);
	}
	else if (request.getMethod() == "GET")
	{
		_handleGet(clientFd, request, loc);
	}
	else if (request.getMethod() == "POST")
	{
		_handlePost(clientFd, request, loc);
	}
	else if (request.getMethod() == "DELETE")
	{
		_handleDelete(clientFd, request, loc);
	}

	epoll_ctl(_epollFd, EPOLL_CTL_DEL, clientFd, NULL);
	close(clientFd);
}

void Server::run()
{
	epoll_event events[64];

	while (true)
	{
		int n = epoll_wait(_epollFd, events, 64, -1);

		for (int i = 0; i < n; ++i)
		{
			if (events[i].data.fd == _serverFd)
				acceptClient();
			else
				handleClient(events[i].data.fd);
		}
	}
}
/* #include "../includes/Server.hpp"
#include "../includes/Request.hpp"
#include <iostream>
#include <cstring>
#include <cstdio>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cerrno>
#include <ctime>
#include <cstdlib>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>

namespace
{
	bool startsWith(const std::string& value, const std::string& prefix)
	{
		return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
	}

	std::string trimTrailingSlash(const std::string& path)
	{
		if (path.size() > 1 && path[path.size() - 1] == '/')
			return path.substr(0, path.size() - 1);
		return path;
	}

	std::string statusText(int statusCode)
	{
		switch (statusCode)
		{
			case 200: return "OK";
			case 201: return "Created";
			case 204: return "No Content";
			case 400: return "Bad Request";
			case 403: return "Forbidden";
			case 404: return "Not Found";
			case 405: return "Method Not Allowed";
			default:  return "Internal Server Error";
		}
	}

	std::string joinPath(const std::string& left, const std::string& right)
	{
		if (left.empty())
			return right;
		if (right.empty())
			return left;
		if (left[left.size() - 1] == '/' && right[0] == '/')
			return left + right.substr(1);
		if (left[left.size() - 1] != '/' && right[0] != '/')
			return left + "/" + right;
		return left + right;
	}
}

Server::Server(int port) : _serverFd(-1), _epollFd(-1), _port(port), _config()
{
	_config.port = port;
	if (_config.root_directory.empty())
		_config.root_directory = "./www";
}

Server::Server(const ServerConfig& config) : _serverFd(-1), _epollFd(-1), _port(config.port), _config(config) {}

Server::~Server()
{
	if (_serverFd >= 0) close(_serverFd);
	if (_epollFd >= 0) close(_epollFd);
}

void Server::init()
{
	int opt = 1;
	sockaddr_in addr;

	_serverFd = socket(AF_INET, SOCK_STREAM, 0);
	setsockopt(_serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	fcntl(_serverFd, F_SETFL, O_NONBLOCK);

	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(_port);

	bind(_serverFd, (sockaddr *)&addr, sizeof(addr));
	listen(_serverFd, SOMAXCONN);

	_epollFd = epoll_create1(0);

	epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = _serverFd;

	epoll_ctl(_epollFd, EPOLL_CTL_ADD, _serverFd, &ev);
}

void Server::acceptClient()
{
	int clientFd = accept(_serverFd, NULL, NULL);
	if (clientFd < 0)
		return;

	fcntl(clientFd, F_SETFL, O_NONBLOCK);

	epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = clientFd;
	epoll_ctl(_epollFd, EPOLL_CTL_ADD, clientFd, &ev);

	std::cout << "Client connected: " << clientFd << std::endl;
}

const LocationConfig* Server::_findBestLocation(const std::string& requestPath) const
{
	const LocationConfig* bestLocation = NULL;
	size_t bestLength = 0;

	for (size_t i = 0; i < _config.locations.size(); ++i)
	{
		const LocationConfig& candidate = _config.locations[i];
		if (!startsWith(requestPath, candidate.path))
			continue;
		if (candidate.path.size() >= bestLength)
		{
			bestLocation = &candidate;
			bestLength = candidate.path.size();
		}
	}

	return bestLocation;
}

bool Server::_isMethodAllowed(const std::string& requestPath, const std::string& method) const
{
	const LocationConfig* location = _findBestLocation(requestPath);
	const std::vector<std::string>* allowedMethods = NULL;

	if (location != NULL && !location->allowed_methods.empty())
		allowedMethods = &location->allowed_methods;
	else if (!_config.allowed_methods.empty())
		allowedMethods = &_config.allowed_methods;

	if (allowedMethods == NULL)
		return true;

	return std::find(allowedMethods->begin(), allowedMethods->end(), method) != allowedMethods->end();
}

std::string Server::_resolvePath(const std::string& requestPath, const LocationConfig*& location) const
{
	location = _findBestLocation(requestPath);

	std::string basePath = _config.root_directory;
	if (location != NULL && !location->root_directory.empty())
		basePath = location->root_directory;

	std::string locationPath = "/";
	if (location != NULL && !location->path.empty())
		locationPath = location->path;

	std::string normalizedRequest = requestPath;
	if (normalizedRequest.empty())
		normalizedRequest = "/";

	if (normalizedRequest.find("..") != std::string::npos)
		return "";

	std::string relativePath;
	if (location != NULL && startsWith(normalizedRequest, locationPath))
		relativePath = normalizedRequest.substr(locationPath.size());
	else if (normalizedRequest.size() > 0 && normalizedRequest[0] == '/')
		relativePath = normalizedRequest.substr(1);
	else
		relativePath = normalizedRequest;

	while (!relativePath.empty() && relativePath[0] == '/')
		relativePath.erase(0, 1);

	std::string resolved = trimTrailingSlash(basePath);
	if (!relativePath.empty())
		resolved = joinPath(resolved, relativePath);

	return resolved;
}

std::string Server::_guessContentType(const std::string& path) const
{
	std::string::size_type dot = path.find_last_of('.');
	if (dot == std::string::npos)
		return "text/plain";

	std::string extension = path.substr(dot + 1);
	if (extension == "html" || extension == "htm") return "text/html";
	if (extension == "css") return "text/css";
	if (extension == "js") return "application/javascript";
	if (extension == "json") return "application/json";
	if (extension == "png") return "image/png";
	if (extension == "jpg" || extension == "jpeg") return "image/jpeg";
	if (extension == "gif") return "image/gif";
	if (extension == "txt") return "text/plain";
	return "application/octet-stream";
}

std::string Server::_readFile(const std::string& path) const
{
	std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
	if (!file.is_open())
		return "";

	std::ostringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}

std::string Server::_listDirectory(const std::string& path, const std::string& requestPath) const
{
	DIR* directory = opendir(path.c_str());
	if (directory == NULL)
		return "";

	std::ostringstream html;
	html << "<html><body><h1>Index of " << requestPath << "</h1><ul>";

	struct dirent* entry = NULL;
	while ((entry = readdir(directory)) != NULL)
	{
		if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0)
			continue;
		html << "<li>" << entry->d_name << "</li>";
	}

	html << "</ul></body></html>";
	closedir(directory);
	return html.str();
}

bool Server::_writeUploadFile(const std::string& directory, const std::string& body, std::string& savedPath) const
{
	struct stat info;
	if (stat(directory.c_str(), &info) != 0 || !S_ISDIR(info.st_mode))
		return false;

	std::ostringstream fileName;
	fileName << directory;
	if (!directory.empty() && directory[directory.size() - 1] != '/')
		fileName << "/";
	fileName << "upload_" << static_cast<long>(std::time(NULL)) << "_" << static_cast<long>(getpid()) << ".txt";

	savedPath = fileName.str();
	std::ofstream file(savedPath.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
	if (!file.is_open())
		return false;

	file << body;
	return file.good();
}

bool Server::_removeResource(const std::string& path) const
{
	struct stat info;
	if (stat(path.c_str(), &info) != 0)
		return false;
	if (S_ISDIR(info.st_mode))
		return false;
	return unlink(path.c_str()) == 0;
}

std::string Server::_buildResponse(int statusCode, const std::string& contentType, const std::string& body) const
{
	std::ostringstream response;
	response << "HTTP/1.1 " << statusCode << ' ' << statusText(statusCode) << "\r\n";
	response << "Content-Type: " << contentType << "\r\n";
	response << "Content-Length: " << body.size() << "\r\n";
	response << "Connection: close\r\n\r\n";
	response << body;
	return response.str();
}

bool Server::_sendAll(int clientFd, const std::string& response) const
{
	size_t sent = 0;
	while (sent < response.size())
	{
		ssize_t current = send(clientFd, response.data() + sent, response.size() - sent, 0);
		if (current <= 0)
			return false;
		sent += static_cast<size_t>(current);
	}
	return true;
}

std::string Server::_handleGet(const Request& request) const
{
	const LocationConfig* location = NULL;
	std::string resolvedPath = _resolvePath(request.getPath(), location);
	if (resolvedPath.empty())
		return _buildResponse(403, "text/plain", "403 Forbidden\n");

	struct stat info;
	if (stat(resolvedPath.c_str(), &info) != 0)
		return _buildResponse(404, "text/plain", "404 Not Found\n");

	if (S_ISDIR(info.st_mode))
	{
		std::string directoryPath = trimTrailingSlash(resolvedPath);
		std::vector<std::string> indices;
		if (location != NULL && !location->index_files.empty())
			indices = location->index_files;

		if (indices.empty())
		{
			if (location != NULL && location->autoindex)
			{
				std::string listing = _listDirectory(directoryPath, request.getPath());
				if (listing.empty())
					return _buildResponse(403, "text/plain", "403 Forbidden\n");
				return _buildResponse(200, "text/html", listing);
			}
			return _buildResponse(403, "text/plain", "403 Forbidden\n");
		}

		for (size_t i = 0; i < indices.size(); ++i)
		{
			std::string indexPath = joinPath(directoryPath, indices[i]);
			std::string indexBody = _readFile(indexPath);
			if (!indexBody.empty() || stat(indexPath.c_str(), &info) == 0)
				return _buildResponse(200, _guessContentType(indexPath), indexBody);
		}

		if (location != NULL && location->autoindex)
		{
			std::string listing = _listDirectory(directoryPath, request.getPath());
			if (listing.empty())
				return _buildResponse(403, "text/plain", "403 Forbidden\n");
			return _buildResponse(200, "text/html", listing);
		}

		return _buildResponse(404, "text/plain", "404 Not Found\n");
	}

	std::string body = _readFile(resolvedPath);
	if (body.empty())
	{
		if (stat(resolvedPath.c_str(), &info) == 0)
			return _buildResponse(200, _guessContentType(resolvedPath), body);
		return _buildResponse(404, "text/plain", "404 Not Found\n");
	}

	return _buildResponse(200, _guessContentType(resolvedPath), body);
}

std::string Server::_handlePost(const Request& request) const
{
	const LocationConfig* location = NULL;
	std::string resolvedPath = _resolvePath(request.getPath(), location);
	std::string uploadDirectory = _config.root_directory;

	if (location != NULL && !location->upload_path.empty())
		uploadDirectory = location->upload_path;
	else if (location != NULL && !location->root_directory.empty())
		uploadDirectory = location->root_directory;

	if (resolvedPath.empty())
		return _buildResponse(403, "text/plain", "403 Forbidden\n");

	std::string savedPath;
	if (!_writeUploadFile(uploadDirectory, request.getBody(), savedPath))
	{
		if (request.getBody().empty())
			return _buildResponse(400, "text/plain", "400 Bad Request\n");
		return _buildResponse(500, "text/plain", "500 Internal Server Error\n");
	}

	return _buildResponse(201, "text/plain", "201 Created: " + savedPath + "\n");
}

std::string Server::_handleDelete(const Request& request) const
{
	const LocationConfig* location = NULL;
	std::string resolvedPath = _resolvePath(request.getPath(), location);
	if (resolvedPath.empty())
		return _buildResponse(403, "text/plain", "403 Forbidden\n");

	if (!_removeResource(resolvedPath))
		return _buildResponse(404, "text/plain", "404 Not Found\n");

	return _buildResponse(200, "text/plain", "200 OK: resource deleted\n");
}

void Server::handleClient(int clientFd)
{
	char buf[4096];
	ssize_t n = recv(clientFd, buf, sizeof(buf), 0);
	if (n <= 0)
	{
		epoll_ctl(_epollFd, EPOLL_CTL_DEL, clientFd, NULL);
		close(clientFd);
		return;
	}

	Request request;
	request.parse(std::string(buf, n));

	if (request.getErrorCode() != 0)
	{
		std::ostringstream body;
		body << request.getErrorCode() << " Bad Request\n";
		std::string response = _buildResponse(request.getErrorCode(), "text/plain", body.str());
		_sendAll(clientFd, response);
		epoll_ctl(_epollFd, EPOLL_CTL_DEL, clientFd, NULL);
		close(clientFd);
		return;
	}

	if (!_isMethodAllowed(request.getPath(), request.getMethod()))
	{
		std::string response = _buildResponse(405, "text/plain", "405 Method Not Allowed\n");
		_sendAll(clientFd, response);
		epoll_ctl(_epollFd, EPOLL_CTL_DEL, clientFd, NULL);
		close(clientFd);
		return;
	}

	std::cout << "Request: " << request.getMethod() << " " << request.getPath() << std::endl;

	std::string response;
	if (request.getMethod() == "GET")
		response = _handleGet(request);
	else if (request.getMethod() == "POST")
		response = _handlePost(request);
	else if (request.getMethod() == "DELETE")
		response = _handleDelete(request);
	else
		response = _buildResponse(405, "text/plain", "405 Method Not Allowed\n");

	_sendAll(clientFd, response);
	epoll_ctl(_epollFd, EPOLL_CTL_DEL, clientFd, NULL);
	close(clientFd);
}

void Server::run()
{
	epoll_event events[64];

	while (true)
	{
		int n = epoll_wait(_epollFd, events, 64, -1);

		for (int i = 0; i < n; ++i)
		{
			if (events[i].data.fd == _serverFd)
				acceptClient();
			else
				handleClient(events[i].data.fd);
		}
	}
} */