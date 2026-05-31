#include "../includes/Response.hpp"
#include "../includes/Server.hpp"
#include "../includes/Logger.hpp"

Response::Response(): _http_version("HTTP/1.1")
{
}

Response::~Response()
{
}

void	Response::setStatusCode(const std::string& code, const std::string& message)
{
	_status_code = code;
	_status_message = message;
}

void	Response::addHeader(const std::string& key, const std::string& value)
{
	_headers[key] = value;
}

void	Response::setBody(const std::string& body)
{
	_body = body;
	std::stringstream ss;
	ss << body.length();
	addHeader("Content-Length", ss.str());
}

std::string Response::toString() const
{
	std::stringstream response_stream;

	// Status line
	response_stream << _http_version << " " << _status_code << " " << _status_message << "\r\n";

	// Headers
	for (std::map<std::string, std::string>::const_iterator it = _headers.begin(); it != _headers.end(); ++it)
	{
		response_stream << it->first << ": " << it->second << "\r\n";
	}

	// Separador
	response_stream << "\r\n";

	// Body
	response_stream << _body;

	return (response_stream.str());
}

void Response::buildSimpleResponse(const std::string& code, const std::string& message)
{
	setStatusCode(code, message);

	std::stringstream body_stream;
	body_stream << "<html><head><title>" << code << " " << message << "</title></head>"
		<< "<body><h1>" << code << " " << message << "</h1></body></html>";

	setBody(body_stream.str());

	addHeader("Content-Type", "text/html");
}

void Response::buildCustomResponse(const std::string& code, const std::string& message, const std::string& body)
{
	setStatusCode(code, message);
	setBody(body);
}

void Response::buildRedirectResponse(int code, const std::string& location_url)
{
	std::string message = "Moved Permanently";
	std::stringstream ss;
	ss << code;
	setStatusCode(ss.str(), message);
	addHeader("Location", location_url);
	setBody("<html><body><h1>" + ss.str() + " " + message + "</h1></body></html>");
}

// Función auxiliar para leer archivos (puedes mover la que tienes en MethodHandler aquí)
static std::string readFile(const std::string& path)
{
	std::ifstream file(path.c_str());
	if (!file.is_open())
		return ("");
	std::stringstream buffer;
	buffer << file.rdbuf();
	return (buffer.str());
}

// Mapa de mensajes de estado estandar
static std::map<int, std::string> getStatusMessages()
{
	std::map<int, std::string> messages;
	messages[201] = "Created";
	messages[204] = "No Content";
	messages[301] = "Moved Permanently";
	messages[400] = "Bad Request";
	messages[403] = "Forbidden";
	messages[404] = "Not Found";
	messages[405] = "Method Not Allowed";
	messages[409] = "Conflict";
	messages[413] = "Payload Too Large";

	messages[500] = "Internal Server Error";
	messages[501] = "Not Implemented";

	// ... añade mas si los necesito
	return (messages);
}

void Response::buildErrorResponse(int error_code, const ServerConfig& config, const LocationConfig* location)
{
    std::map<int, std::string> status_messages = getStatusMessages();
    std::string message = "Error";
    if (status_messages.count(error_code))
        message = status_messages.at(error_code);

    std::string body;
    std::string error_page_path;
    // Por defecto, usamos el root del servidor
    std::string root_dir = config.root_directory;

    // 1. Primero, buscar en la 'location'
    if (location && location->error_pages.count(error_code))
    {
        error_page_path = location->error_pages.at(error_code);
        // El root de la location tiene prioridad si existe
        if (!location->root_directory.empty())
            root_dir = location->root_directory;
    }
    // 2. Si no, buscar en la configuración del servidor
    else if (config.error_pages.count(error_code))
    {
        error_page_path = config.error_pages.at(error_code);
    }

    if (!error_page_path.empty())
    {
        std::string full_path = root_dir;
        if (!full_path.empty() && full_path.length() != '/')
            full_path += '/';
		if (!error_page_path.empty() && error_page_path[0] == '/')
			error_page_path.erase(0, 1);
        full_path += error_page_path;
        body = readFile(full_path);
    }

    // 3. Generar respuesta (el resto de la función es igual)
    if (body.empty())
    {
        std::stringstream ss;
        ss << error_code;
        buildSimpleResponse(ss.str(), message);
    }
    else
    {
        std::stringstream ss;
        ss << error_code;
        buildCustomResponse(ss.str(), message, body);
        addHeader("Content-Type", "text/html");
    }
}
