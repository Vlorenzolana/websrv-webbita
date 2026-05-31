#include "../includes/CGI.hpp"
#include "../includes/Logger.hpp"

// Funcion principal que gestiona todo los metodos
Response MethodHandler::handle(Client &client, const LocationConfig *location)
{
	if (client.getRequest().getMethod() == "GET")
		return (_handleGet(client, location));
	else if (client.getRequest().getMethod() == "POST")
		return (_handlePost(client, location));
	else if (client.getRequest().getMethod() == "DELETE")
		return (_handleDelete(client, location));

	Response response;
	response.buildErrorResponse(501, client.getConfig(), location);
	return (response);
}

// --- Implementacion de los metodos ---

Response MethodHandler::_handleGet(Client &client, const LocationConfig* location)
{
	const Request &request = client.getRequest();
	const ServerConfig &config = client.getConfig();

	std::string full_path = request.getFullPath();

	Response response;

	if (access(full_path.c_str(), F_OK) == -1) // Si el archivo no existe
	{
		response.buildErrorResponse(404, config, location);
	}
	else // Si el archivo existe
	{
		if (request.getIsCGI())
		{
			// Iniciar CGI de forma asíncrona; la respuesta se completará en Server::handleCGIEvent
			manageCGI(client, response);
			return response; // vacío; no pasar a WRITING
		}
		else
		{
			response.buildCustomResponse("200", "OK", _readFile(full_path));
			response.addHeader("Content-Type", getMimeType(full_path));
		}
	}
	return (response);
}

Response MethodHandler::_handlePost(Client &client, const LocationConfig *location)
{
	const Request &request = client.getRequest();
	const ServerConfig &config = client.getConfig();

	std::string full_path = request.getFullPath();
	Response response;
	// Soporte POST para CGI
	if (request.getIsCGI()) // -> [`Request::getIsCGI`](src/Request.cpp)
	{
		manageCGI(client, response); // -> [`manageCGI`](src/CGI.cpp)
		return response;
	}

	// //Manejo de los usuarios
	if (request.getPath().find("/users/") != std::string::npos)
	{
		response = _handleUser(client, response);
		return (response);
	}

	std::string final_path;
	if (location && !location->upload_path.empty())
	{
		std::string request_path = request.getPath();
		size_t last_slash = request_path.rfind('/');
		std::string filename = (last_slash != std::string::npos) ? request_path.substr(last_slash + 1) : request_path;

		final_path = location->upload_path;
		if (final_path[final_path.length() - 1] != '/')
			final_path += '/';
		final_path += filename;
	}
	else
		final_path = request.getFullPath();

	// 1. Conseguir en boundary de la cabecera 'Content-Type'
	std::string content_type = request.getHeaderValue("Content-Type");
	std::string boundary;
	size_t boundary_pos = content_type.find("boundary=");
	if (boundary_pos != std::string::npos)
		boundary = content_type.substr(boundary_pos + 9);
	else
	{
		// Si no es multipart/form-data, lo tratamos como un cuerpo simple
		if (access(final_path.c_str(), F_OK) == 0)
			return (response.buildErrorResponse(409, config, location), response);
		std::ofstream new_file(final_path.c_str(), std::ios::binary);
		if (!new_file.is_open())
			return (response.buildErrorResponse(500, config, location), response);
		new_file << request.getBody();
		new_file.close();
		return (response.buildErrorResponse(201, config, location), response);
	}

	// 2. Comprobar si el recurso ya existe
	if (access(final_path.c_str(), F_OK) == 0)
		return (response.buildErrorResponse(409, config, location), response);

	// 3. Parsear el cuerpo para extraer solo el contenido del archivo
	std::string file_content = MethodHandler::_parseMultipartBody(request.getBody(), boundary);
	if (file_content.empty())
		return (response.buildErrorResponse(400, config, location), response);

	// 4. Escribir el contenido extraido en el archivo
	std::ofstream new_file(final_path.c_str(), std::ios::binary);
	if (!new_file.is_open())
		return (response.buildErrorResponse(500, config, location), response);

	new_file.write(file_content.c_str(), file_content.length());
	new_file.close();

	return (response.buildErrorResponse(201, config, location), response);
}

Response MethodHandler::_handleDelete(Client &client, const LocationConfig* location)
{
	const Request &request = client.getRequest();
	const ServerConfig &config = client.getConfig();

	std::string full_path = request.getFullPath();
	Response response;

	if (access(full_path.c_str(), F_OK) == -1)
	{
		// Si el archivo no existe, no se puede borrar. Devolvemos 404
		response.buildErrorResponse(404, config, location);
	}
	else if (remove(full_path.c_str()) == 0) // Intentamos borrarlo
	{
		response.buildErrorResponse(204, config, location);
	}
	else
	{
		// Si `remove` falla por otra razón (ej. permisos)
		response.buildErrorResponse(500, config, location);
	}
	return (response);
}

