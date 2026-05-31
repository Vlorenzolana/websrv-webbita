#ifndef METHODHANDLER_HPP
#define METHODHANDLER_HPP

#include "Response.hpp"
#include "Request.hpp"
#include "Config.hpp"
#include "RequestHandler.hpp"
#include <fstream>
#include <unistd.h>
#include <vector>

#define USER_SUSCEED 22
#define USER_ERROR 11

class MethodHandler
{
	private:
		static Response _handleGet(Client& client, const LocationConfig* location);
		static Response _handlePost(Client& client, const LocationConfig* location);
		static Response _handleDelete(Client& client, const LocationConfig* location);

		static std::string  _readFile(const std::string& path);
		static std::string  _getMimeType(const std::string& path);
		static std::string  _parseMultipartBody(const std::string& body, const std::string& boundary);
		static Response _handleUser(Client& client, Response &response);
		static Response _handleUserSingup(Client& client, Response &response);
		static Response _handleUserLoging(Client& client, Response &response);
		static void _handleAddAcces(Client &client, Response &response);


	public:
		static std::string  getMimeType(const std::string& path);
		// metodo publico que gestiona todo
		static Response handle(Client& client, const LocationConfig* location);
};

#endif // METHODHANDLER_HPP
