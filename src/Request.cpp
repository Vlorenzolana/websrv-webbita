#include "../includes/Request.hpp"

// --- Metodos privados auxiliares --- 

// Parseo de la linea de peticion ej: "GET /index.html HTTP/1.1"
bool	Request::_parseRequestLine(const std::string& line)
{
	std::stringstream ss(line);
	std::string	full_path;

	if (!(ss >> _method))
		return (false);
	if (!(ss >> full_path))
		return (false);
	if (!(ss >> _http_version))
		return (false);

	size_t query_pos = full_path.find('?');
	if (query_pos != std::string::npos)
	{
		_path = full_path.substr(0, query_pos);
		_query_string = full_path.substr(query_pos + 1);
	}
	else
	{
		_path = full_path;
	}
	if (_path.find("/cgi-bin/") != std::string::npos)
		this->_isCGI = true;
	return (true);
}

// Parseo de unica linea de cabecera ej: "Host: localhost"
bool	Request::_parseHeaderLine(const std::string& line)
{
	std::stringstream ss(line);
	std::string key, value;

	if (!std::getline(ss, key, ':'))
		return (false);
	if (!std::getline(ss, value))
		return (false);

	size_t	first_char = value.find_first_not_of(" \t");
	if (first_char != std::string::npos)
		value = value.substr(first_char);
	_headers[key] = value;

	return (true);
}

// --- Metodos publicos ---

Request::Request(): _is_parsed(false), _error_code(0) , _isCGI(false)
{
//	std::cout << "Request constructor" << std::endl;
}

Request::~Request()
{
//	std::cout << "Request destructor" << std::endl;
}


bool	Request::parse(const std::string& raw_request)
{
	std::stringstream request_stream(raw_request);
	std::string line;

	if (std::getline(request_stream, line))
	{
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);

		if (!_parseRequestLine(line))
		{
			_error_code = 400; //Bad Request
			return (false);
		}
	}
	else
	{
		_error_code = 400;
		return (false);
	}
	while (std::getline(request_stream, line))
	{
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);
		if (line.empty())
			break;
		if (!_parseHeaderLine(line))
		{
			_error_code = 400;
			return (false);
		}
	}
//	if (request_stream.good())
//	{
//		std::stringstream body_stream;
//		body_stream << request_stream.rdbuf();
//		_body = body_stream.str();
//	}

	_is_parsed = true;
	return (true);
}

// --- Seters ---

void	Request::setBody(const std::string& body)
{
	_body = body;
}

void	Request::setFullPath(const std::string& path)
{
	_full_path = path;
}

// --- Geters ---

const std::string& Request::getFullPath() const
{
	return (_full_path);
}

const std::string& Request::getMethod() const
{
	return (_method);
}

const std::string& Request::getPath() const
{
	return (_path);
}

const std::string& Request::getQueryString() const
{
	return (_query_string);
}

const std::string& Request::getHttpVersion() const
{
	return (_http_version);
}

const Request::HeaderMap& Request::getHeaders() const
{
	return (_headers);
}

bool	Request::isParsed() const
{
	return (_is_parsed);
}

int	Request::getErrorCode() const
{
	return (_error_code);
}

const std::string& Request::getBody() const
{
	return (_body);
}

std::string Request::getHeaderValue(const std::string& header_name) const
{
	HeaderMap::const_iterator it = _headers.find(header_name);
	if (it != _headers.end())
		return (it->second);
	return ("");
}

bool Request::getIsCGI() const
{
	return (_isCGI);
}
