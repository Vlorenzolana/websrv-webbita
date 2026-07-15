# 🏗️ Arquitectura Final: CGI Routing + Multiple Virtual Hosts

## 📊 Diagrama de Flujo

```
┌─────────────────────────────────────────────────────────────────┐
│                    MAIN PROCESS (webserv)                       │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  main.cpp                                               │   │
│  │  ├─ Lee config/multivhost.conf                         │   │
│  │  ├─ Crea ServerConfig[] = [server1, server2, server3]  │   │
│  │  └─ Para cada config:                                  │   │
│  │     ├─ new Server(config)                              │   │
│  │     ├─ server->init()      (crea socket + epoll)       │   │
│  │     └─ pthread_create(runServer, server)               │   │
│  │                                                          │   │
│  │  ┌──────────────────────────────────────────────────┐   │   │
│  │  │ THREAD 1 (Virtual Host 1)                       │   │   │
│  │  │ Port: 8080                                      │   │   │
│  │  │ Root: ./www/site1                               │   │   │
│  │  │                                                  │   │   │
│  │  │ ┌─────────────────────────────────────────────┐ │   │   │
│  │  │ │ epoll_wait() → eventos de clientes          │ │   │   │
│  │  │ │                                              │ │   │   │
│  │  │ │ Cliente 1: GET /cgi/test.py                 │ │   │   │
│  │  │ │   → _handleGet()                            │ │   │   │
│  │  │ │   → _isCGIRequest(".py") → TRUE             │ │   │   │
│  │  │ │   → _handleCGI()                            │ │   │   │
│  │  │ │      → CGIHandler::execute()                │ │   │   │
│  │  │ │         → fork() + execve("/usr/bin/python3") │ │   │   │
│  │  │ │         → captura stdout                    │ │   │   │
│  │  │ │      → _sendResponse(200, "OK", output)    │ │   │   │
│  │  │ │                                              │ │   │   │
│  │  │ │ Cliente 2: GET /index.html                  │ │   │   │
│  │  │ │   → _handleGet()                            │ │   │   │
│  │  │ │   → _isCGIRequest("html") → FALSE           │ │   │   │
│  │  │ │   → abre archivo                            │ │   │   │
│  │  │ │   → _sendResponse(200, "OK", file_content)  │ │   │   │
│  │  │ └─────────────────────────────────────────────┘ │   │   │
│  │  └──────────────────────────────────────────────────┘   │   │
│  │                                                          │   │
│  │  ┌──────────────────────────────────────────────────┐   │   │
│  │  │ THREAD 2 (Virtual Host 2)                       │   │   │
│  │  │ Port: 8081                                      │   │   │
│  │  │ Root: ./www/site2                               │   │   │
│  │  │ (igual a Thread 1, pero escuchando en 8081)     │   │   │
│  │  └──────────────────────────────────────────────────┘   │   │
│  │                                                          │   │
│  │  ┌──────────────────────────────────────────────────┐   │   │
│  │  │ THREAD 3 (Virtual Host 3)                       │   │   │
│  │  │ Port: 8082                                      │   │   │
│  │  │ Root: ./www/site3                               │   │   │
│  │  │ (igual a Thread 1, pero escuchando en 8082)     │   │   │
│  │  └──────────────────────────────────────────────────┘   │   │
│  │                                                          │   │
│  │  pthread_join(all threads) → espera a que terminen      │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

## 🔄 Flujo de Procesamiento de Requests

### Caso 1: GET archivo estático (index.html)

```
Cliente: GET http://localhost:8080/index.html
           ↓
    epoll_wait() despierta
           ↓
    handleClient(clientFd)
           ↓
    request.parse()
           ↓
    _matchLocation("/index.html") → encuentra "location /"
           ↓
    _handleGet(clientFd, request, location)
           ↓
    _resolvePath(location, "/index.html") → "./www/site1/index.html"
           ↓
    stat(fullPath) → OK, es archivo regular
           ↓
    _isCGIRequest("./www/site1/index.html") → FALSE (extensión .html)
           ↓
    abre archivo con ifstream
           ↓
    _getMimeType(".html") → "text/html"
           ↓
    _sendResponse(200, "OK", "text/html", contenido)
           ↓
    HTTP/1.1 200 OK
    Content-Type: text/html
    Content-Length: 456
    
    <!DOCTYPE html>...
