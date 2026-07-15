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
		// El server se construye con toda la configuración del bloque `server`.
		// Así puede usar puertos, locations, root, errores y límites de body.
		Server(const ServerConfig& config);
		~Server();

		// Abre socket, configura epoll y deja el server listo para escuchar.
		void init();
		// Bucle principal de eventos: acepta clientes y procesa requests.
		void run();

	private:
		// File descriptors del socket de escucha y del epoll.
		int          _serverFd;
		int          _epollFd;
		// Puerto asociado a este virtual host.
		int          _port;
		// Configuración completa del server actual.
		ServerConfig _config;

		// Acepta un cliente nuevo y lo registra en epoll.
		void acceptClient();
		// Lee, parsea y responde a una conexión concreta.
		void handleClient(int clientFd);

		// --- Routing ---
		// Busca el location más específico que encaja con la ruta.
		const LocationConfig* _matchLocation(const std::string& path) const;
		// Comprueba si el método HTTP está permitido en ese location.
		bool _isMethodAllowed(const LocationConfig* loc, const std::string& method) const;
		// Convierte una ruta URL a una ruta real del filesystem.
		std::string _resolvePath(const LocationConfig* loc, const std::string& reqPath) const;
		// Protección simple contra intentos de `..` en la ruta.
		bool _hasPathTraversal(const std::string& path) const;

		// --- Handlers de metodo ---
		// Atiende GET sirviendo ficheros, índices o autoindex.
		void _handleGet(int clientFd, const Request& request, const LocationConfig* loc);
		// Atiende POST, CGI o subida de archivos.
		void _handlePost(int clientFd, const Request& request, const LocationConfig* loc);
		// Atiende DELETE eliminando recursos regulares.
		void _handleDelete(int clientFd, const Request& request, const LocationConfig* loc);

		// --- CGI Support ---
		// Detecta si una ruta parece un script CGI por extensión.
		bool _isCGIRequest(const std::string& fullPath) const;
		// Ejecuta el script CGI y devuelve la respuesta al cliente.
		void _handleCGI(int clientFd, const Request& request, const LocationConfig* loc, const std::string& fullPath);

		// --- Helpers de respuesta ---
		// Construye y envía una respuesta HTTP genérica.
		void _sendResponse(int clientFd, int code, const std::string& statusText,
			const std::string& contentType, const std::string& body,
			const std::map<std::string, std::string>& extraHeaders = std::map<std::string, std::string>());
		// Envía una respuesta de error usando página personalizada si existe.
		void _sendErrorResponse(int clientFd, int code);
		// Genera un HTML simple por defecto para errores.
		std::string _defaultErrorBody(int code, const std::string& statusText) const;
		// Traduce códigos HTTP a texto humano legible.
		std::string _statusText(int code) const;
		// Adivina el MIME type a partir de la extensión.
		std::string _getMimeType(const std::string& path) const;
		// Crea una página HTML de listado de directorio.
		std::string _buildAutoindexPage(const std::string& dirPath, const std::string& reqPath) const;
};

#endif