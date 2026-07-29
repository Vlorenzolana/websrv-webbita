#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <map>
#include <string>
#include <vector>

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
    std::map<std::string, std::string> cgi_interpreters;

    LocationConfig()
        : autoindex(false), return_code(0)
    {
    }
};

struct ServerConfig
{
    std::string host;
    std::string server_name;
    int port;
    std::string root_directory;
    long long client_max_body_size;
    std::map<int, std::string> error_pages;
    std::vector<LocationConfig> locations;

    ServerConfig()
        : host("0.0.0.0"), port(8080), client_max_body_size(1048576)
    {
    }
};

#endif