```

### Caso 2: GET script CGI (test.py)

```
Cliente: GET http://localhost:8080/cgi/test.py
           ↓
    epoll_wait() despierta
           ↓
    handleClient(clientFd)
           ↓
    request.parse()
           ↓
    _matchLocation("/cgi/test.py") → encuentra "location /cgi"
           ↓
    _handleGet(clientFd, request, location)
           ↓
    _resolvePath(location, "/cgi/test.py") → "./www/site1/cgi/test.py"
           ↓
    stat(fullPath) → OK, es archivo regular
           ↓
    _isCGIRequest("./www/site1/cgi/test.py") → TRUE (extensión .py)
           ↓
    _handleCGI(clientFd, request, location, fullPath)
           │
           ├─ ext = "py" → interpreter = "/usr/bin/python3"
           │
           ├─ CGIHandler cgiHandler(fullPath, interpreter)
           │
           └─ cgiHandler.execute(request, uploadPath)
                │
                ├─ Configura variables de entorno:
                │  REQUEST_METHOD=GET
                │  PATH_INFO=/cgi/test.py
                │  QUERY_STRING=
                │  SERVER_PROTOCOL=HTTP/1.1
                │  CONTENT_LENGTH=
                │  CONTENT_TYPE=
                │
                ├─ pipe(pipe_in), pipe(pipe_out)
                │
                ├─ fork()
                │  ├─ [CHILD]
                │  │  ├─ dup2(pipe_in[0], STDIN_FILENO)
                │  │  ├─ dup2(pipe_out[1], STDOUT_FILENO)
                │  │  └─ execve("/usr/bin/python3", ["python3", "test.py"], envp)
                │  │     → STDOUT del script → pipe_out[1]
                │  │
                │  └─ [PARENT]
                │     ├─ close(pipe_in[0], pipe_out[1])
                │     ├─ lee desde pipe_out[0]
                │     ├─ captura el output del script
                │     └─ waitpid() por proceso hijo
           ↓
    Script Python ejecutado:
    ┌──────────────────────────────────────────────┐
    │ #!/usr/bin/env python3                       │
    │                                              │
    │ print("Content-Type: text/html\r\n\r\n")    │
    │ print("<html><body>")                        │
    │ print("<h1>Python CGI Script Test</h1>")     │
    │ ...                                          │
    │ print("</body></html>")                      │
    └──────────────────────────────────────────────┘
           ↓
    output capturado =
    "Content-Type: text/html\r\n\r\n<html><body>..."
           ↓
    _sendResponse(200, "OK", "text/plain", output)
           ↓
    HTTP/1.1 200 OK
    Content-Type: text/plain
    Content-Length: 523
    
    <html><body>
    <h1>Python CGI Script Test</h1>
    ...
```

### Caso 3: POST a CGI

```
Cliente: POST http://localhost:8080/cgi/test.py
         Body: "name=test&value=123"
           ↓
    handleClient(clientFd)
           ↓
    request.parse() → extrae body
           ↓
    _matchLocation("/cgi/test.py") → "location /cgi"
           ↓
    _handlePost(clientFd, request, location)
           ↓
    _resolvePath() → "./www/site1/cgi/test.py"
           ↓
    stat() → OK, archivo regular
           ↓
    _isCGIRequest(".py") → TRUE
           ↓
    _handleCGI(clientFd, request, location, fullPath)
           ↓
    CGIHandler configura entorno:
    REQUEST_METHOD=POST
    CONTENT_LENGTH=18
    CONTENT_TYPE=application/x-www-form-urlencoded
           ↓
    Script recibe datos por STDIN:
    name=test&value=123
           ↓
    Script procesa y devuelve respuesta
           ↓
    _sendResponse(200, "OK", output)
```

## 📡 Routing de Múltiples Hosts

```
1. Cliente conecta a puerto 8080
   → Thread 1 lo acepta (Socket 1)
   
2. Cliente conecta a puerto 8081
   → Thread 2 lo acepta (Socket 2)
   
3. Cliente conecta a puerto 8082
   → Thread 3 lo acepta (Socket 3)

Simultáneamente:
- Thread 1 maneja múltiples clientes en puerto 8080 (epoll)
- Thread 2 maneja múltiples clientes en puerto 8081 (epoll)
- Thread 3 maneja múltiples clientes en puerto 8082 (epoll)

