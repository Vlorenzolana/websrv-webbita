# webserv - guia para la correccion

Proyecto de servidor HTTP/1.1 en C++98 para 42. Esta guia esta pensada para la correccion de esta tarde: primero demuestra el comportamiento obligatorio y despues los extras, sin mezclar ambos recorridos.

## Estado verificado

- Compilacion correcta con `-Wall -Wextra -Werror -std=c++98`.
- I/O basado en `epoll` y descriptores no bloqueantes.
- Parser incremental de peticiones HTTP/1.0 y HTTP/1.1.
- Metodos implementados: `GET`, `POST` y `DELETE`.
- `Content-Length`, `Transfer-Encoding: chunked`, limites de cuerpo y errores HTTP.
- Virtual hosts, varios listeners, locations, roots, index, autoindex y redirecciones.
- Upload binario, multipart y borrado protegido.
- CGI asincrono con timeout, limite de salida, pipes no bloqueantes y recoleccion de hijos.
- Paginas de error personalizadas.

La verificacion automatica actual termina con:

```text
All smoke tests passed on port 18080.
```

## Arranque rapido

El proyecto necesita un entorno Linux porque usa `fork`, `pipe`, `epoll` y `waitpid`. En Windows, usar WSL:

```sh
wsl --cd /mnt/c/Users/VanessaL/Documents/practice/websrv-webbita -- make
wsl --cd /mnt/c/Users/VanessaL/Documents/practice/websrv-webbita -- ./webserv config/webserv.conf
```

La configuracion base escucha en `127.0.0.1:8081`. Para ejecutar la bateria completa:

```sh
wsl --cd /mnt/c/Users/VanessaL/Documents/practice/websrv-webbita -- make test
```

Otros comandos utiles:

```sh
make clean
make fclean
make re
make leaks
./tests/smoke_test.sh 18090
```

`make leaks` requiere `valgrind` y debe ejecutarse en WSL/Linux.

## Recorrido recomendado para la correccion

### 1. Compilacion y estructura

1. Ejecutar `make` y comprobar que no hay warnings.
2. Mostrar `Makefile`, `main.cpp` y el uso de C++98.
3. Arrancar con `./webserv config/webserv.conf`

### Como funciona realmente `epoll` en este proyecto

Esta frase no significa que exista una clase `Epoll`, ni que cada conexion
tenga un hilo propio. El diseno es un bucle de eventos: un solo objeto
`Server` espera cambios de estado en muchos descriptores y decide que pequena
operacion hacer en cada evento.

#### 1. Entrada del programa y ownership de `Server`

El punto de entrada esta en `srcs/main.cpp`:

```cpp
ConfigParser parser;
const std::vector<ServerConfig> servers = parser.parseFile(configPath);
Server server(servers);
server.init();
server.run();
```

La vida del objeto `server` cubre todo el servidor. Su destructor se ejecuta
al salir del `try` o si se propaga una excepcion, y cierra clientes, listeners,
pipes CGI y el descriptor de `epoll`.

El header de esa clase es `includes/Server.hpp`. La implementacion esta en
`srcs/Server.cpp`. `Server` no hereda de ninguna clase y no tiene metodos
virtuales: no hay polimorfismo aqui. Es una clase coordinadora que compone
varios tipos de estado y usa las clases `Request`, `CGIHandler`,
`ServerConfig` y `LocationConfig`.

#### 2. Que guarda `Server`

Las estructuras privadas de `includes/Server.hpp` son registros de estado,
no clases de dominio con herencia:

```cpp
struct ClientState
{
        Request request;
        int listenPort;
        std::string listenHost;
        std::time_t lastActivity;
        bool processing;
};

struct PendingResponse
{
        std::string data;
        std::size_t offset;
};

struct CgiPipeRef
{
        pid_t childPid;
        bool isInput;
};
```

Los mapas relacionan un descriptor con su estado:

```cpp
std::map<int, ListenerState> _listeners;
std::map<int, ClientState> _clients;
std::map<int, PendingResponse> _pendingResponses;
std::map<pid_t, CgiState> _cgiByPid;
std::map<int, CgiPipeRef> _cgiPipeRefs;
```

La idea de “un solo propietario por descriptor” se entiende asi:

- Un listener aparece en `_listeners` y se cierra desde la gestion de
    listeners/destructor.
- Un socket de cliente aparece en `_clients`; `_closeConnection()` lo quita
    de `epoll`, lo cierra y borra su estado.
- Un pipe CGI se identifica en `_cgiPipeRefs` y se cierra mediante
    `_closeCgiPipe()`, que tambien actualiza el `CgiState` correspondiente.
- `_pendingResponses` no es otro owner del socket: solo contiene la respuesta
    pendiente asociada a un cliente que ya vive en `_clients`.

Hay referencias de consulta y estado duplicado para los CGI, por ejemplo el
pipe esta en `CgiState` y tambien indexado por `_cgiPipeRefs`. Eso no significa
que haya dos cierres independientes: el cierre debe pasar por
`_closeCgiPipe()` para mantener ambos mapas coherentes.

#### 3. Creacion de `epoll` y listeners

`Server::init()` crea una instancia de `epoll` y abre un listener por cada
pareja host/puerto necesaria:

```cpp
_epollFd = epoll_create1(EPOLL_CLOEXEC);
...
_openListener(_servers[i]);
```

`_openListener()` hace la secuencia clasica del socket TCP:

```cpp
const int listenerFd = socket(AF_INET, SOCK_STREAM, 0);
setsockopt(listenerFd, SOL_SOCKET, SO_REUSEADDR, ...);
fcntl(listenerFd, F_SETFL, flags | O_NONBLOCK);
bind(listenerFd, ...);
listen(listenerFd, SOMAXCONN);
```

Despues registra el listener en `epoll` interesado en lecturas:

```cpp
struct epoll_event event;
std::memset(&event, 0, sizeof(event));
event.events = EPOLLIN;
event.data.fd = listenerFd;
epoll_ctl(_epollFd, EPOLL_CTL_ADD, listenerFd, &event);
```

En un socket de escucha, `EPOLLIN` significa “hay al menos una conexion
pendiente para aceptar”; no significa que ya haya datos HTTP.

#### 4. El bucle principal

`Server::run()` espera como maximo un segundo en cada vuelta:

```cpp
while (true)
{
        const int count = epoll_wait(_epollFd, events, 128, 1000);
        ...
        for (int i = 0; i < count; ++i)
        {
                const int fd = events[i].data.fd;
                if (_listeners.find(fd) != _listeners.end())
                        _acceptClients(fd);
                else if (_cgiPipeRefs.find(fd) != _cgiPipeRefs.end())
                        _handleCgiEvent(fd, events[i].events);
                else if (_clients.find(fd) != _clients.end())
                        _handleClientEvent(fd, events[i].events);
        }
        _reapCgiProcesses();
        _checkTimeouts();
}
```

El `fd` es la clave de la clasificacion. No se hace `dynamic_cast`, no hay
callbacks virtuales y no se pregunta al descriptor que tipo tiene. El orden de
consulta evita tratar un pipe CGI como socket de cliente.

Tras procesar los eventos se hacen dos tareas de mantenimiento:

- `_reapCgiProcesses()` llama a `waitpid(..., WNOHANG)` para recoger hijos que
    ya terminaron sin bloquear el bucle.
- `_checkTimeouts()` revisa la actividad de clientes y el tiempo de vida de
    los CGI para cerrar o matar trabajos que exceden sus limites.

#### 5. Aceptar clientes sin bloquear

`_acceptClients()` acepta conexiones hasta que `accept()` devuelve `EAGAIN` o
`EWOULDBLOCK`:

```cpp
while (true)
{
        const int clientFd = accept(listenerFd, NULL, NULL);
        if (clientFd < 0)
        {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                        return;
                return;
        }
        ...
}
```

El listener es no bloqueante, por eso el servidor no se queda esperando a que
llegue otra conexion. Cada cliente nuevo tambien se configura como no
bloqueante y se registra con:

```cpp
event.events = EPOLLIN | EPOLLRDHUP;
event.data.fd = clientFd;
epoll_ctl(_epollFd, EPOLL_CTL_ADD, clientFd, &event);
_clients[clientFd] = state;
```

`EPOLLRDHUP` permite detectar que el peer cerro su mitad de la conexion.

