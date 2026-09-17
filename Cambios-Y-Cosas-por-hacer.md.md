Orden real del código: conexión, lectura, selección del virtual host, validaciones, CGI o recurso estático, y finalmente respuesta mediante `epoll`. También distinguiré qué ocurre cuando cambian la IP, el puerto o el archivo de configuración.

Read , lines 500 to 625
Read , lines 670 to 850
Read , lines 1080 to 1240
Read , lines 270 to 340

## 1. Flujo general de una petición
Cuando un cliente hace:

```bash
curl -X POST --data-binary @archivo.txt \
  http://127.0.0.1:8081/uploads/archivo.txt
```

el flujo de tu servidor es:

```text
socket de escucha
    |
    v
accept()
    |
    v
cliente registrado en epoll
    |
    v
recv()
    |
    v
Request::parse()
    |
    v
selección del virtual host
    |
    v
selección de location
    |
    v
comprobación body size
    |
    v
comprobación del método
    |
    v
comprobación de seguridad de la ruta
    |
    v
¿es CGI?
    |       \
   sí       no
    |        |
   CGI    GET / POST / DELETE
```

---

# 2. ¿Cómo funciona `epoll`?

## ¿Qué es un descriptor?

En Linux, muchas cosas se representan con un número entero llamado **file descriptor**:

```text
socket de escucha       -> descriptor
conexión de un cliente   -> descriptor
pipe del CGI             -> descriptor
archivo abierto         -> descriptor
```

Tu servidor mantiene mapas para saber qué representa cada descriptor:

```cpp
std::map<int, ListenerState> _listeners;
std::map<int, ClientState> _clients;
std::map<int, CgiPipeRef> _cgiPipeRefs;
```

Por ejemplo:

```text
fd 4 -> listener en 127.0.0.1:8081
fd 5 -> cliente HTTP
fd 6 -> stdout de un CGI
fd 7 -> stdin de un CGI
```

## ¿Por qué no necesita un hilo por conexión?

Sin `epoll`, una aproximación simple sería:

```text
crear un hilo para cliente 1
crear un hilo para cliente 2
crear un hilo para cliente 3
...
```

Eso consume más memoria y aumenta la complejidad.

Con `epoll`, tienes un solo bucle principal:

```cpp
while (true)
{
    epoll_wait(...);
    procesar eventos;
}
```

El kernel vigila todos los descriptores registrados y solo despierta al servidor cuando alguno está listo.

El servidor no pregunta continuamente:

```text
¿cliente 1 tiene datos?
¿cliente 2 tiene datos?
¿cliente 3 tiene datos?
```

En lugar de eso, el kernel le devuelve directamente:

```text
fd 5 está listo para leer
fd 6 está listo para leer
fd 8 está listo para escribir
```

Por eso `epoll` escala mejor que revisar conexión por conexión.

---

# 3. Los tres tipos principales de eventos

En tu `Server::run()`:

```cpp
const int count = epoll_wait(_epollFd, events, 128, 1000);
```

Después se revisa cada descriptor:

```cpp
if (_listeners.find(fd) != _listeners.end())
    _acceptClients(fd);
else if (_cgiPipeRefs.find(fd) != _cgiPipeRefs.end())
    _handleCgiEvent(fd, events[i].events);
else if (_clients.find(fd) != _clients.end())
    _handleClientEvent(fd, events[i].events);
```

## Caso A: es un listener

```text
¿Es un listener?
    |
    +-- sí -> aceptar clientes nuevos
```

El listener es el socket que escucha en:

```text
127.0.0.1:8081
```

Se registra con:

```cpp
event.events = EPOLLIN;
```

En el listener, `EPOLLIN` significa:

```text
hay una conexión nueva esperando
```

Entonces se llama a:

```cpp
_acceptClients(listenerFd);
```

Dentro se ejecuta:

```cpp
accept(listenerFd, NULL, NULL);
```

Esto crea un nuevo descriptor para el cliente.

Ejemplo:

```text
fd 4 -> listener 127.0.0.1:8081
fd 5 -> cliente nuevo
```

El nuevo cliente se añade a `epoll`:

```cpp
event.events = EPOLLIN | EPOLLRDHUP;
epoll_ctl(_epollFd, EPOLL_CTL_ADD, clientFd, &event);
```

Y se guarda información sobre él:

```cpp
state.listenPort = listener->second.port;
state.listenHost = listener->second.host;
```

Esto es importante porque después permite saber por qué listener entró la conexión.

---

## Caso B: es un cliente

