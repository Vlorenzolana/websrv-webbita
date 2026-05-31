# Guía para Desarrollar un Webserver desde Cero hasta un MVP
Aquí tienes una guía paso a paso para montar un webserver desde cero hasta un MVP, con énfasis en la separación de las tareas de parseo. Puedes compartirla con tu compañero para que sepa cómo avanzar y qué debe implementar en cada etapa.

---

### 1. Estructura Inicial del Proyecto
- Crea carpetas separadas para:  
  - src (código fuente)
  - includes (headers)
  - config (archivos de configuración)
  - www (archivos estáticos)
- Define un archivo principal (`main.cpp`) que será el punto de entrada.

### 2. Objetivo del MVP
- El MVP debe aceptar conexiones HTTP, parsear solicitudes, servir archivos estáticos y responder correctamente a peticiones GET.
- El parseo de configuración y de solicitudes HTTP debe estar desacoplado del resto de la lógica.

---

### 3. Módulos Principales

#### a) Módulo de Configuración (Parser de Config)
- **Responsabilidad:** Leer y parsear el archivo de configuración del servidor (por ejemplo, webserv.conf).
- **Tareas:**
  - Leer el archivo línea por línea.
  - Validar la sintaxis y extraer parámetros (puerto, rutas, root, etc).
  - Guardar la configuración en una estructura de datos (clase Config).
- **Separación:**  
  - El parser de configuración debe estar en archivos separados (`ConfigParser.cpp/hpp`).
  - El resto del servidor solo accede a la configuración a través de la clase Config.

#### b) Módulo de Red (Server/Client)
- **Responsabilidad:** Escuchar en el puerto configurado, aceptar conexiones y leer datos del socket.
- **Tareas:**
  - Crear el socket, hacer bind y listen.
  - Aceptar conexiones entrantes.
  - Leer datos de los clientes y pasarlos al parser HTTP.

#### c) Módulo de Parseo de Solicitudes HTTP (Parser HTTP)
- **Responsabilidad:** Parsear la solicitud HTTP recibida.
- **Tareas:**
  - Separar la línea de petición, headers y body.
  - Validar el método, la ruta y los headers.
  - Guardar la información en una estructura (`Request`).
- **Separación:**  
  - El parser HTTP debe estar en archivos separados (`Request.cpp/hpp`).
  - El resto del servidor solo accede a la solicitud parseada a través de la clase `Request`.

#### d) Módulo de Manejo de Solicitudes (RequestHandler)
- **Responsabilidad:** Procesar la solicitud parseada y generar una respuesta.
- **Tareas:**
  - Determinar si la ruta existe y es accesible.
  - Leer archivos estáticos desde www.
  - Generar la respuesta HTTP adecuada (200, 404, etc).

#### e) Módulo de Respuesta (Response)
- **Responsabilidad:** Construir la respuesta HTTP.
- **Tareas:**
  - Formatear el status line, headers y body.
  - Enviar la respuesta al cliente.

---

### 4. Flujo General

1. **Arranque:**  
   - El servidor lee la configuración usando el parser de config.
2. **Escucha:**  
   - El servidor escucha en el puerto configurado.
3. **Conexión:**  
   - Al recibir una conexión, lee la solicitud.
4. **Parseo:**  
   - El parser HTTP procesa la solicitud y la convierte en un objeto `Request`.
5. **Manejo:**  
   - El `RequestHandler` decide qué hacer (servir archivo, error, etc).
6. **Respuesta:**  
   - El módulo de respuesta construye y envía la respuesta.

---

### 5. Consejos para Separar el Parseo

- **Nunca mezcles lógica de parseo con lógica de negocio.**
- El parser de configuración solo debe encargarse de transformar texto en estructuras de datos.
- El parser HTTP solo debe encargarse de transformar la cadena recibida en un objeto `Request`.
- El resto del servidor debe trabajar solo con objetos ya parseados.

---

### 6. Siguientes Pasos para tu Compañero

1. Implementar el parser de configuración y probarlo con archivos de ejemplo.
2. Implementar el parser HTTP y probarlo con solicitudes simples.
3. Montar el servidor básico que acepte conexiones y use ambos parsers.
4. Implementar el manejo de solicitudes y la generación de respuestas.
5. Probar el MVP sirviendo archivos estáticos y respondiendo a errores.

---

## Estructura, módulos y clases