// --- Funciones de ayuda ---

// Extrae el contenido del archivo de un cuerpo multipart/form-data
std::string MethodHandler::_parseMultipartBody(const std::string &body, const std::string &boundary)
{
	std::string boundary_sep = "--" + boundary;
	std::string boundary_end = boundary_sep + "--";

	// 1. Encontrar el inicio del cuerpo, la primera aparicion del separador de boundary
	size_t start_pos = body.find(boundary_sep);
	if (start_pos == std::string::npos)
		return ("");

	// Buscamos el final de la cabecera
	size_t headers_end = body.find("\r\n\r\n", start_pos);
	if (headers_end == std::string::npos)
		return ("");

	// El contenido del archivo empieza justo despues
	size_t content_start = headers_end + 4;

	// 2. Encontrar el final del cuerpo
	size_t content_end = body.find(boundary_sep, content_start);
	if (content_end == std::string::npos)
		return ("");

	// Extraemos el contenido, restamos 2 para quitar el "\r\n" final
	return (body.substr(content_start, content_end - content_start - 2));
}

std::string MethodHandler::_readFile(const std::string &path)
{
	std::ifstream file(path.c_str());
	if (!file.is_open())
		return ("");
	std::stringstream buffer;
	buffer << file.rdbuf();
	return (buffer.str());
}

// Funcion importante para saber que Content-Type es
std::string MethodHandler::getMimeType(const std::string &path)
{
	std::map<std::string, std::string> mime_types;
	// Tipos de texto
	mime_types[".html"] = "text/html";
	mime_types[".css"] = "text/css";
	mime_types[".js"] = "application/javascript";
	mime_types[".json"] = "application/json";
	mime_types[".xml"] = "application/xml";
	// Tipos de imagen
	mime_types[".jpg"] = "image/jpg";
	mime_types[".jpeg"] = "image/jpeg";
	mime_types[".png"] = "image/png";
	mime_types[".gif"] = "image/gif";
	mime_types[".svg"] = "image/svg+xml";
	// Tipos de video
	mime_types[".mp4"] = "video/mp4";
	mime_types[".webm"] = "video/webm";
	mime_types[".ogg"] = "video/ogg";
	mime_types[".avi"] = "video/x-msvideo";
	mime_types[".mov"] = "video/quicktime";
	// Other
	mime_types[".pdf"] = "application/pdf";
	mime_types[".php"] = "/usr/bin/php";
	mime_types[".py"] = "/usr/bin/python3";
	mime_types[".sh"] = "/usr/bin/bash";
	mime_types[".zip"] = "application/zip";
	mime_types[".rar"] = "application/x-rar-compressed";

	size_t dot_pos = path.rfind('.');
	if (dot_pos != std::string::npos)
	{
		std::string ext = path.substr(dot_pos);
		if (mime_types.count(ext))
			return (mime_types[ext]);
	}
	// Es el tipo por defecto para descargas
	return ("application/octet-stream");
}

Response MethodHandler::_handleUser(Client &client, Response &response)
{
	const Request &request = client.getRequest();
	if (request.getPath().find("/addAcces") != std::string::npos)
		_handleAddAcces(client, response);
	if (request.getPath().find("/login") != std::string::npos)
		return (_handleUserLoging(client, response));
	if (request.getPath().find("/signup") != std::string::npos)
		return (_handleUserSingup(client, response));
	return (response);
}

static std::string urlDecode(const std::string &in)
{
	std::string out;
	out.reserve(in.size());
	for (size_t i = 0; i < in.size(); ++i)
	{
		unsigned char c = in[i];
		if (c == '+')
			out.push_back(' ');
		else if (c == '%' && i + 2 < in.size() && std::isxdigit(static_cast<unsigned char>(in[i + 1])) && std::isxdigit(static_cast<unsigned char>(in[i + 2])))
		{
			char hex[3] = {static_cast<char>(in[i + 1]), static_cast<char>(in[i + 2]), 0};
			out.push_back(static_cast<char>(strtol(hex, NULL, 16)));
			i += 2;
		}
		else
			out.push_back(c);
	}
	return out;
}

static std::map<std::string, std::string> parseFormURLEncoded(const std::string &body)
{
	std::map<std::string, std::string> fields;
	size_t pos = 0;
	while (pos < body.size())
	{
		size_t amp = body.find('&', pos);
		if (amp == std::string::npos)
			amp = body.size();
		std::string pair = body.substr(pos, amp - pos);
		size_t eq = pair.find('=');
		if (eq != std::string::npos)
		{
			std::string key = urlDecode(pair.substr(0, eq));
			std::string val = urlDecode(pair.substr(eq + 1));
			fields[key] = val;
		}
		pos = amp + 1;
	}
	return fields;
}