```text
¿Es un cliente?
    |
    +-- EPOLLIN  -> leer petición
    +-- EPOLLOUT -> enviar respuesta
    +-- EPOLLRDHUP -> cliente cerró conexión
```

`EPOLLIN` significa que el cliente ha enviado datos.

Entonces se llama a:

```cpp
_handleClientReadable(clientFd);
```

El servidor recibe una parte de la petición:

```cpp
recv(clientFd, buffer, sizeof(buffer), 0);
```

El buffer tiene `8192` bytes, pero una petición puede ser más grande o llegar fragmentada.

Por eso los datos se pasan progresivamente a:

```cpp
client->second.request.parse(...);
```

Ejemplo de una petición dividida:

```text
recv 1:
POST /uploads/file.txt HTTP/1.1

recv 2:
Host: localhost
Content-Length: 10

recv 3:
Hola mundo
```

`Request` va acumulando todo hasta que la petición está completa.

Cuando termina:

```cpp
_processRequest(...)
```

procesa la petición.

---

## Caso C: es un pipe de CGI

```text
¿Es un pipe de CGI?
    |
    +-- pipe de entrada -> escribir body hacia CGI
    +-- pipe de salida  -> leer respuesta del CGI
```

Cuando se ejecuta un CGI, tienes dos direcciones:

```text
servidor -> stdin del CGI
stdout del CGI -> servidor
```

El servidor registra los pipes en `epoll`.

Para el pipe de salida:

```cpp
outputEvent.events = EPOLLIN | EPOLLRDHUP;
```

Esto significa:

```text
el CGI produjo datos para leer
```

Para el pipe de entrada:

```cpp
inputEvent.events = EPOLLOUT | EPOLLRDHUP;
```

Esto significa:

```text
el pipe puede recibir más datos
```

Así el servidor puede atender simultáneamente:

```text
cliente A haciendo GET
cliente B enviando POST
cliente C ejecutando CGI
cliente D descargando un archivo
```

sin crear un hilo individual para cada conexión.

---

# 4. Flujo de comprobación del body size

Hay dos momentos importantes.

## Primer momento: después de leer las cabeceras

Cuando ya están disponibles las cabeceras:

```cpp
_updateClientBodyLimit(clientFd);
```

El servidor ya puede leer el header:

```http
Host: limited.localhost
Content-Length: 2048
```

Con el `Host`, selecciona el virtual host correcto y obtiene su:

```conf
client_max_body_size 1K;
```

Entonces actualiza el límite del objeto `Request`.

## Segundo momento: al procesar la petición

En `_processRequest()` se comprueba:

```cpp
if (server->client_max_body_size > 0 &&
    request.getBody().size() >
        static_cast<std::size_t>(server->client_max_body_size))
{
    _sendErrorResponse(clientFd, 413, server, location);
    return;
}
```

Si el body supera el límite:

```text
413 Payload Too Large
```

El flujo se detiene ahí. No se ejecuta CGI ni se guarda un upload.

Ejemplo:

```conf
client_max_body_size 1K;
```

y una petición de `2048` bytes:

```text
body = 2048 bytes
limit = 1024 bytes
2048 > 1024
respuesta = 413
```

Si el valor es:

```conf
client_max_body_size 0;
```

en tu código significa sin límite.

---

# 5. Comprobación de métodos

Después del body size se busca la `location` correspondiente:

```cpp
const LocationConfig* location =
    _matchLocation(*server, request.getPath());
```

Por ejemplo:

```text
/uploads/file.txt
```

coincide con:

```conf
location /uploads {
    allowed_methods GET POST;
}
```

Después se comprueba:

```cpp
if (!_isMethodAllowed(location, request.getMethod()))
{
    _sendErrorResponse(clientFd, 405, server, location);
    return;
}
```

Ejemplos:

```text
GET  /uploads/file.txt -> permitido
POST /uploads/file.txt -> permitido
DELETE /uploads/file.txt -> 405
```

La configuración:

```conf
allowed_methods GET POST;
```

significa:

```text
GET  -> download
POST -> upload
DELETE -> bloqueado
```

El método se comprueba antes de ejecutar la lógica de `GET`, `POST`, `DELETE` o CGI.

---

# 6. Protección contra path traversal

Después del método se revisa:

```cpp
_hasPathTraversal(request.getPath())
```

Esto bloquea rutas como:

```text
/uploads/../../etc/passwd
```

También detecta formas codificadas como:

```text
/uploads/%2e%2e/%2e%2e/etc/passwd
```

Si encuentra un segmento:

```text
..
```

devuelve:

```text
403 Forbidden
```

---

# 7. Protección contra symlinks

