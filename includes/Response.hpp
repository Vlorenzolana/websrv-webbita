#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include <sstream>
#include <string>
#include <map>
#include "Config.hpp"

class Response
{
	private:
		std::string _http_version;
		std::string _status_code;
		std::string _status_message;
		std::map<std::string, std::string> _headers;
		std::string _body;

	public:
		Response();
		~Response();

		void setStatusCode(const std::string& code, const std::string& message);
		void addHeader(const std::string& key, const std::string& value);
		void setBody(const std::string& body);

		std::string toString() const;
		void buildSimpleResponse(const std::string& code, const std::string& message);
		void buildCustomResponse(const std::string& code, const std::string& message, const std::string& body);
		void buildErrorResponse(int error_code, const ServerConfig& config, const LocationConfig* location);
		void buildRedirectResponse(int code, const std::string& location_url);

};

#endif // RESPONSE_HPP
