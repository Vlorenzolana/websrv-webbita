#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <fstream>

struct	LocationConfig
{
	std::string					path;
	std::vector<std::string>	allowed_methods;
	bool						autoindex;
	std::vector<std::string>	index_files;
	int							return_code;
	std::string					return_url;
	std::string					root_directory;
	std::string					upload_path;
	std::map<int, std::string>	error_pages;

	LocationConfig(): autoindex(false), return_code(0) {}
};

struct	ServerConfig
{
	std::string					server_name;
	int							port;
	std::string					root_directory;
	bool						enable_reuse_addr;
	bool						autoindex;
	long long					client_max_body_size;
	std::vector<std::string>	allowed_methods;
	std::map<int, std::string>	error_pages;
	std::vector<LocationConfig>	locations;

	ServerConfig(): port(0), enable_reuse_addr(false), autoindex(false), client_max_body_size(1048576) {}
};

class	Config
{
	private:
		std::vector<ServerConfig>	_server_configs;
		std::map<int, ServerConfig>	_listener_configs;

		bool	ParseServerBlock(const std::string& content, size_t& index);
		bool	ParseLocationBlock(const std::string& content, size_t& index, ServerConfig& server);
		bool	ParseServerNameDirective(const std::string& content, size_t& index, ServerConfig& server);
		bool	ParseListenDirective(const std::string& content, size_t& index, ServerConfig& server);
		bool	ParseClientMaxBodySizeDirective(const std::string& content, size_t& index, ServerConfig& server);
		bool	ParseReuseAddrDirective(const std::string& content, size_t& index, ServerConfig& server);

		bool	ParseReturnCodeDirective(const std::string& content, size_t& index, LocationConfig& current_location);
		bool	ParseIndexDirective(const std::string& content, size_t& index, LocationConfig& current_location);
		bool	ParseUploadPathDirective(const std::string& content, size_t& index, LocationConfig& current_location);

	public:
		Config();
		~Config();
		
		bool	parse(const char* filename);
		const std::vector<ServerConfig>&	getServerConfigs() const;
};

std::string ReadFileToString(const std::string& filename);
std::string GetNextToken(const std::string& raw_data, size_t& index);
bool		CreateDefaultLocation(ServerConfig& new_server);

template <typename ConfigT>
bool	ParseRootDirectiveT(const std::string& content, size_t& index, ConfigT& config_struct);
template <typename ConfigT>
bool	ParseAutoindexDirectiveT(const std::string& content, size_t& index, ConfigT& config_struct);
template <typename ConfigT>
bool	ParseAllowedMethodsDirectiveT(const std::string& content, size_t& index, ConfigT& config_struct);
template <typename ConfigT>
bool	ParseErrorPageDirectiveT(const std::string& content, size_t& index, ConfigT& config_struct);

#include "ParserTemplates.tpp"

#endif