En `_handleDelete()` se construye la ruta real:

```cpp
const std::string fullPath =
    _resolvePath(server, location, request.getPath());
```

Después se intenta abrir con:

```cpp
const int fileFd =
    open(fullPath.c_str(), O_RDONLY | O_NOFOLLOW);
```

`O_NOFOLLOW` le dice al sistema:

```text
no sigas un enlace simbólico al abrir esta ruta
```

Si la ruta es un symlink, `open()` falla con `ELOOP`:

```cpp
if (fileFd < 0)
    return _buildErrorResponse(
        errno == ELOOP ? 403 : 404,
        &server,
        location);
```

Resultado:

```text
symlink -> 403 Forbidden
archivo inexistente -> 404 Not Found
```

Después se cierra el descriptor:

```cpp
close(fileFd);
```

Y se usa `stat()` para comprobar que es un archivo normal:

```cpp
if (!S_ISREG(fileStat.st_mode))
    return _buildErrorResponse(403, &server, location);
```

Finalmente se elimina:

```cpp
std::remove(fullPath.c_str());
```

## Observación importante

La protección actual evita que `open()` siga un symlink. Sin embargo, entre el `open()` y el `std::remove()` existe una pequeña ventana de tiempo. Es una limitación del diseño actual porque la lista de funciones permitidas no incluye APIs más específicas como `unlinkat()`.

Para el requisito del proyecto, la comprobación con `O_NOFOLLOW` es la opción compatible con las funciones permitidas.

---

# 8. Comprobación y ejecución progresiva de CGI

El CGI no se ejecuta directamente al recibir cualquier petición.

Primero se ejecutan las validaciones normales:

```text
1. Request completa
2. Virtual host seleccionado
3. Location encontrada
4. Body size válido
5. Método permitido
6. Path traversal descartado
```

Después se resuelve la ruta:

```cpp
const std::string fullPath =
    _resolvePath(*server, location, request.getPath());
```

Ejemplo:

```text
URL:
    /cgi-bin/echo.py

root:
    ./www/cgi-bin

ruta:
    ./www/cgi-bin/echo.py
```

El servidor comprueba:

```cpp
stat(fullPath.c_str(), &fileStat)
```

y confirma que sea un archivo regular:

```cpp
S_ISREG(fileStat.st_mode)
```

Después busca un intérprete configurado:

```cpp
_findCgiInterpreter(location, fullPath, interpreter)
```

Por ejemplo:

```conf
cgi_extension .py /usr/bin/python3;
```

significa:

```text
archivo .py -> ejecutar con /usr/bin/python3
```

Si no hay intérprete configurado, no se trata como CGI.

## Límite de procesos CGI

Antes de crear otro proceso:

```cpp
if (_cgiByPid.size() >= CGI_MAX_PROCESSES)
{
    _sendErrorResponse(clientFd, 503, server, location);
    return;
}
```

Tu límite es:

```cpp
CGI_MAX_PROCESSES = 16;
```

Si ya hay 16 CGI ejecutándose:

```text
503 Service Unavailable
```

## Inicio del CGI

Después `_startCgi()` comprueba de nuevo:

```cpp
_findCgiInterpreter(...)
access(fullPath.c_str(), R_OK)
```

Luego `CGIHandler::execute()` crea el proceso y los pipes.

El servidor guarda:

```text
PID del CGI
stdin del CGI
stdout del CGI
cliente asociado
body que debe enviar
hora de inicio
```

El body del `POST` se envía progresivamente por el pipe de entrada.

La salida se lee progresivamente por el pipe de salida.

El servidor no espera bloqueado haciendo una lectura infinita. `epoll` le avisa cuándo puede leer o escribir.

## Finalización del CGI

El servidor revisa periódicamente:

```cpp
_reapCgiProcesses();
```

usando:

```cpp
waitpid(pid, &status, WNOHANG);
```

`WNOHANG` significa:

```text
comprueba si terminó, pero no bloquees el servidor
```

Cuando el CGI:

1. Terminó.
2. Cerró su salida.
3. No excedió el tamaño máximo.

se crea la respuesta HTTP:

```cpp
_queueResponse(clientFd, _buildCgiHttpResponse(output));
```

Casos de error:

```text
CGI termina con error        -> 500
CGI tarda demasiado          -> 504
CGI genera demasiada salida  -> 502
más de 16 CGI activos        -> 503
```

El timeout de CGI es:

```cpp
CGI_TIMEOUT_SECONDS = 120;
```

---

# 9. Flujo de virtual hosts

Supón esta configuración:

```conf
server {
    listen 127.0.0.1:8081;
    server_name alpha.localhost;
    root ./www;
}

server {
    listen 127.0.0.1:8081;
    server_name beta.localhost;
    root ./YoupiBanane;
}
```

Ambos usan:

```text
IP:     127.0.0.1
puerto: 8081
```

pero tienen distinto:

```text
server_name
root
```

## Paso 1: conexión TCP

El cliente se conecta a:

```text
127.0.0.1:8081
```

El sistema operativo no sabe todavía si quiere `alpha` o `beta`. Solo sabe que llegó al socket del puerto `8081`.

## Paso 2: el servidor acepta

`accept()` crea la conexión del cliente.

Tu `ClientState` guarda:

```cpp
listenPort = 8081;
listenHost = 127.0.0.1;
```

## Paso 3: llega el header Host

El cliente envía:

```http
Host: alpha.localhost
```

o:

```http
Host: beta.localhost
```

## Paso 4: selección

`_selectServerConfig()`:

1. Lee `Host`.
2. Elimina el puerto si viene así:

```http
Host: alpha.localhost:8081
```

3. Convierte a minúsculas.
4. Busca configuraciones con el mismo listener.
5. Compara `server_name`.

Resultado:

```text
Host: alpha.localhost -> server alpha
Host: beta.localhost  -> server beta
```

Si no hay coincidencia, usa el primer `server` que encontró para esa IP y puerto.

---

# 10. Varios `server` con la misma IP y mismo puerto

Ejemplo:

```conf
server {
    listen 127.0.0.1:8081;
    server_name alpha.localhost;
}

server {
    listen 127.0.0.1:8081;
    server_name beta.localhost;
}
```

Resultado:

```text
un solo socket TCP
varias configuraciones internas
selección mediante Host
```

Tu código evita abrir el mismo listener dos veces:

```cpp
if (it->second.host == _servers[i].host &&
    it->second.port == _servers[i].port)
{
    alreadyOpen = true;
}
```

Esto es el caso típico de virtual hosting basado en nombre.

---

# 11. Varios `server` con distinta IP

Ejemplo:

```conf
server {
    listen 127.0.0.1:8081;
    server_name local.localhost;
}

server {
    listen 127.0.0.2:8081;
    server_name other.localhost;
}
```

Aquí sí se abren dos listeners porque cambia la combinación:

```text
127.0.0.1:8081
127.0.0.2:8081
```

El flujo sería:

```text
conexión a 127.0.0.1:8081
    -> listener de 127.0.0.1
    -> solo servidores asociados a esa IP

conexión a 127.0.0.2:8081
    -> listener de 127.0.0.2
    -> solo servidores asociados a esa IP
```

Aunque el `Host` diga otro nombre, `_selectServerConfig()` primero exige que coincidan:

```cpp
_servers[i].port == listenPort
_servers[i].host == listenHost
```

Por eso un virtual host de `127.0.0.2` no se selecciona desde una conexión que entró por `127.0.0.1`.

---

# 12. Distinta IP y mismo archivo de configuración

Puedes ejecutar:

```bash
./webserv config/multi-ip.conf
```

y tener en ese archivo:

```conf
server {
    listen 127.0.0.1:8081;
    server_name site-one.localhost;
}

server {
    listen 127.0.0.2:8081;
    server_name site-two.localhost;
}
```

El mismo proceso lee un solo archivo, crea dos `ServerConfig` y abre dos listeners:

```text
fd 4 -> 127.0.0.1:8081
fd 5 -> 127.0.0.2:8081
```

Ambos descriptores se registran en el mismo `epoll`.

Por eso el bucle puede recibir:

```text
evento en fd 4 -> aceptar cliente de 127.0.0.1
evento en fd 5 -> aceptar cliente de 127.0.0.2
```

---

# 13. Misma IP y distinto archivo de configuración

Puedes ejecutar dos procesos:

```bash
./webserv config/site-a.conf
./webserv config/site-b.conf
```

Pero no pueden escuchar los dos en la misma combinación:

```text
127.0.0.1:8081
```

El segundo proceso fallará con:

```text
Address already in use
```

Cada proceso tiene su propio:

```text
epoll
listeners
clientes
configuración
```

No comparten automáticamente los virtual hosts.

Para que funcionen los dos procesos debes usar puertos distintos:

```bash
./webserv config/site-a.conf
./webserv config/site-b.conf PORT+1
```

Si ambos archivos tienen:

```conf
listen 127.0.0.1:8081;
```

el segundo, usando `PORT+1`, se iniciará en:

```text
127.0.0.1:8082
```

---

