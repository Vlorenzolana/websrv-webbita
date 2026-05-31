#include "../includes/Config.hpp"

/*	Parses the 'reuse_addr on|off;' directive and sets the boolean flag. By default it is 'off'. */
bool Config::ParseReuseAddrDirective(const std::string& content, size_t& index, ServerConfig& server)
{
	std::string token;

	token = GetNextToken(content, index);
	if (token.empty())																//	Expect value.
	{
		std::cerr << "Error: Unexpected EOF after 'reuse_addr' directive." << std::endl;
		return false;
	}
	if (token == "on")																// Validate and assign.
		server.enable_reuse_addr = true;
	else if(token == "off")
		server.enable_reuse_addr = false;
	else
	{
		std::cerr << "Error: Expected 'on' or 'off' after 'reuse_addr'. Found '" << token << "'" << std::endl;
		return false;
	}
	token = GetNextToken(content, index);
	if (token != ";")																//	Expect ';'
	{
		std::cerr << "Error: Expected ';' after 'reuse_addr' value, found '" << token << "'" << std::endl;
		return false;
	}
	return true;    
}

/*	Parses 'client_max_body_size [long long bytes]; */
bool Config::ParseClientMaxBodySizeDirective(const std::string& content, size_t& index, ServerConfig& server)
{
	std::string token;
	long long value = 0;

	if (server.client_max_body_size != 1048576)						//	Validation: Check duplicates against the constructor default.
	{
		std::cerr << "Error: Duplicate 'client_max_body_size' directive found." << std::endl;
		return false;
	}
	token = GetNextToken(content, index);							//	Expect value
	if (token.empty())
	{
		std::cerr << "Error: Unexpected EOF after 'client_max_body_size' directive." << std::endl;
		return false;
	}
	std::istringstream ss(token);   								//	Convert to long long using stringstream
	ss >> value;
	if (ss.fail() || value < 0)										//	Validation: Check conversion or negative
	{
		std::cerr << "Error: Invalid size value '" << token << "' in client_max_body_size." << std::endl;
		return false;
	}
	server.client_max_body_size = value;							//	Populate
	token = GetNextToken(content, index);							//	Expect ';'
	if (token != ";")
	{
		std::cerr << "Error: Expected ';' after size value, found '" << token << "'" << std::endl;
		return false;
	}
	return true;
}

bool	Config::ParseServerNameDirective(const std::string& content, size_t& index, ServerConfig& server)
{
	std::string	token;

	token = GetNextToken(content, index);								//	Expect server name.
	if (token.empty())													//	Validation: Check if empty.
	{
		std::cerr << "Error: Unexpected EOF after 'server_name' directive." << std::endl;
		return false;
	}
	if (!server.server_name.empty())
	{
		std::cerr << "Error: Duplicate server_name directive" << std::endl;
		return false;
	}
	server.server_name = token;											//	Populate. Token is already a std::string
	token = GetNextToken(content, index);
	if (token != ";")													//	Expect ';'
	{
		std::cerr << "Error: Expected ';' after 'server_name' value, found '" << token << "'" << std::endl;
		return false;
	}
	return true;
}

/*	Parses the 'listen <port>' directive and handles errors. */
bool Config::ParseListenDirective(const std::string& content, size_t& index, ServerConfig& server)
{
	std::string	token;
	int			temp_port;

	token = GetNextToken(content, index);								//	Expect value
	if (token.empty())													//	Validation: Check if empty.
	{
		std::cerr << "Error: Unexpected EOF after 'listen' directive." << std::endl;
		return false;
	}
	if (server.port != 0) 												//	Validation: Check if port is already set.
	{
		std::cerr << "Error: Duplicate 'listen' directive." << std::endl;
		return false;
	}
	std::istringstream ss(token);										//	String to int using stringstream
	ss >> temp_port;
	if (ss.fail() || temp_port <= 0 || temp_port > 65535)				//	Validation: Conversion and range (0-65535)
	{
		std::cerr << "Error: Invalid port number '" << token << "' in listen directive." << std::endl;
		return false;
	}
	server.port = temp_port;											//	Populate struct
	token = GetNextToken(content, index);								//	Expect ';'
	if (token != ";")
	{
		std::cerr << "Error: Expected ';' after 'listen' value, found '" << token << "'" << std::endl;
		return false;
	}
	return true;
}