Escalabilidad:
- N threads × M conexiones por thread = N×M conexiones totales
- Cada thread tiene su propio epoll fd
- No hay lock contention entre threads
```

## 🎯 Puntos de Decisión en Server::handleClient()

```
Client request llega
        ↓
    ¿La ruta matchea alguna location?
    ├─ NO → _sendErrorResponse(404)
    │
    └─ YES → ¿Método permitido en esta location?
         ├─ NO → _sendErrorResponse(405)
         │
         └─ YES → ¿Hay return_code configurado?
              ├─ YES → redirect
              │
              └─ NO → ¿Path traversal (..)?
                   ├─ YES → _sendErrorResponse(403)
                   │
                   └─ NO → ¿Qué método es?
                        ├─ GET → _handleGet()
                        │        ├─ ¿Es CGI?
                        │        │  ├─ YES → _handleCGI()
                        │        │  └─ NO  → Sirve archivo
                        │        │
                        │        └─ ¿Es directorio?
                        │           ├─ YES → Busca index files
                        │           │        ├─ Encontrado → Sirve
                        │           │        └─ NO → autoindex?
                        │           │           ├─ YES → _buildAutoindexPage()
                        │           │           └─ NO  → 403
                        │           └─ NO  → Sirve archivo
                        │
                        ├─ POST → _handlePost()
                        │         ├─ ¿Es CGI?
                        │         │  ├─ YES → _handleCGI()
                        │         │  └─ NO  → Guarda en upload_path
                        │         │
                        │         └─ ¿Tamaño > limit?
                        │            ├─ YES → 413
                        │            └─ NO  → OK
                        │
                        └─ DELETE → _handleDelete()
                                   ├─ ¿Es archivo?
                                   │  ├─ YES → Borra y 200
                                   │  └─ NO  → 403
                                   │
                                   └─ ¿Error al borrar?
                                      ├─ YES → 500
                                      └─ NO  → 200
```

## 📁 Estructura de Archivos Final

```
websrv-webbita-feature-merge-mvp/
├── src/
│   ├── main.cpp ★ Modificado: Threads + Multi-host
│   ├── Server.cpp ★ Modificado: CGI detection + handling
│   ├── CGIHandler.cpp (sin cambios)
│   ├── ConfigParser.cpp
│   ├── ConfigValidator.cpp
│   └── Request.cpp
│
├── includes/
│   ├── Server.hpp ★ Modificado: Métodos CGI
│   ├── CGIHandler.hpp
│   ├── ConfigParser.hpp
│   ├── ConfigValidator.hpp
│   ├── Request.hpp
│   └── Config.hpp
│
├── www/
│   ├── site1/
│   │   ├── index.html ★ Nuevo
│   │   ├── cgi/
│   │   │   ├── test.py ★ Nuevo
│   │   │   └── test.sh ★ Nuevo
│   │   └── uploads/
│   ├── site2/
│   │   ├── index.html ★ Nuevo
│   │   └── uploads/
│   ├── site3/
│   │   └── index.html ★ Nuevo
│   └── uploads1/, uploads2/
│
├── config/
│   ├── multivhost.conf ★ Nuevo (3 virtual hosts)
│   └── (otros archivos config)
│
├── Makefile ★ Modificado: CGIHandler + -lpthread
├── build.sh ★ Nuevo: Script build para Linux
├── build.bat ★ Nuevo: Script build para Windows
├── test_multivhost.sh ★ Nuevo: Tests Bash
├── test_multivhost.ps1 ★ Nuevo: Tests PowerShell
└── TESTING_GUIDE.md ★ Nuevo: Documentación completa

Archivos modificados: 3 (Server.hpp, Server.cpp, main.cpp, Makefile)
Archivos nuevos: 10+
```

## 🚀 Ventajas de esta Implementación

| Característica | Ventaja |
|---|---|
| **CGI Routing** | Scripts ejecutables automáticamente sin config especial |
| **Multiple Hosts** | 1 proceso ejecuta múltiples servidores en paralelo |
| **epoll** | Escalable: 1 thread × 1000+ conexiones |
| **Threads** | Fácil de entender, mejor que async/await |
| **Seguridad** | Path traversal check, size limits, method validation |
| **Compatibility** | POSIX threads (funciona en Linux, Mac, WSL) |

## ⚠️ Limitaciones Actuales

1. **CGI Output**: No captura stderr, solo stdout
2. **Timeout**: Scripts CGI sin timeout pueden bloquear
3. **Signal Handling**: No maneja SIGCHLD correctamente (zombie processes)
4. **Performance**: Mejor usar select/poll para >>10k conexiones
5. **Windows**: Threads POSIX no disponibles (usar WSL o winsock2)
