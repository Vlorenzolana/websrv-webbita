#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>
#include <map>

struct LocationConfig
{
    std::string path;
    std::vector<std::string> allowed_methods;
    bool autoindex;
    std::vector<std::string> index_files;
    int return_code;
    std::string return_url;
    std::string root_directory;
    std::string upload_path;
    std::map<int, std::string> error_pages;

    LocationConfig() : 
        autoindex(false), 
        return_code(0) 
    {}
};

struct ServerConfig
{
    std::string server_name;
    int port;
    std::string root_directory;
    bool enable_reuse_addr;
    bool autoindex;
    long long client_max_body_size;
    std::vector<std::string> allowed_methods;
    std::map<int, std::string> error_pages;
    std::vector<LocationConfig> locations;

    ServerConfig() : 
        port(8080),
        enable_reuse_addr(true),
        autoindex(false), 
        client_max_body_size(1048576)
    {}
};

#endif