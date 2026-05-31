#include "../includes/Config.hpp"

// Por ahora, solo establece un puerto por defecto para que el servidor arranque
/* bool Config::parse(const char* filename)
{
	(void)filename;
	ServerConfig defaultConfig;
	defaultConfig.port = 8080;
	defaultConfig.root_directory = "./www";
	defaultConfig.error_pages[404] = "./www/404.html";
	defaultConfig.error_pages[409] = "./www/409.html";
	defaultConfig.enable_reuse_addr = 1;
	defaultConfig.client_max_body_size = 1048576; // 1MB en bytes
	defaultConfig.allowed_methods.push_back("GET");

	// --- Test redirection ---
	LocationConfig redirect_location;
	redirect_location.path = "/pagina-antigua";
	redirect_location.root_directory = "./www";
	redirect_location.return_code = 301;
	redirect_location.return_url = "https://www.youtube.com";

	// --- Hardcodeado las locations para probarlo ---
	LocationConfig root_location;
	root_location.path = "/";
	root_location.root_directory = "./www";
	root_location.allowed_methods.push_back("GET");
	root_location.allowed_methods.push_back("POST"); //Necesario para hacer que fincione el loging de usuariso
	root_location.autoindex = true;
	root_location.return_code = 0;
	root_location.index_files.push_back("index.html");

	LocationConfig uploads_location;
	uploads_location.path = "/uploads";
	uploads_location.root_directory = "./www";
	uploads_location.autoindex = true;
	uploads_location.allowed_methods.push_back("GET");
	uploads_location.allowed_methods.push_back("POST");
	uploads_location.index_files.push_back("index.html");
	uploads_location.index_files.push_back("index2.html");
	uploads_location.upload_path = "./www/uploads";

	defaultConfig.locations.push_back(root_location);
	defaultConfig.locations.push_back(uploads_location);
	defaultConfig.locations.push_back(redirect_location);

	_server_configs.push_back(defaultConfig);

	// NUEVO SERVIDOR
	ServerConfig server2_config;
	server2_config.port = 8081;
	server2_config.root_directory = "./www_server2";
	server2_config.error_pages[404] = "./www/404.html";
	server2_config.enable_reuse_addr = 1;
	server2_config.client_max_body_size = 1048576;
	server2_config.allowed_methods.push_back("GET");
	server2_config.allowed_methods.push_back("POST");

	LocationConfig server2_root;
	server2_root.path = "/";
	server2_root.root_directory = "./www_server2";
	server2_root.allowed_methods.push_back("GET");
	server2_root.allowed_methods.push_back("POST");
	server2_root.autoindex = true;
	server2_root.index_files.push_back("index.html");

	LocationConfig server2_uploads;
	server2_uploads.path = "/uploads";
	server2_uploads.root_directory = "/www_server2";
	server2_uploads.allowed_methods.push_back("POST");
	server2_uploads.allowed_methods.push_back("GET");
	server2_uploads.upload_path = "./www_server2/uploads";

	server2_config.locations.push_back(server2_root);
	server2_config.locations.push_back(server2_uploads);
	
//	_server_configs.push_back(server2_config);
	return (true);
} */


//int Config::getPort() const
//{
//	return (_port);
//}
//
//const std::string& Config::getRootDirectory() const
//{
//	return (_root_directory);
//}
//
//const std::string& Config::getErrorPage404() const
//{
//	return (_error_page_404);
//}
//
//bool Config::getAutoindex() const
//{
//	return (_autoindex);
//}
//
//int	Config::getReuseAddr() const
//{
//	return (_enable_reuse_addr);
//}
