#include "../includes/Config.hpp"


/* Creates a default location for the server if none is specified in the configuration file */
bool	CreateDefaultLocation(ServerConfig& new_server)
{
	LocationConfig 	current_location;
	
	current_location.path = "/";
	current_location.allowed_methods.push_back("GET");
	current_location.allowed_methods.push_back("POST");
	current_location.autoindex = true;
	current_location.root_directory = "./www/";
	current_location.index_files.push_back("index.html");

	new_server.locations.push_back(current_location);
	if (new_server.locations.empty())
	{
		std::cout << "Error: Could not create default location" << std::endl;
		return false;
	}
	return true;
}

/*	Extracts the next valid token from the raw config data (C++98 Safe).
 *	returns: Next token as a std::string. Empty string when the end is reached. */
std::string GetNextToken(const std::string& raw_data, size_t& index)
{
	const size_t		datalen = raw_data.length();
	const std::string	delimiters = " \t\n\r{};#";
	size_t				start;
	size_t				end;
	char				next_char;

	while (index < datalen)												//	1.- SKIPPING LOOP: Skip all blanks, tabs, newlines, carriage returns.
	{												
		while (index < datalen && (raw_data[index] == ' ' || raw_data[index] == '\t' ||  raw_data[index] == '\n' || raw_data[index] == '\r'))	
			index++;
		if (raw_data[index] == '#')										//	Skip comments to end of line
		{
			while (index < datalen && raw_data[index] != '\n')			//	When newline, restart outer loop.
				index++;
			continue; 
		}
		if (index < datalen)											//	If non-separator, break skipping loop.
			break;
	}
	if (index >= datalen)												//	Check if EOF after skipping separators
		return "";
	next_char = raw_data[index];										//	2.- EXTRACT SINGLE CHAR: Delimiter tokens
	if (next_char == '{' || next_char == '}' || next_char == ';')
	{
		index++; 														//	Advance, 
		return std::string(1, next_char);								//	then create and return a single-char string (C++98 style).
	}
	start = index;														//	3. EXTRACT MULTI-CHAR TOKENS: Directives or Values
	end = raw_data.find_first_of(delimiters, index);					//	Find position of first delimiter starting from 'index'. If none found, npos.
	if (end != std::string::npos)										//	If delimiter, token ends there.
		index = end; 													//	Update 'index' to delimiter
	else																//	If no delimiter (npos), rest of the string is the token.
		index = datalen;
	return raw_data.substr(start, index - start);						// Extract and return the token using substr (len = index - start)
}

/*	This function uses standard C++ file streams to read the file contents.
	Return: whole file as a std::string. Empty string on failure. */
std::string ReadFileToString(const std::string& filename)
{
	std::stringstream	buffer;
	std::string			contents;
	
	std::ifstream file(filename.c_str(), std::ios::in);					//	.c_str() extracts data from filename as a stream. Guarantees null termination.
	if (!file.is_open())
	{
		return "";
	}
	buffer << file.rdbuf();												//	Store the whole stream in a buffer...
	file.close();														//	close the file to avoid leaking...
	contents = buffer.str();											//	then convert the buffer to a string...
	return contents;													//	and simply return it.
}
