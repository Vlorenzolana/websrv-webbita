#include "../includes/Config.hpp"

Config::Config(){}

Config::~Config(){}

const std::vector<ServerConfig>& Config::getServerConfigs() const
{
	return (_server_configs);
}

/*	Parses a single 'location path { ... }' block. */
bool Config::ParseLocationBlock(const std::string& content, size_t& index, ServerConfig& server)
{
	LocationConfig 	current_location;
	std::string		token;

	token = GetNextToken(content, index);						//	Expect path.
	if (token.empty() || token == "{")
	{
		std::cerr << "Error: Expected path after 'location' directive." << std::endl;
		return (false);
	}
	current_location.path = token;								//	Store path
	token = GetNextToken(content, index);						//	Expect '{'
	if (token != "{")
	{
		std::cerr << "Error: Expected '{' after location path, found '" << token << "'" << std::endl;
		return (false);
	}
	while (!(token = GetNextToken(content, index)).empty())		//	Parse directives untill '}' is found.
	{
		if (token == "}")										//	If '}', break the loop.
			break; 
		else if (token == "root")
		{
			if (!ParseRootDirectiveT(content, index, current_location))				//	Template
				return (false);
		}
		else if (token == "autoindex")
		{
			if (!ParseAutoindexDirectiveT(content, index, current_location))		//	Template
				return (false);
		}
		else if (token == "allowed_methods")
		{
			if (!ParseAllowedMethodsDirectiveT(content, index, current_location))	//	Template
			return (false);
		}
		else if (token == "return")
		{
			if (!ParseReturnCodeDirective(content, index, current_location))		//	Only location
				return (false);			
		}
		else if (token == "index")
		{
			if (!ParseIndexDirective(content, index, current_location))				//	Only location
				return (false);
		}
		else if (token == "upload_path")											//	Template
		{
			if (!ParseUploadPathDirective(content, index, current_location))
				return false;
		}
		else if (token == "error_page")
		{
			if (!ParseErrorPageDirectiveT(content, index, current_location))
				return false;
		}
		
		else
		{
			std::cerr << "Error: Unknown directive '" << token << "' inside location block." << std::endl;
			return (false);
		}
	}
	if (token.empty())
	{
		std::cerr << "Error: Unexpected end of file while parsing location block (missing '}')" << std::endl;
		return (false);
	}

	for (size_t i = 0; i < server.locations.size(); ++i)
	{
		if (server.locations[i].path == current_location.path)
		{
			std::cerr << "Error: Duplicate location path '" << current_location.path << "' found in server block." << std::endl;
			return (false);
		}
	}
	server.locations.push_back(current_location);				// If all is OK, attach location block to parent server
	return (true);
}

bool	Config::ParseServerBlock(const std::string& content, size_t& index)
{
	ServerConfig	new_server;								//	Instantiate a new ServerConfig struct to be filled and added to class Config.
	std::string		token;

	token = GetNextToken(content, index);					//	Expect opening brace.
	if (token != "{")
	{
		std::cerr << "Error: expected '{' token after 'server'. Found " << token << std::endl;
		return (false);
	}
	while (!(token = GetNextToken(content, index)).empty())	//	Loop untill closing brace.
	{
		if (token == "}")
			break;
		else if (token == "location")
		{
			if (!ParseLocationBlock(content, index, new_server))		// NESTED BLOCK: Dedicated function to handle 'location { ... }'
			return false;
		}
		else if (token == "server_name")
		{
			if (!ParseServerNameDirective(content, index, new_server))		
				return false;
		}
		else if (token == "listen")
		{
			if (!ParseListenDirective(content, index, new_server))		//	Only server
				return (false);
		}
		else if (token == "root")
		{
			if (!ParseRootDirectiveT(content, index, new_server))		// Template
				return (false);
		}
		else if (token == "error_page")
		{
			if (!ParseErrorPageDirectiveT(content, index, new_server))	//	Template
				return false;
		}
		else if (token == "autoindex")
		{
			if (!ParseAutoindexDirectiveT(content, index, new_server))	//	Template
				return (false);
		}
		else if (token == "enable_reuse_addr")
		{
			if (!ParseReuseAddrDirective(content, index, new_server))	//	Only server
				return (false);
		}
		else if (token == "client_max_body_size")
		{
			if (!ParseClientMaxBodySizeDirective(content, index, new_server))	//	Template
				return (false);
		}
		else if (token == "allowed_methods")
		{
			if (!ParseAllowedMethodsDirectiveT(content, index, new_server))
				return (false);
		}
		else
		{
			std::cerr << "Error: Unknown directive " << token << " found inside server block" << std::endl;
			return false;
		}
	}
	bool root_location_exists = false;
	for (size_t i = 0; i < new_server.locations.size(); ++i)
	{
		if (new_server.locations[i].path == "/")
		{
			root_location_exists = true;
			break ;
		}
	}
	if (!root_location_exists)
	{
		if (!CreateDefaultLocation(new_server))
			return (false);
	}

	if (new_server.port == 0)
	{
		std::cerr << "Error: The 'listen' directive is mandatory in the server block" << std::endl;
		return (false);
	}

	if (new_server.root_directory.empty())
	{
		std::cerr << "Error: The 'root' directive is mandatory in the server block" << std::endl;
		return (false);
	}

	_server_configs.push_back(new_server);		//	If all is OK, add new_server to configurations vector in main Config class.
	return (true);
}

bool	Config::parse(const char* filename)
{
	std::string	content;
	std::string token;
	size_t		index = 0;

	content = ReadFileToString(filename);
	if (content == "")
		return (false);
	while (!(token = GetNextToken(content, index)).empty())
	{
		if (token == "server")
		{
			if (!ParseServerBlock(content, index))
				return (false);
		}
		else 
		{
			std::cerr << "Error: Unexpected token '" << token << "' at top level." << std::endl;
			return (false);
		}
	}
	return (true);
}
