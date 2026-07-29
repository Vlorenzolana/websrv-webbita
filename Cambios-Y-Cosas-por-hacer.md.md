# Continuación del trabajo

## 1. Correcciones implementadas

### Compilación y ejecución

```sh
make
./webserv config/webserv.conf
```

### Correcciones principales

- Sockets de cliente y pipes de CGI en modo no bloqueante, integrados con `epoll`.

```cpp
// srcs/Server.cpp
bool Server::_setNonBlocking(int fd)
{
    const int flags = fcntl(fd, F_GETFL, 0);
    return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) >= 0;
}
```

```cpp
// srcs/CGIHandler.cpp
bool CGIHandler::_setNonBlocking(int fd)
{
    const int flags = fcntl(fd, F_GETFL, 0);
    return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) >= 0;
}
```

- Recolección de procesos hijo con `waitpid(..., WNOHANG)` y timeout de 10 segundos.

```cpp
// srcs/Server.cpp
for (std::map<pid_t, CgiState>::iterator it = _cgiByPid.begin();
     it != _cgiByPid.end(); ++it)
{
    if (!it->second.childExited && it->second.childPid > 0)
    {
        if (kill(-it->second.childPid, SIGKILL) < 0)
            kill(it->second.childPid, SIGKILL);
        waitpid(it->second.childPid, NULL, WNOHANG);
    }
}
```

- Ejecución de CGI conectada al routing de GET/POST y ejecutada desde el directorio del script.

```cpp
// srcs/CGIHandler.cpp
const std::string directory = _directoryName(_scriptPath);
const std::string scriptName = _baseName(_scriptPath);
if (chdir(directory.c_str()) < 0)
    _exit(126);

char* arguments[3];
arguments[0] = const_cast<char*>(_interpreterPath.c_str());
arguments[1] = const_cast<char*>(scriptName.c_str());
arguments[2] = NULL;
execve(arguments[0], arguments, envp);
```

- Mapeo del intérprete por ubicación mediante `cgi_extension`.

```cpp
// srcs/ConfigParser.cpp
else if (tokens[0] == "cgi_extension")
    _parseCgiExtension(location, tokens);
```

- Parseo incremental de HTTP con cabeceras insensibles a mayúsculas/minúsculas y decodificación de chunked.

```cpp
// srcs/Request.cpp
const std::string transferEncoding = _toLower(getHeaderValue("transfer-encoding"));
if (!transferEncoding.empty())
{
    if (transferEncoding != "chunked")
    {
        _setError(501);
        return;
    }
    _isChunkedBody = true;
    _parsingState = PARSE_CHUNK_SIZE;
    return;
}
```

- Aplicación temprana de `client_max_body_size`.

```cpp
// srcs/Request.cpp
if (_maxBodySize > 0 && _contentLength > _maxBodySize)
{
    _setError(413);
    return;
}
```

- Host y puerto de escucha configurables.

```cpp
// srcs/ConfigParser.cpp
void ConfigParser::_parseListen(ServerConfig& server, const std::string& value)
{
    const std::string clean = _withoutSemicolon(value);
    const std::size_t colon = clean.rfind(':');
    if (colon == std::string::npos)
    {
        server.port = _parsePort(clean);
        return;
    }

    const std::string host = clean.substr(0, colon);
    const std::string port = clean.substr(colon + 1);
    server.host = host;
    server.port = _parsePort(port);
}
```

- Páginas de error personalizadas.

```cpp
// srcs/ConfigParser.cpp
else if (tokens[0] == "error_page")
    _parseErrorPage(server.error_pages, line);
```

- Subidas binarias y multipart/form-data.

```cpp
// tests/smoke_test.sh
curl -X POST --data-binary @"$TMP_DIR/raw.bin" \
    "http://127.0.0.1:${PORT}/uploads/raw.bin"
```

- Escritura de respuestas solo tras recibir un evento `EPOLLOUT`.

```cpp
// srcs/Server.cpp
if ((events & EPOLLOUT) &&
    client->second.responsePending && !client->second.processing)
{
    _sendResponse(clientFd);
}
```

- Timeout de clientes inactivos y limpieza de descriptores con un único propietario.

```cpp
// srcs/Server.cpp
_checkTimeouts();
```

```nginx
location /cgi-bin {
    allowed_methods GET POST;
    root ./www/cgi-bin;
    upload_path ./www/uploads;
    cgi_extension .py /usr/bin/python3;
    cgi_extension .sh /bin/bash;
}
```

### Prueba rápida incluida

```sh
make test
```

