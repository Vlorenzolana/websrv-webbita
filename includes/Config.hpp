#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>
#include <map>

struct LocationConfig
{
    // Ruta del location, por ejemplo `/`, `/cgi` o `/upload`.
    std::string path;
    // Métodos HTTP permitidos dentro de este bloque `location`.
    std::vector<std::string> allowed_methods;
    // Activa o desactiva el listado de directorios.
    bool autoindex;
    // Ficheros índice que se prueban cuando la ruta apunta a un directorio.
    std::vector<std::string> index_files;
    // Código de redirección, por ejemplo 301 o 302.
    int return_code;
    // URL de destino de la redirección.
    std::string return_url;
    // Root específico del location. Si está vacío, hereda del server.
    std::string root_directory;
    // Ruta donde se guardan uploads si este location los soporta.
    std::string upload_path;
    // Mapa de páginas de error personalizadas por código HTTP.
    std::map<int, std::string> error_pages;

    LocationConfig() : 
        autoindex(false), 
        return_code(0) 
    {}
};

struct ServerConfig
{
    // Nombre lógico del virtual host.
    std::string server_name;
    // Puerto de escucha del server.
    int port;
    // Root principal del server.
    std::string root_directory;
    // Opciones de reutilización del socket.
    bool enable_reuse_addr;
    // Opción global de autoindex.
    bool autoindex;
    // Límite máximo del body permitido para requests.
    long long client_max_body_size;
    // Métodos globales permitidos si no se definen en un location.
    std::vector<std::string> allowed_methods;
    // Páginas de error personalizadas del server.
    std::map<int, std::string> error_pages;
    // Lista de bloques location asociados a este server.
    std::vector<LocationConfig> locations;

    ServerConfig() : 
        port(8080),
        enable_reuse_addr(true),
        autoindex(false), 
        client_max_body_size(1048576)
    {}
};

#endif