static void setUserSessionCookies(Response &response, const User &user)
{
	std::ostringstream id;
	id << user.getId();
	std::ostringstream tc;
	tc << user.getTimesConected();

	std::string headerValue;
	headerValue += "session_id=" + id.str() + "; HttpOnly; Path=/; Max-Age=3600; SameSite=Lax";
	headerValue += "\r\nSet-Cookie: username=" + user.getName() + "; Path=/; Max-Age=3600; SameSite=Lax";
	headerValue += "\r\nSet-Cookie: access_count=" + tc.str() + "; Path=/; Max-Age=3600; SameSite=Lax";
	if (!user.getEmail().empty())
		headerValue += "\r\nSet-Cookie: email=" + user.getEmail() + "; Path=/; Max-Age=3600; SameSite=Lax";
	response.addHeader("Set-Cookie", headerValue);
}

static std::string getRequiredField(const std::map<std::string, std::string> &m, const char *key)
{
	std::map<std::string, std::string>::const_iterator it = m.find(key);
	if (it == m.end() || it->second.empty())
		return "";
	return it->second;
}

Response MethodHandler::_handleUserSingup(Client &client, Response &response)
{
	const Request &request = client.getRequest();
	std::map<std::string, std::string> claveValor = parseFormURLEncoded(request.getBody());
	const std::map<unsigned int, User> &users = client.getServer().getRegisteredUsersRef();
	for (std::map<std::string, std::string>::const_iterator it = claveValor.begin();
		 it != claveValor.end(); ++it)
	{
		std::stringstream ss;
		ss << "[SIGNUP] " << it->first << " = " << it->second;
		Logger::log(INFO, ss.str());
	}
	std::string username = getRequiredField(claveValor, "username");
	std::string email = getRequiredField(claveValor, "email");
	std::string password = getRequiredField(claveValor, "password");

	// Buscar duplicado
	for (std::map<unsigned int, User>::const_iterator it = users.begin();
		 it != users.end(); ++it)
	{
		if (it->second.getName() == username)
		{
			Logger::log(INFO, "USUARIO REPETIDO: " + username);
			response.buildErrorResponse(509, client.getConfig(), NULL);
			return response; // salir temprano
		}
	}
	if (client.getServer().addUser(username, password, email))
	{
		Logger::log(INFO, "[SIGNUP] Usuario creado: " + username);
		const User *u = client.getServer().findUserByName(username);
		std::stringstream body;
		body << "{"
			 << "\"username\":\"" << username << "\","
			 << "\"redirect\":\"/users/user-test.html\""
			 << "}";
		response.buildCustomResponse("201", "Created", body.str());
		response.addHeader("Content-Type", "application/json");
		setUserSessionCookies(response, *u); // Usa helper consistente
		return response;
	}
	else
	{
		Logger::log(FATAL, "[SIGNUP] Fallo al crear usuario (addUser retornó false)");
		response.buildErrorResponse(500, client.getConfig(), NULL);
		return response;
	}
	return (response);
}

Response MethodHandler::_handleUserLoging(Client &client, Response &response)
{
	const Request &request = client.getRequest();
	std::map<std::string, std::string> claveValor = parseFormURLEncoded(request.getBody());
	const std::map<unsigned int, User> &users = client.getServer().getRegisteredUsersRef();

	std::string username = getRequiredField(claveValor, "username");
	std::string email = getRequiredField(claveValor, "email");
	std::string password = getRequiredField(claveValor, "password");

	for (std::map<unsigned int, User>::const_iterator it = users.begin();
		 it != users.end(); ++it)
	{
		if (it->second.getName() == username)
		{
			response.addHeader("Content-Type", "application/json");
			std::stringstream body;
			body << "{"
				 << "\"username\":\"" << username << "\","
				 << "\"redirect\":\"/users/user-test.html\""
				 << "}";
			response.buildCustomResponse("201", "Created", body.str());
			response.addHeader("Content-Type", "application/json");
			const User *u = &it->second;
			setUserSessionCookies(response, *u); // Usa helper consistente
			return response;					 // salir temprano
		}
	}
	response.buildCustomResponse("512", "Error", "Invalid usser or pasword.");
	return (response);
}

void MethodHandler::_handleAddAcces(Client &client, Response &response)
{
	const Request &request = client.getRequest();
	std::map<std::string, std::string> claveValor = parseFormURLEncoded(request.getBody());
    std::map<unsigned int, User> &users = client.getServer().getRegisteredUsersRefNoConst();
	std::string username = getRequiredField(claveValor, "username");
	for (std::map<unsigned int, User>::iterator it = users.begin();
		it != users.end(); ++it)
	{
		if (it->second.getName() == username)
		{
			it->second.incriseTimesConected();
			response.buildCustomResponse("204", "No Content", "");
			{
				std::ostringstream ac;
				ac << it->second.getTimesConected();
				response.addHeader("X-Access-Count", ac.str());
			}
			const User *u = &it->second;
			setUserSessionCookies(response, *u); // Usa helper consistente
			return;
		}
	}
	response.buildCustomResponse("512", "Error", "Invalid usser or pasword.");
}