# 14. Resumen de combinaciones

| Configuración | Resultado |
|---|---|
| Misma IP, mismo puerto, distintos `server_name` | Un listener, selección por `Host` |
| Distinta IP, mismo puerto | Un listener por IP |
| Misma IP, distinto puerto | Un listener por puerto |
| Mismo archivo, varias IP/puertos | Un proceso con varios listeners dentro del mismo `epoll` |
| Distintos archivos, misma IP y puerto | Conflicto: `Address already in use` |
| Distintos archivos, misma IP y distinto puerto | Dos procesos independientes |
| Mismo `server_name` repetido en la misma IP y puerto | Se selecciona la primera coincidencia |
| `Host` desconocido | Se usa el primer server de ese listener |

La idea central de tu arquitectura es:

```text
epoll decide qué descriptor necesita atención.

El listener acepta conexiones.

El cliente recibe y envía HTTP.

El pipe transporta datos de CGI.

El Host selecciona el virtual host.

El virtual host decide root, límites, locations y errores.
```

## Pruebas manuales
Claro, una **guía manual** con tres pruebas.

## 1. Probar `CGI_MAX_PROCESSES = 1`

### Terminal 1: compilar y arrancar

```bash
cd /mnt/c/Users/VanessaL/Documents/practice/websrv-webbita
make
./webserv config/webserv.conf
```

### Terminal 2: ocupar el único proceso CGI

```bash
curl -i --max-time 35 \
  http://127.0.0.1:8081/cgi-bin/hold.py
```

Mientras esta petición sigue abierta, comprueba el proceso:

```bash
ps -ef | grep '[h]old.py'
```

### Terminal 3: lanzar otro CGI

```bash
curl -i --max-time 5 \
  http://127.0.0.1:8081/cgi-bin/echo.py
```

Resultado esperado:

```text
HTTP/1.1 503 Service Unavailable
```

Cuando termine el primer CGI, la misma petición debería devolver:

```text
HTTP/1.1 200 OK
```

El primer `hold.py` debería terminar con `504`, porque tarda 30 segundos pero el límite es:

```cpp
CGI_TIMEOUT_SECONDS = 25;
```

---

## 2. Probar `client_max_body_size`

En `webserv.conf` el virtual host `localhost2` tiene:

```conf
client_max_body_size 4M;
```

### Crear un archivo válido de 1 MB

```bash
dd if=/dev/zero of=/tmp/body-1m.bin bs=1M count=1
```

Enviar el archivo:

```bash
curl -i \
  -H 'Host: localhost2' \
  -X POST \
  --data-binary @/tmp/body-1m.bin \
  http://127.0.0.1:8081/cgi-bin/echo.py
```

Resultado esperado:

```text
HTTP/1.1 200 OK
```

### Crear un archivo demasiado grande

```bash
dd if=/dev/zero of=/tmp/body-5m.bin bs=1M count=5
```

Enviar el archivo:

```bash
curl -i \
  -H 'Host: localhost2' \
  -X POST \
  --data-binary @/tmp/body-5m.bin \
  http://127.0.0.1:8081/cgi-bin/echo.py
```

Resultado esperado:

```text
HTTP/1.1 413 Payload Too Large
```

Esto confirma que la petición se rechaza antes de ejecutar el CGI.

También puedes comprobar el límite usando `Transfer-Encoding: chunked`:

```bash
curl -i \
  -H 'Host: localhost2' \
  -H 'Transfer-Encoding: chunked' \
  -X POST \
  --data-binary @/tmp/body-5m.bin \
  http://127.0.0.1:8081/cgi-bin/echo.py
```

Debe devolver igualmente `413`.

---

## 3. Probar error interno de CGI

Este CGI ya existe en el smoke test, pero puedes crearlo manualmente:

```bash
cat > www/cgi-bin/fail.py <<'PY'
#!/usr/bin/env python3
raise RuntimeError("intentional failure")
PY

chmod +x www/cgi-bin/fail.py
```

Probarlo:

```bash
curl -i \
  http://127.0.0.1:8081/cgi-bin/fail.py
```

Resultado esperado:

```text
HTTP/1.1 500 Internal Server Error
```

Esto verifica que el servidor detecta que el proceso CGI termina con estado de error.

Para limpiar:

```bash
rm -f www/cgi-bin/fail.py /tmp/body-1m.bin /tmp/body-5m.bin
```

Para detener el servidor:

```text
Ctrl+C
```

Las tres pruebas cubren:

```text
CGI_MAX_PROCESSES  -> 503
client_max_body_size -> 413
CGI con error interno -> 500
```