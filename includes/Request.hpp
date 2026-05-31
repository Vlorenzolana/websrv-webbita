#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <string>
#include <iostream>
#include <sstream>
#include <iostream>
#include <map>
#include <vector>

class Request
{
	public:
		typedef std::map<std::string, std::string> HeaderMap;

	private:
		std::string _method; //ej: "GET", "POST", "DELETE"
		std::string _path; //ej: "/index.html", "/image/logo.jpg"
		std::string _query_string;
		std::string _full_path;
		std::string _http_version; //ej: "HTTP/1.1"
		HeaderMap	_headers; // Un mapa para guardar todas las cabeceras
		std::string _body;
		bool	_is_parsed;
		int	_error_code;
		bool	_isCGI;

		bool	_parseRequestLine(const std::string& line);
		bool	_parseHeaderLine(const std::string& line);

	public:
		Request();
		~Request();

		// El "parse" simulado. Recibe el buffer solo para mantener la estructura.
		bool parse(const std::string& raw_request);

		// getters
		const std::string&	getMethod() const;
		const std::string&	getPath() const;
		const std::string&	getQueryString() const;
		const std::string&	getHttpVersion() const;
		const HeaderMap&	getHeaders() const;
		const std::string&	getBody() const;
		const std::string&	getFullPath() const;
		bool			getIsCGI() const;

		// seters
		void		setBody(const std::string& body);
		void		setFullPath(const std::string& full_path);
		bool		isParsed() const;
		int		getErrorCode() const;

		std::string getHeaderValue(const std::string& header_name) const;

};

#endif // REQUEST_HPP