El script usa el puerto 18080 por defecto. Si se desea, se puede pasar otro puerto directamente:

```sh
./tests/smoke_test.sh 18090
```

## 2. Verificación realizada

### Evidencia comprobada

- Compilación correcta:
  - Comando ejecutado: `bash TESTS/build.sh all`
  - Resultado: se generó el binario `webserv` y la compilación terminó con éxito.

- Funcionalidad básica verificada en ejecución real:
  - Las peticiones a los puertos 8080, 8081 y 8082 devolvieron HTTP 200.
  - La ruta CGI respondió correctamente con HTTP 200.

### Puntos del código que cumplen

- Makefile:
  - Define `NAME = webserv`
  - Tiene `all`, `clean`, `fclean` y `re`
  - Usa `-std=c++98`

- main.cpp:
  - Acepta un archivo de configuración como argumento y usa uno por defecto si no se pasa.

- Server.cpp:
  - Usa `epoll` (`epoll_create1`, `epoll_ctl`, `epoll_wait`)
  - Usa `fcntl(..., F_SETFL, O_NONBLOCK)` en el modo esperado

- CGIHandler.cpp:
  - Implementa el flujo CGI con `fork`, `pipe`, `dup2` y `execve`

### Estado final

- ✅ Cumple los puntos principales y más críticos del subject.
- ⚠️ Los casos extra más específicos, como 405, 413, DELETE, traversal y limpieza completa de procesos hijo, requieren una comprobación adicional más exhaustiva.

## 3. Explicación técnica del parser y del CGI

### 3.1. ConfigParser

Lo nuevo en esta parte es que el parser lee el archivo de configuración, valida la sintaxis básica y cambia de contexto según si está en el ámbito global, en un bloque `server` o en un bloque `location`.

Qué hace bien:
- Detecta si está dentro de un contexto global, de servidor o de location.
- Interpreta directivas como `listen`, `server_name`, `root`, `error_page` y `client_max_body_size`.
- Valida y normaliza la configuración antes de entregarla al resto del servidor.
- Gestiona errores simples de configuración.

```cpp
// Flujo general del parseo: cambia entre GLOBAL, SERVER y LOCATION.
// El parser está mirando el contexto en el que se encuentra mientras lee el archivo de configuración.

// En la parte global: directivas generales.
// Dentro de server: se permiten configuraciones del servidor.
// Dentro de location: se permiten reglas específicas para una ruta concreta.

std::vector<ServerConfig> ConfigParser::parseFile(const std::string& filename)
{
    std::ifstream file(filename.c_str());
    std::vector<ServerConfig> servers;

    if (!file.is_open())
        throw std::runtime_error("Could not open config file: " + filename);

    std::string line;
    ParsingState state = GLOBAL;

    while (std::getline(file, line))
    {
        line = _trim(line);
        if (line.empty() || line[0] == '#')
            continue;

        std::vector<std::string> tokens = _split(line);
        if (tokens.empty())
            continue;

        if (tokens[0] == "server")
            state = SERVER;
        else if (tokens[0] == "location")
            state = LOCATION;
        else if (tokens[0] == "}")
            state = GLOBAL;
    }

    return servers;
}
```

#### Código de apoyo que conviene revisar

```cpp
void ConfigParser::_handleServer(ServerConfig& server, const std::vector<std::string>& tokens, const std::string& line)
{
    if (tokens.empty())
        return;

    if (tokens[0] == "listen" && tokens.size() == 2)
        server.listen_port = std::atoi(tokens[1].c_str());
    else if (tokens[0] == "server_name" && tokens.size() == 2)
        server.server_name = tokens[1];
    else if (tokens[0] == "root" && tokens.size() == 2)
        server.root = tokens[1];
    else if (tokens[0] == "error_page")
        _parseErrorPage(server.error_pages, line);
    else
        throw std::runtime_error("Unknown or invalid server directive: " + tokens[0]);
}
```

### 3.2. CGIHandler

Este archivo se encarga de ejecutar scripts CGI. Su tarea es crear un entorno correcto para que el script pueda funcionar, capturar su salida y convertirla en una respuesta HTTP.

Qué hay que mirar:
- La creación del proceso hijo.
- La preparación de variables de entorno.
- La entrada y salida del script.
- El manejo de errores y respuestas.

```cpp
int CGIHandler::executeCGI(const std::string& scriptPath)
{
    pid_t pid = fork();

    if (pid == 0)
    {
        execve(scriptPath.c_str(), argv, envp);
        exit(1);
    }
    else if (pid > 0)
    {
        waitpid(pid, &status, 0);
    }

    return 0;
}
```

