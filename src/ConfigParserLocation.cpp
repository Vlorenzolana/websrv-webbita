#include "../includes/Config.hpp"

/*	Parses the 'index <file1> <file2> ... ;' directive. */
bool Config::ParseIndexDirective(const std::string& content, size_t& index, LocationConfig& current_location)
{
	std::string token;

	if (!current_location.index_files.empty())					//	Validation: One-and-done
	{
		std::cerr << "Error: Duplicate 'index' directive found in location block." << std::endl;
		return false;
	}
	while (!(token = GetNextToken(content, index)).empty())		//	Loop untill ';'
	{
		if (token == ";")
		{
			if (current_location.index_files.empty())			//	At end, must not be empty.
			{
				std::cerr << "Error: 'index' directive requires at least one file name." << std::endl;
				return false;
			}
			return true;										//	If end, and valid token previously stored, it is OK.
		}
		if (token == "{" || token == "}")
		{
			std::cerr << "Error: Unexpected block delimiter '" << token << "' inside 'index' directive." << std::endl;
			return false;
		}
		current_location.index_files.push_back(token);
	}
	std::cerr << "Error: Unexpected EOF while parsing 'index' directive (missing ';'?)." << std::endl;   	//	If loop exited because GetNextToken returned empty, it means EOF.
	return false;
}

/*	Parses the 'return <CODE> <URL>;' directive and stores the redirect information. */
bool Config::ParseReturnCodeDirective(const std::string& content, size_t& index, LocationConfig& current_location)
{
	std::string	token;
	int			code;

	if (current_location.return_code != 0)					//	Validation: No duplicates
	{
		std::cerr << "Error: Duplicate 'return' directive found in location block." << std::endl;
		return false;
	}
	token = GetNextToken(content, index);					 //	Expect code
	if (token.empty())
	{
		std::cerr << "Error: Unexpected EOF after 'return' directive (missing code)." << std::endl;
		return false;
	}
	std::istringstream ss(token);							//	Get into stream
	ss >> code;												//	Insert stream into int
	if (ss.fail() || code < 300 || code > 399)				//	Validation: Conversion and range
	{
		std::cerr << "Error: Invalid or out-of-range return code '" << token << "' (Expected 3xx code)." << std::endl;
		return false;
	}
	current_location.return_code = code;					//	Populate with code.
	token = GetNextToken(content, index);					//	Expect URL
	if (token.empty())
	{
		std::cerr << "Error: Unexpected EOF after return code (missing URL)." << std::endl;
		return false;
	}
	if (token == ";")
	{
		std::cerr << "Error: 'return' directive requires a URL after the status code." << std::endl;
		return false;
	}
	current_location.return_url = token;					//	Populate with URL
	token = GetNextToken(content, index);					//	Expect ';'
	if (token != ";")
	{
		std::cerr << "Error: Expected ';' after return URL, found '" << token << "'" << std::endl;
		return false;
	}
	return true;
}

/*	Parse the 'upload_path <path>;' directive. */
bool	Config::ParseUploadPathDirective(const std::string& content, size_t& index, LocationConfig& current_location)
{
	std::string	token;

	token = GetNextToken(content, index);				//	Expect path
	if (token.empty())
	{
		std::cerr << "Error: Unexpected EOF after 'upload_path' directive." << std::endl;
		return false;
	}
	if (!current_location.upload_path.empty())				//	One-and-done
	{
		std::cerr << "Error: Duplicate 'upload_path' directive." << std::endl;
		return false;
	}
	if (token == ";")									// Validation: Some path required
	{
		std::cerr << "Error: 'upload_path' directive requires a path." << std::endl;
		return false;
	}
	current_location.upload_path = token;					//	Populate
	token = GetNextToken(content, index);
	if (token != ";")									//	Expect ';'
	{
		std::cerr << "Error: Expected ';' after 'upload_path' path, found '" << token << "'" << std::endl;
		return false;
	}
	return true;
}