#### 6. Leer una peticion HTTP

Cuando llega `EPOLLIN`, `_handleClientEvent()` delega en
`_handleClientReadable()` si el cliente no esta procesando otra respuesta:

```cpp
if ((events & EPOLLIN) && !_clients[clientFd].processing)
        _handleClientReadable(clientFd);
```

El handler hace un `recv()` no bloqueante, incorpora los bytes al parser
`Request` y conserva el estado entre eventos:

```cpp
char buffer[8192];
const ssize_t received = recv(clientFd, buffer, sizeof(buffer), 0);
if (received > 0)
{
        const bool complete = client->second.request.parse(
                std::string(buffer, static_cast<std::size_t>(received)));
        if (complete || client->second.request.isParsed())
        {
                client->second.processing = true;
                _processRequest(clientFd, client->second.request,
                        client->second.listenPort, client->second.listenHost);
        }
}
```

Importante para la explicacion: una llamada a `recv()` no garantiza una
peticion completa. Puede llegar solo la primera linea, un header parcial o un
trozo del body. `Request` esta en `ClientState`, por lo que sus buffers y su
maquina de estados sobreviven hasta el siguiente `EPOLLIN`.

El `recv()` no esta dentro de un bucle que intente vaciar el socket. Se hace
una lectura por evento y el modo level-triggered de `epoll` volvera a notificar
si siguen quedando datos. Esto reduce el riesgo de bloquearse y mantiene el
trabajo de cada vuelta acotado.

#### 7. Procesar y preparar la respuesta

`_processRequest()` selecciona la configuracion por listener y `Host`, busca
la `location`, valida el metodo y decide entre archivo estatico, upload,
DELETE, error o CGI. El trabajo de routing esta en la misma clase `Server`;
`Request` solo parsea y expone los datos de la peticion.

Para una respuesta normal se llama a `_queueResponse()`. Este metodo no hace
`send()` inmediatamente: guarda los bytes y cambia el interes del cliente a
`EPOLLOUT`:

```cpp
_pendingResponses[clientFd] = pending;
_clients[clientFd].processing = true;

event.events = EPOLLOUT | EPOLLRDHUP;
event.data.fd = clientFd;
epoll_ctl(_epollFd, EPOLL_CTL_MOD, clientFd, &event);
```

La razon es que `send()` puede aceptar solo una parte de una respuesta o no
estar listo. Cuando el socket notifica `EPOLLOUT`,
`_handleClientWritable()` llama a `_flushResponse()`:

```cpp
const ssize_t sent = send(clientFd,
        pending->second.data.data() + pending->second.offset,
        pending->second.data.size() - pending->second.offset, 0);
if (sent > 0)
        pending->second.offset += static_cast<std::size_t>(sent);
```

`offset` permite continuar en el siguiente evento. Cuando llega al final, se
borra la respuesta pendiente y se cierra la conexion en este flujo HTTP.

#### 8. Pipes CGI dentro del mismo bucle

El CGI no se gestiona con un hilo ni con un `waitpid()` bloqueante. Al crear un
CGI, `CGIHandler` usa `fork`, `pipe`, `dup2` y `execve`; el padre conserva un
pipe de entrada y otro de salida. `Server` registra esos extremos en
`epoll` y los etiqueta en `_cgiPipeRefs` con `isInput`.

El flujo es:

```text
cliente --EPOLLIN--> Server --write/EPOLLOUT--> stdin del CGI
cliente <--EPOLLOUT-- Server <--read/EPOLLIN-- stdout del CGI
```

`_handleCgiEvent()` decide si debe escribir el body pendiente o leer salida:

```cpp
if (isInput && (events & EPOLLOUT))
        _handleCgiWritable(pipeFd);
else if (!isInput && (events & EPOLLIN))
        _handleCgiReadable(pipeFd);
```

`_handleCgiWritable()` usa `inputOffset`, y `_handleCgiReadable()` añade una
lectura de hasta 8192 bytes a `CgiState::output`. Si la salida supera 16 MiB,
el CGI se termina. Cuando stdout llega a EOF y el hijo ha terminado,
`_tryFinalizeCgi()` construye la respuesta HTTP final.