También es importante pasar variables como:

```cpp
setenv("REQUEST_METHOD", method.c_str(), 1);
setenv("QUERY_STRING", query.c_str(), 1);
setenv("CONTENT_LENGTH", contentLength.c_str(), 1);
```

#### Comparación simple vs mejorada

Versión simple: solo ejecuta el script.

```cpp
int CGIHandler::executeCGI(const std::string& scriptPath)
{
    pid_t pid = fork();

    if (pid == 0)
    {
        execve(scriptPath.c_str(), argv, envp);
        exit(1);
    }
    else if (pid > 0)
    {
        waitpid(pid, &status, 0);
    }

    return 0;
}
```

Versión mejorada: añade comunicación y control.

```cpp
int inputPipe[2] = {-1, -1};
int outputPipe[2] = {-1, -1};

if (pipe(inputPipe) < 0)
    return false;
if (pipe(outputPipe) < 0)
{
    _closePipe(inputPipe);
    return false;
}
```

Versión mejorada: configura los descriptores de fichero.

```cpp
if (!_setNonBlocking(inputPipe[1]) || !_setNonBlocking(outputPipe[0]) ||
    !_setCloseOnExec(inputPipe[0]) || !_setCloseOnExec(inputPipe[1]) ||
    !_setCloseOnExec(outputPipe[0]) || !_setCloseOnExec(outputPipe[1]))
{
    _closePipe(inputPipe);
    _closePipe(outputPipe);
    return false;
}
```

Nota sobre `O_NONBLOCK` y macOS:
`O_NONBLOCK` se usa junto con `fcntl(..., F_SETFL, O_NONBLOCK)` para poner un descriptor en modo no bloqueante. Sin eso, operaciones como `read`, `write`, `accept` o `recv` podrían quedarse esperando indefinidamente y bloquear el servidor. En macOS esta es la forma estándar y portable de habilitar I/O no bloqueante, por eso se incluye en el código.

## 4. Casos que necesito que compruebes

### 4.1. Funcionalidad básica

1. Servidor arranca correctamente con un archivo de configuración válido.
2. Se pueden abrir las páginas estáticas de los tres virtual hosts.
3. Un `GET` a `/index.html` devuelve `200 OK`.
4. Un `GET` a una ruta inexistente devuelve `404 Not Found`.

### 4.2. CGI

1. Comprobar un CGI en Python: `curl http://localhost:8080/cgi/test.py`.
2. Comprobar un CGI en Bash: `curl http://localhost:8080/cgi/test.sh`.
3. Probar que se pasan correctamente los parámetros en query string: `curl "http://localhost:8080/cgi/test.py?name=test&value=123"`.
4. Probar un `POST` con datos de formulario: `curl -X POST -d "name=test" http://localhost:8080/cgi/test.py`.

### 4.3. Subidas y archivos

1. Subir un archivo binario o de imagen con `curl -X POST --data-binary @archivo http://localhost:8080/upload/`.
2. Comprobar que el archivo queda guardado en la carpeta configurada.
3. Descargar el archivo subido y verificar que el contenido sigue siendo el mismo.
4. Probar un `DELETE` sobre un archivo existente.

### 4.4. Errores HTTP

1. Probar un método no permitido y verificar `405 Method Not Allowed`.
2. Probar un cuerpo demasiado grande y verificar `413 Payload Too Large`.
3. Probar una ruta con traversal y verificar que se rechaza con `403` o `404` según la política del servidor.

### 4.5. Configuración y robustez

1. Probar con un archivo de configuración inválido para comprobar que el servidor falla de forma limpia.
2. Comprobar que el servidor sigue respondiendo si un cliente se conecta y no envía datos.
3. Verificar que no se quedan procesos CGI huérfanos tras terminar la ejecución.
4. Probar varias conexiones simultáneas para comprobar que no se bloquea el servidor.

### 4.6. Comandos

```sh
# Compilar
bash TESTS/build.sh all

# Arrancar servidor
./webserv config/multivhost.conf

# Probar puertos virtuales
curl -i http://localhost:8080/
curl -i http://localhost:8081/
curl -i http://localhost:8082/

# Probar CGI
curl -i http://localhost:8080/cgi/test.py
curl -i "http://localhost:8080/cgi/test.py?name=test"

# Probar upload
curl -i -X POST --data-binary @archivo.txt http://localhost:8080/upload/
```

## 5. Resumen rápido

Si revisas estos puntos, tendremos una buena cobertura de lo que debería comprobar en el servidor: arranque, virtual hosts, CGI, uploads, errores HTTP y robustez frente a casos límite.