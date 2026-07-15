#ifndef SERVER_HPP
#define SERVER_HPP

#include "ConfigParser.hpp"   // Debe definir (o incluir) ServerConfig y LocationConfig
#include "Request.hpp"

#include <string>
#include <map>
#include <sys/epoll.h>

class Server
{
	public:
		// OJO: el constructor ahora recibe el ServerConfig completo,
		// no solo el puerto, porque el enrutamiento necesita las
		// locations, el root, error_pages, client_max_body_size, etc.
		Server(const ServerConfig& config);
		~Server();

		void init();
		void run();

	private:
		int          _serverFd;
		int          _epollFd;
		int          _port;
		ServerConfig _config;

		void acceptClient();
		void handleClient(int clientFd);

		// --- Routing ---
		const LocationConfig* _matchLocation(const std::string& path) const;
		bool _isMethodAllowed(const LocationConfig* loc, const std::string& method) const;
		std::string _resolvePath(const LocationConfig* loc, const std::string& reqPath) const;
		bool _hasPathTraversal(const std::string& path) const;

		// --- Handlers de metodo ---
		void _handleGet(int clientFd, const Request& request, const LocationConfig* loc);
		void _handlePost(int clientFd, const Request& request, const LocationConfig* loc);
		void _handleDelete(int clientFd, const Request& request, const LocationConfig* loc);

		// --- CGI Support ---
		bool _isCGIRequest(const std::string& fullPath) const;
		void _handleCGI(int clientFd, const Request& request, const LocationConfig* loc, const std::string& fullPath);

		// --- Helpers de respuesta ---
		void _sendResponse(int clientFd, int code, const std::string& statusText,
			const std::string& contentType, const std::string& body,
			const std::map<std::string, std::string>& extraHeaders = std::map<std::string, std::string>());
		void _sendErrorResponse(int clientFd, int code);
		std::string _defaultErrorBody(int code, const std::string& statusText) const;
		std::string _statusText(int code) const;
		std::string _getMimeType(const std::string& path) const;
		std::string _buildAutoindexPage(const std::string& dirPath, const std::string& reqPath) const;
};

#endif

/* #ifndef SERVER_HPP
#define SERVER_HPP

#include "Config.hpp"
#include <sys/epoll.h>
#include <string>

class Request;

class Server
{
private:
    int _serverFd;
    int _epollFd;
    int _port;
    ServerConfig _config;

public:
    Server(int port);
    Server(const ServerConfig& config);
    ~Server();

    void init();
    void run();

private:
    void acceptClient();
    void handleClient(int clientFd);

    std::string _handleGet(const Request& request) const;
    std::string _handlePost(const Request& request) const;
    std::string _handleDelete(const Request& request) const;

    const LocationConfig* _findBestLocation(const std::string& requestPath) const;
    bool _isMethodAllowed(const std::string& requestPath, const std::string& method) const;
    std::string _buildResponse(int statusCode, const std::string& contentType, const std::string& body) const;
    bool _sendAll(int clientFd, const std::string& response) const;
    std::string _readFile(const std::string& path) const;
    std::string _listDirectory(const std::string& path, const std::string& requestPath) const;
    std::string _resolvePath(const std::string& requestPath, const LocationConfig*& location) const;
    std::string _guessContentType(const std::string& path) const;
    bool _writeUploadFile(const std::string& directory, const std::string& body, std::string& savedPath) const;
    bool _removeResource(const std::string& path) const;
};

#endif
 */