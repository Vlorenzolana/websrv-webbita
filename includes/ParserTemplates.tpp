//	TEMPLATE FUNCTIONS FOR PARSING DIRECTIVES SHARED BOTH BY SERVER AND LOCATION

#include <string>

template <typename ConfigT>
bool	ParseErrorPageDirectiveT(const std::string& content, size_t& index, ConfigT& config_struct)
{
	std::string	code_token;
	std::string	path_token;
	std::string	semicolon;
	int			error_code;

	code_token = GetNextToken(content, index);								//	Expect error code.
	if (code_token.empty())
	{
		std::cerr << "Error: Unexpected EOF after 'error_page' directive." << std::endl;
		return false;
	}
	std::istringstream code_ss(code_token);									// Convert code string to int
	code_ss >> error_code;
	if (code_ss.fail() || error_code < 300 || error_code > 599)				// Validation: Check conversion and range.
	{
		std::cerr << "Error: Invalid or out-of-range error code '" << code_token << "'." << std::endl;
		return false;
	}
	path_token = GetNextToken(content, index);								//	Expect path.
	if (path_token.empty())													//	Validation: EOF
	{
		std::cerr << "Error: Unexpected EOF after error code (missing path)." << std::endl;
		return false;
	}
	if (path_token == ";")													//	Validation: Path is not ';'.
	{
		std::cerr << "Error: 'error_page' path is missing." << std::endl;
		return false;
	}
	if (config_struct.error_pages.find(error_code) != config_struct.error_pages.end())	//	Validation: Method .find() from map container to check double entries
	{
		std::cerr << "Error: Duplicate 'error_page' entry for code " << error_code << "." << std::endl;
		return false;
	}
	semicolon = GetNextToken(content, index);								//	Expect ';'
	if (semicolon != ";")
	{
		std::cerr << "Error: Expected ';' after error path, found '" << semicolon << "'" << std::endl;
		return false;
	}
	config_struct.error_pages[error_code] = path_token;							//	All OK. Populate map.
	return true;
}

template <typename ConfigT>
bool	ParseAutoindexDirectiveT(const std::string& content, size_t& index, ConfigT& config_struct)
{
	std::string	token;

	token = GetNextToken(content, index);
	if (token.empty())
	{
		std::cerr << "Error: Unexpected EOF after 'autoindex' directive" << std::endl;
		return false;
	}
	if (token == "on")
		config_struct.autoindex = true;
	else if(token == "off")
		config_struct.autoindex = false;
	else
	{
		std::cerr << "Error: Expected 'on' or 'off' after 'autoindex' directive. Found '" << token << "'" << std::endl;
		return false;
	}
	token = GetNextToken(content, index);
	if (token != ";")
	{
		std::cerr << "Error: Expected ';' after 'autoindex' value, found '" << token << "'" << std::endl;
		return false;
	}
	return true;	
}

template <typename ConfigT>
bool	ParseRootDirectiveT(const std::string& content, size_t& index, ConfigT& config_struct)
{
	std::string	token;

	token = GetNextToken(content, index);
	if (token.empty())													//	Validation: Check if EOF.
	{
		std::cerr << "Error: Unexpected EOF after 'root' directive." << std::endl;
		return false;
	}
	if (!config_struct.root_directory.empty())									//	Validation: Check if root already set.
	{
		std::cerr << "Error: Duplicate 'root' directive." << std::endl;
		return false;
	}
	config_struct.root_directory = token;										//	Populate struct. No need for conversion since 'token' is already a string.
	token = GetNextToken(content, index);								//	Expect ';'
	if (token != ";")
	{
		std::cerr << "Error: Expected ';' after 'root' value, found '" << token << "'" << std::endl;
		return false;
	}
	return true;
}

template <typename ConfigT>
bool	ParseAllowedMethodsDirectiveT(const std::string& content, size_t& index, ConfigT& config_struct)
{
	std::string token;

	if (!config_struct.allowed_methods.empty())											//	Validation. "One-and-done"
	{
		std::cerr << "Error: Duplicate 'allowed_methods' directive found." << std::endl;
		return false;
	}
	while (!(token = GetNextToken(content, index)).empty())								//	Loop untill ';'
	{
		if (token == ";")																//	If end of directive...
		{
			if (config_struct.allowed_methods.empty())										//	... at least one valid method must be present
			{
				std::cerr << "Error: 'allowed_methods' directive requires at least one verb." << std::endl;
				return false;
			}
			return true;
		}
		config_struct.allowed_methods.push_back(token);									//	Store valid method into the vector.
	}
	std::cerr << "Error: Unexpected EOF while parsing 'allowed_methods' directive (missing ';'?)." << std::endl;
	return false;
}