#### 9. Que significa exactamente “sin I/O bloqueante”

Significa que las operaciones susceptibles de esperar estan controladas:

- listeners y clientes tienen `O_NONBLOCK`;
- pipes del padre tienen `O_NONBLOCK`;
- se espera a que `epoll` anuncie `EPOLLIN` o `EPOLLOUT` antes de leer/escribir;
- no se usa un `recv()` o `send()` en bucle hasta vaciar toda la peticion;
- no se usa `waitpid(pid, 0)` durante el procesamiento normal;
- se usa `waitpid(pid, WNOHANG)` en `_reapCgiProcesses()`.

`epoll_wait()` si puede dormir, pero ese es su cometido: duerme al proceso
hasta que hay trabajo o vence el timeout de mantenimiento. No es un bloqueo
accidental de un cliente concreto.

#### 10. Forma canonica y polimorfismo: que responder

Este proyecto usa composicion y despacho estatico, no polimorfismo:

- `Server`, `Request`, `CGIHandler`, `ConfigParser` y `ConfigValidator` son
    clases independientes sin clases base virtuales.
- `LocationConfig`, `ServerConfig` y los estados internos son `struct` de
    datos.
- No hay `virtual`, `override`, clases abstractas ni `dynamic_cast`.
- La eleccion entre listener, cliente y pipe se hace buscando el descriptor
    en mapas.

La forma canonica ortodoxa de C++98 suele referirse a constructor por defecto,
constructor de copia, operador de asignacion y destructor. `CGIHandler.hpp`
incluye un comentario que dice “Orthodox Canonical Form”, pero declara
explicitamente solo constructor y destructor; la copia y la asignacion quedan
generadas por el compilador. Por tanto, en la correccion es mas exacto decir
que la clase tiene constructor y destructor definidos, pero no implementar una
forma canonica completa de cuatro miembros.

### 2. HTTP obligatorio

Con el servidor arrancado:

```sh
curl -i http://127.0.0.1:8081/
curl -i http://127.0.0.1:8081/missing
curl -i -X POST --data 'small body' http://127.0.0.1:8081/
curl -i -X DELETE http://127.0.0.1:8081/uploads/un-archivo.txt
```

Demostrar tambien:

- `405` cuando el metodo no esta permitido en la location.
- `404` para recursos inexistentes.
- `413` para cuerpos demasiado grandes.
- `400`, `414`, `431`, `501` y `505` con peticiones malformadas o no soportadas.
- Upload binario y `multipart/form-data`.
- `Transfer-Encoding: chunked`.
- Respuesta parcial y peticiones enviadas en fragmentos.
- `Host` para seleccionar virtual hosts.
- Dos puertos de escucha y paginas de error personalizadas.

### 3. CGI obligatorio: demostrar primero el caso sencillo

El caso sencillo debe poder demostrarse con una sola extension y un solo interprete. La configuracion minima de la location seria, por ejemplo:

```nginx
location /cgi-bin {
    allowed_methods GET POST;
    root ./www/cgi-bin;
    cgi_extension .py /usr/bin/python3;
}
```

Pruebas:

```sh
curl -i http://127.0.0.1:8081/cgi-bin/echo.py
curl -i -X POST --data 'hello CGI' http://127.0.0.1:8081/cgi-bin/echo.py
```

Hay que explicar y observar:

- `fork`, `pipe`, `dup2` y `execve`.
- Variables CGI: metodo, query string, content length/type, servidor y ruta.
- Entrada POST por el pipe del hijo.
- Salida CGI convertida en respuesta HTTP.
- Error del script convertido en `500`.
- Timeout convertido en `504`.
- Limpieza del proceso hijo y ausencia de zombies.

No hace falta mantener dos implementaciones distintas. Es mejor conservar un unico flujo de CGI y preparar una configuracion de demostracion sencilla. Asi se prueba explicitamente la parte obligatoria y no se arriesga el codigo estable solo para separar versiones.

### 4. Bonus: multiples CGI

El bonus actual se demuestra con dos extensiones y dos interpretes:

```nginx
cgi_extension .py /usr/bin/python3;
cgi_extension .sh /bin/bash;
```

Pruebas sugeridas:

```sh
curl -i http://127.0.0.1:8081/cgi-bin/echo.py
curl -i http://127.0.0.1:8081/cgi-bin/echo.sh
```

La respuesta que conviene dar en la correccion es: el caso sencillo es el mismo mecanismo con una sola entrada `cgi_extension`; el bonus es la tabla de extension a interprete con varias entradas. Preparar ambos escenarios en configuraciones o comandos separados es suficiente y es mas defendible que duplicar `CGIHandler`.

## Puntos que aun toca trabajar antes de la correccion

Prioridad alta:

- **Preparar una demo del CGI sencillo.** Crear o dejar localizable un script Python que funcione con GET y POST, y demostrarlo con una configuracion que solo tenga `.py`. Despues repetir con `.sh` para el bonus.
- **Probar el limite de 16 CGI.** El smoke test comprueba timeout y concurrencia general, pero conviene lanzar 17 scripts lentos y verificar que alguno recibe `503`, que los procesos terminan y que el servidor sigue respondiendo.
- **Ejecutar `make leaks`.** Hacerlo con valgrind disponible y guardar el resultado para la mesa. Revisar tambien descriptores abiertos despues de CGI y despues de clientes abortados.
- **Probar configuraciones invalidas.** Puerto fuera de rango, host invalido, root inexistente, interprete CGI no ejecutable, location duplicada, directiva duplicada y bloques sin cerrar.
- **Revisar el subject exacto de bonus.** Llevar una lista que distinga claramente obligatorio, bonus de multiples CGI y cualquier bonus adicional que se quiera presentar.

Prioridad media:

- Probar `GET` con query string y comprobar `QUERY_STRING`.
- Probar headers con mayusculas/minusculas, headers duplicados, `Content-Length` conflictivo y `Transfer-Encoding` no soportado.
- Probar chunked malformado, chunk demasiado grande y cuerpo que supera `client_max_body_size` durante la transferencia.
- Probar rutas con `..`, symlinks, permisos de lectura/escritura y `DELETE` sobre archivos protegidos.
- Probar dos clientes simultaneos mientras un CGI lento esta activo.
- Comprobar que la seleccion de location usa la ruta esperada cuando hay prefijos solapados.
- Revisar manualmente `autoindex`, `index`, `return` y el comportamiento cuando falta una pagina de error personalizada.

Prioridad baja, pero recomendable:

- Repetir el smoke test varias veces para detectar carreras.
- Probar cierres abruptos del cliente durante upload y durante CGI.
- Verificar que no quedan archivos de prueba en `www/uploads` ni scripts temporales en `www/cgi-bin`.
- Tener preparada una explicacion breve de los limites: timeout CGI de 10 s y salida CGI maxima de 16 MiB.

## Checklist de la mesa

- [ ] `make` sin warnings.
- [ ] Arranque con configuracion valida.
- [ ] Error claro ante configuracion invalida.
- [ ] GET estatico y pagina index.
- [ ] POST y upload binario/multipart.
- [ ] DELETE y permisos.
- [ ] Errores HTTP y paginas personalizadas.
- [ ] Chunked y peticion fragmentada.
- [ ] Virtual host y multiples listeners.
- [ ] CGI sencillo con un interprete.
- [ ] CGI bonus con dos interpretes.
- [ ] CGI con POST, error, timeout y salida grande.
- [ ] Limite de procesos CGI y respuesta `503`.
- [ ] Sin zombies, fugas ni descriptores abandonados.
- [ ] `make test` pasa.

## Archivos clave

- `Makefile`: compilacion, pruebas y valgrind.
- `config/webserv.conf`: configuracion de demostracion.
- `srcs/Server.cpp`: sockets, `epoll`, clientes y ciclo de vida CGI.
- `srcs/Request.cpp`: parser HTTP incremental y cuerpos.
- `srcs/CGIHandler.cpp`: entorno y lanzamiento CGI.
- `srcs/ConfigParser.cpp`: parser de configuracion.
- `srcs/ConfigValidator.cpp`: validacion semantica.
- `tests/smoke_test.sh`: bateria automatica de regresion.
- `Cambios-Y-Cosas-por-hacer.md.md`: notas tecnicas de cambios recientes.