## 1. Estructura Inicial del Proyecto
- `src/` — Código fuente (implementaciones)
- `includes/` — Headers (declaraciones de clases)
- `config/` — Archivos de configuración
- `www/` — Archivos estáticos

---

## 2. Módulos Principales y Ejemplos de Código

### a) Parser de Configuración
**Propósito:** Leer y transformar el archivo de configuración en estructuras de datos.

```cpp
// includes/Config.hpp
struct LocationConfig {
    std::string path;
    std::vector<std::string> allowed_methods;
    bool autoindex;
    std::vector<std::string> index_files;
    int return_code;
    std::string return_url;
    std::string root_directory;
    std::string upload_path;
    std::map<int, std::string> error_pages;
    LocationConfig(): autoindex(false), return_code(0) {}
};

struct ServerConfig {
    std::string server_name;
    int port;
    std::string root_directory;
    bool enable_reuse_addr;
    bool autoindex;
    long long client_max_body_size;
    std::vector<std::string> allowed_methods;
    std::map<int, std::string> error_pages;
    std::vector<LocationConfig> locations;
    ServerConfig(): port(0), enable_reuse_addr(false), autoindex(false), client_max_body_size(1048576) {}
};
```

---

### b) Parser de Solicitudes HTTP
**Propósito:** Transformar la cadena recibida por el socket en un objeto `Request`.

```cpp
// includes/Request.hpp
class Request {
public:
    typedef std::map<std::string, std::string> HeaderMap;
private:
    std::string _method;
    std::string _path;
    std::string _query_string;
    std::string _http_version;
    HeaderMap _headers;
    std::string _body;
    bool _is_parsed;
    int _error_code;
    // ...
public:
    Request();
    ~Request();
    bool parse(const std::string& raw_request);
    const std::string& getMethod() const;
    const std::string& getPath() const;
    // ...
};
```

---

### c) Manejo de Solicitudes
**Propósito:** Procesar la solicitud parseada y generar una respuesta.

```cpp
// includes/RequestHandler.hpp
class RequestHandler {
private:
    static bool _isDirectory(const std::string& path);
    static const LocationConfig* _findLocationForPath(const std::string& path, const ServerConfig& config);
public:
    static Response handle(Client& client);
};
```

---

### d) Generación de Respuestas
**Propósito:** Construir la respuesta HTTP.

```cpp
// includes/Response.hpp
class Response {
private:
    std::string _http_version;
    std::string _status_code;
    std::string _status_message;
    std::map<std::string, std::string> _headers;
    std::string _body;
public:
    Response();
    ~Response();
    void setStatusCode(const std::string& code, const std::string& message);
    void addHeader(const std::string& key, const std::string& value);
    void setBody(const std::string& body);
    std::string toString() const;
    void buildSimpleResponse(const std::string& code, const std::string& message);
    // ...
};
```

---

### e) Servidor Principal
**Propósito:** Gestionar sockets, aceptar conexiones y coordinar los módulos anteriores.

```cpp
// includes/Server.hpp
class Server {
private:
    const Config& _config;
    std::map<int, Client> _clients;
    std::vector<int> _listenSockets;
    std::map<int, ServerConfig> _listener_configs;
    // ...
    void acceptNewConnection(int listener_fd, int epoll_fd);
    void handleClientRequest(int client_fd, int epoll_fd);
    void handleClientResponse(int client_fd, int epoll_fd);
    void closeClientConnection(int client_fd, int epoll_fd);
};
```

---

## 4. Explicación de Clases Principales

- **Config / ServerConfig / LocationConfig:**  Gestionan toda la configuración del servidor y sus ubicaciones. Permiten separar la lógica de configuración del resto del código.
- **Request:**  Representa una solicitud HTTP parseada. Permite que el resto del servidor trabaje con datos ya estructurados.
- **Response:**  Construye y formatea la respuesta HTTP que se enviará al cliente.
- **RequestHandler:**  Decide cómo responder a cada solicitud, usando la configuración y el objeto `Request`.
- **Server:**  Gestiona los sockets, acepta conexiones y coordina el flujo general del servidor.

---

## 5. Checklist

1. Implementar el parser de configuración (`ConfigParser`).
2. Implementar el parser HTTP (`Request`).
3. Montar el servidor básico (`Server`).
4. Implementar el manejo de solicitudes (`RequestHandler`).
5. Implementar la generación de respuestas (`Response`).
6. Probar el MVP sirviendo archivos estáticos y respondiendo a errores.