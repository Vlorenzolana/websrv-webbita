# Continuación del trabajo

## Cambio reciente: límite y drenaje de CGI

- Se limita a 16 el número de procesos CGI activos simultáneamente. Cada CGI
    puede registrar uno o dos pipes en `epoll`: salida y, si tiene body, entrada.
    Las peticiones que superarían el límite de procesos reciben `503`.
- Los extremos del padre se mantienen en modo no bloqueante y conservan las
    flags existentes del descriptor al añadir `O_NONBLOCK`.
- Cada evento de `epoll` realiza como máximo una lectura o escritura no
    bloqueante. Como el registro es level-triggered, `epoll` vuelve a notificar
    mientras quede trabajo pendiente, sin consultar `errno` después de
    `read` o `write`.
- Los sockets de cliente aplican el mismo criterio: un `recv()` o `send()` por
    evento de `epoll`, con offsets para continuar respuestas parciales.
- El límite CGI devuelve `503 Service Unavailable` con el texto HTTP estándar.
- Los procesos terminados se revisan con `waitpid(..., WNOHANG)` para evitar
    zombies y permitir que el servidor continúe atendiendo conexiones.
- Se mantiene el timeout de CGI de 10 segundos y el límite de salida de 16 MiB.

La compilación y el smoke test fueron ejecutados en WSL porque el proyecto usa
`fork`, `pipe`, `epoll` y `waitpid`:

```text
All smoke tests passed on port 18080.
```

## 1. Correcciones implementadas

### Compilación y ejecución

```sh
make
./webserv config/webserv.conf
```

### Correcciones principales

- I/O de clientes alineado con el modelo CGI: `_handleClientReadable()` hace
    un único `recv()` por evento `EPOLLIN` y `_flushResponse()` hace un único
    `send()` por evento `EPOLLOUT`. Los offsets permiten continuar una lectura
    de petición o una respuesta parcial en la siguiente notificación de
    `epoll`, sin loops adicionales sobre el mismo descriptor.

- Límite CGI con respuesta HTTP estándar: cuando ya hay 16 procesos CGI
    activos, la petición nueva recibe `503 Service Unavailable`. El texto de
    estado también se usa en el cuerpo HTML de error si no hay una página
    personalizada configurada.

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

- Resolución del host de escucha mediante `getaddrinfo` y `freeaddrinfo`, en
    lugar de `inet_pton`, con mensajes de error propios.

```cpp
struct addrinfo hints;
struct addrinfo* addresses = NULL;
std::memset(&hints, 0, sizeof(hints));
hints.ai_family = AF_INET;
hints.ai_socktype = SOCK_STREAM;
hints.ai_flags = AI_PASSIVE;

getaddrinfo(server.host.c_str(), port.c_str(), &hints, &addresses);
freeaddrinfo(addresses);
```

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

- Páginas de error personalizadas para los códigos HTTP que puede generar el
    servidor. Se mantienen páginas específicas para `400`, `403`, `404`, `405`,
    `413`, `414`, `431`, `501`, `503` y `505`, además de la página común `50x.html`
    para `500` y `504`.

```cpp
// srcs/ConfigParser.cpp
else if (tokens[0] == "error_page")
    _parseErrorPage(server.error_pages, line);
```

```nginx
error_page 400 /errors/400.html;
error_page 403 /errors/403.html;
error_page 405 /errors/405.html;
error_page 413 /errors/413.html;
error_page 414 /errors/414.html;
error_page 431 /errors/431.html;
error_page 404 /errors/404.html;
error_page 501 /errors/501.html;
error_page 503 /errors/503.html;
error_page 505 /errors/505.html;
error_page 500 504 /errors/50x.html;
```

El servidor carga la página configurada para el código y, si no existe una
configuración específica, genera un cuerpo HTML de error por defecto. Los
errores de arranque usan mensajes propios, por ejemplo `Invalid listen host`,
`Could not bind listener`, `Could not start listener` y `Could not register
listener in epoll`, sin convertir `errno` en texto mediante `strerror`.

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
    _pendingResponses.find(clientFd) != _pendingResponses.end())
    _handleClientWritable(clientFd);
```

- Timeout de clientes inactivos y limpieza de descriptores con un único propietario.

```cpp
// srcs/Server.cpp
_checkTimeouts();

```nginx
    allowed_methods GET POST;
    upload_path ./www/uploads;
    cgi_extension .py /usr/bin/python3;
    cgi_extension .sh /bin/bash;
```
### Prueba rápida incluida

```sh
make test
```

El script usa el puerto 18080 por defecto. Si queremos se puede pasar otro puerto directamente:

```sh
./tests/smoke_test.sh 18090
```

El smoke test incluye:

- Puerto configurable, partiendo de `127.0.0.1:8081` en la configuración real.
- Peticiones fragmentadas byte a byte.
- Comprobaciones de `405`, `413`, `501` y `403`.
- Configuración de páginas personalizadas para `400`, `403`, `404`, `405`,
  `413`, `414`, `431`, `501` y `505`, además de `500` y `504`; el smoke test
  verifica explícitamente el cuerpo personalizado de `404` y los códigos de
  respuesta de varios casos negativos.
- Prueba de `DELETE` y confirmación de que el archivo se elimina.
- Pruebas de concurrencia repetidas.
- Uploads binarios y multipart, transferencia chunked, CGI, timeout `504` y
    detección de procesos zombie.

La ejecución verificada es:

```text
All smoke tests passed on port 18080.
```

## 2. Verificación realizada

### Comprobado:

- Compilación correcta:
    - Comando ejecutado: `make`
  - Resultado: se generó el binario `webserv` y la compilación terminó con éxito.

- Funcionalidad básica verificada en ejecución real:
    - Las peticiones de la prueba smoke se ejecutaron en el puerto configurable
        `18080` y devolvieron los códigos esperados.
    - La configuración base usa `127.0.0.1:8081`; el smoke test la adapta al
        puerto indicado sin modificar el archivo original.

### Puntos del código que cumplen

- Makefile:
  - Define `NAME = webserv`
  - Tiene `all`, `clean`, `fclean` y `re`
  - Usa `-std=c++98`

- main.cpp:
  - Acepta un archivo de configuración como argumento y usa uno por defecto si no se pasa.

- Server.cpp:
    - Usa `epoll` (`epoll_create`, `epoll_ctl`, `epoll_wait`)
    - Usa `fcntl` para conservar las flags existentes y añadir `O_NONBLOCK`

- CGIHandler.cpp:
  - Implementa el flujo CGI con `fork`, `pipe`, `dup2` y `execve`

### Estado final

- ✅ Cumple los puntos principales y más críticos del subject.
- ⚠️ La comprobación exhaustiva de fugas de descriptores, permisos y algunas
    variantes de configuración requiere pruebas manuales adicionales.

### Validación del cambio de I/O

```sh
wsl --cd /mnt/c/Users/VanessaL/Documents/practice/websrv-webbita -- make clean
wsl --cd /mnt/c/Users/VanessaL/Documents/practice/websrv-webbita -- make test
```

Resultado verificado:

```text
All smoke tests passed on port 18080.
```

## 3. Explicación técnica del parser y del CGI

### 3.1. ConfigParser

Lo nuevo en esta parte es que el parser lee el archivo de configuración, valida la sintaxis básica y cambia de contexto según si está en el ámbito global, en un bloque `server` o en un bloque `location`.

Qué hace bien:
- Detecta si está dentro de un contexto global, de servidor o de location.
- Interpreta directivas como `listen`, `server_name`, `root`, `error_page` y `client_max_body_size`.
- Valida y normaliza la configuración antes de entregarla al resto del servidor.
- Gestiona errores simples de configuración.

```cpp
// srcs/ConfigParser.cpp
std::vector<ServerConfig> ConfigParser::parseFile(const std::string& filename)
{
    std::ifstream file(filename.c_str());
    if (!file.is_open())
        throw std::runtime_error("Could not open config file: " + filename);

    // Cada línea se normaliza, se tokeniza y se procesa según GLOBAL,
    // SERVER o LOCATION. La validación semántica se ejecuta al final.
    // ...
    ConfigValidator validator;
    validator.validateAndNormalize(servers);
    return servers;
}
```

El parser real usa `ServerConfig::host`, `ServerConfig::port` y
`ServerConfig::root_directory`; no usa campos llamados `listen_port` o `root`.
También comprueba los puntos y coma, directivas duplicadas, rangos de puertos,
raíces, páginas de error y bloques sin cerrar.

### 3.2. CGIHandler

Este archivo ejecuta scripts CGI. Su función es crear un entorno correcto para
que el script pueda funcionar, capturar su salida y convertirla en una respuesta
HTTP.

Qué hay que mirar:
- La creación del proceso hijo.
- La preparación de variables de entorno.
- La entrada y salida del script.
- El manejo de errores y respuestas.

El método real es `CGIHandler::execute(...)`. Construye un mapa de variables
CGI (`REQUEST_METHOD`, `QUERY_STRING`, `CONTENT_LENGTH`, `CONTENT_TYPE`, entre
otras), lo convierte en un `envp` para `execve`, y no usa `setenv`.

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

Después crea el proceso con `fork`, conecta las tuberías mediante `dup2`, hace
`chdir` al directorio del script y ejecuta el intérprete configurado con
`execve`. El padre conserva el extremo de escritura de entrada y el de lectura
de salida para gestionarlos de forma asíncrona con `epoll`.

Versión mejorada: configura los descriptores de fichero.

```cpp
if (!_setNonBlocking(inputPipe[1]) || !_setNonBlocking(outputPipe[0]))
{
    _closePipe(inputPipe);
    _closePipe(outputPipe);
    return false;
}
```

Se utiliza `fcntl` con `F_GETFL` y `F_SETFL` para conservar las flags existentes
del descriptor y añadir `O_NONBLOCK`. El servidor también configura
`FD_CLOEXEC` donde corresponde para evitar herencias accidentales de
descriptores.

Nota sobre `O_NONBLOCK` y macOS:
`O_NONBLOCK` se usa junto con `fcntl(..., F_SETFL, O_NONBLOCK)` para poner un descriptor en modo no bloqueante. Sin eso, operaciones como `read`, `write`, `accept` o `recv` podrían quedarse esperando indefinidamente y bloquear el servidor. En macOS esta es la forma estándar y portable de habilitar I/O no bloqueante, por eso se incluye en el código.

## 4. Casos que necesito que compruebes

La prueba debe ejecutarse contra la configuración real del repositorio. En
`config/webserv.conf` el listener actual es `127.0.0.1:8081`, la ruta CGI es
`/cgi-bin` y la ruta de subida es `/uploads`.

### 4.1. Arranque y configuración

1. Compilar con `make` usando C++98 y `-Werror`.
2. Arrancar con la configuración válida y confirmar que anuncia `Listening`.
3. Arrancar con un host inválido y comprobar que termina con un mensaje propio,
    sin mostrar `strerror(errno)`.
4. Arrancar con un puerto ocupado y comprobar que libera los recursos ya
    abiertos y termina limpiamente.
5. Probar configuraciones con directivas duplicadas, puerto `0`, puerto `65536`,
    host vacío, raíz inexistente y página de error ilegible.
6. Confirmar que solo se usan las funciones externas permitidas por el subject.

### 4.2. HTTP fragmentado y límites

1. Enviar la línea de petición, cabeceras y cuerpo en escrituras de un byte.
2. Enviar varias peticiones consecutivas por la misma conexión y comprobar el
    cierre o persistencia según el comportamiento implementado.
3. Enviar cabeceras con nombres en mayúsculas, minúsculas y combinadas.
4. Probar `Content-Length` correcto, ausente, duplicado, no numérico y con
    overflow.
5. Probar `Transfer-Encoding: chunked` dividido entre varios `recv`, incluyendo
    chunks vacíos, extensiones, tamaño hexadecimal inválido y terminador ausente.
6. Probar un `Transfer-Encoding` distinto de `chunked` y verificar `501`.
7. Superar `client_max_body_size` mediante `Content-Length` y mediante chunked;
    ambos casos deben devolver `413` sin guardar datos parciales.
8. Enviar una petición incompleta, dejarla abierta y confirmar que el servidor
    sigue atendiendo a otros clientes.

### 4.3. Routing, métodos y archivos

1. `GET /` y `GET /index.html` deben devolver `200` y el contenido correcto.
2. Una ruta inexistente debe devolver `404` usando la página personalizada.
3. `POST` en `/` y `GET` en `/uploads` deben respetar los métodos configurados.
4. Probar `DELETE` sobre un archivo existente, inexistente, vacío y sin permisos.
5. Intentar traversal con `../`, codificación porcentual y variantes repetidas;
    nunca debe salirse de la raíz configurada.
6. Descargar archivos binarios y comparar bytes con `cmp` o un hash SHA-256.
7. Probar nombres con espacios, caracteres especiales, nombre vacío y colisiones
    en subidas multipart.
8. Verificar `autoindex` en `/uploads` y que no se habilita accidentalmente en `/`.

### 4.4. CGI bajo presión

1. Ejecutar `www/cgi-bin/echo.py` con GET, query string, POST vacío y POST de
    más de 1 MiB; comprobar método, argumentos, cuerpo y código HTTP.
2. Ejecutar un CGI inexistente, no ejecutable, con intérprete inválido y que
    termina con error; deben producir respuestas controladas, normalmente `404`
    o `500` según el caso.
3. Ejecutar un CGI que tarde más de 10 segundos y comprobar `504`, terminación
    del proceso y ausencia de zombies.
4. Hacer diez CGI simultáneos mientras se solicitan páginas estáticas; las
    respuestas estáticas no deben quedar bloqueadas.
5. Hacer que el CGI produzca más de `CGI_MAX_OUTPUT_SIZE` y comprobar que el
    proceso se termina y la respuesta no desborda memoria.
6. Verificar que las tuberías de entrada y salida se cierran en éxito, error,
    timeout y desconexión del cliente.

### 4.5. Concurrencia, I/O y fugas

1. Ejecutar 50 conexiones concurrentes a `/` y comprobar que todas reciben
    `200`.
2. Mantener un cliente lento enviando una petición byte a byte mientras otros
    clientes reciben respuestas normales.
3. Forzar escrituras parciales y `EAGAIN` con respuestas grandes; el servidor
    debe continuar desde el offset correcto sin duplicar bytes.
4. Interrumpir clientes durante lectura, escritura, upload y CGI; no deben
    quedar descriptores abiertos ni procesos hijos.
5. Comparar `/proc/<pid>/fd` antes y después de cientos de peticiones para
    detectar fugas de descriptores.
6. Repetir el smoke test varias veces y verificar que no quedan archivos de
    prueba ni procesos `webserv` o CGI.

### 4.6. Comandos base

```sh
make
./webserv config/webserv.conf

curl -i http://127.0.0.1:8081/
curl -i http://127.0.0.1:8081/index.html
curl -i http://127.0.0.1:8081/missing
curl -i "http://127.0.0.1:8081/cgi-bin/echo.py?name=test&value=123"
curl -i -X POST -d "name=test" http://127.0.0.1:8081/cgi-bin/echo.py
curl -i -X POST --data-binary @archivo.bin \
     http://127.0.0.1:8081/uploads/archivo.bin
curl -i -X DELETE http://127.0.0.1:8081/uploads/archivo.bin

make test
```

### 4.7. Guía de prueba manual CGI

Ejecutar los pasos desde WSL, en dos terminales, empezando por la raíz del
repositorio:

```sh
cd /mnt/c/Users/VanessaL/Documents/practice/websrv-webbita
```

#### Paso 1: compilar y arrancar

En la primera terminal:

```sh
make clean && make
./webserv config/webserv.conf
```

Debe aparecer `Listening on 127.0.0.1:8081`. Mantener esta terminal abierta.

#### Paso 2: comprobar HTTP y CGI

En la segunda terminal:

```sh
curl -i http://127.0.0.1:8081/
curl -i "http://127.0.0.1:8081/cgi-bin/echo.py?name=test"
curl -i -X POST -d "name=test" \
    http://127.0.0.1:8081/cgi-bin/echo.py
time curl -i http://127.0.0.1:8081/
```

Las respuestas deben terminar con un código HTTP válido; la petición estática
debe seguir respondiendo mientras se ejecuta el CGI.

#### Paso 3: preparar un CGI lento

Crear un script temporal para comprobar el timeout y los procesos activos:

```sh
printf '%s\n' \
    '#!/usr/bin/env python3' \
    'import time' \
    'time.sleep(30)' \
    'print("Content-Type: text/plain")' \
    'print()' \
    'print("late response")' > www/cgi-bin/hold.py
chmod +x www/cgi-bin/hold.py
```

Lanzarlo y observarlo desde la segunda terminal:

```sh
curl -i http://127.0.0.1:8081/cgi-bin/hold.py &
ps -ef | grep '[h]old.py'
```

Debe terminar aproximadamente a los 10 segundos con `504`. Después no debe
quedar ningún proceso `hold.py`:

```sh
ps -ef | grep '[h]old.py' || true
```

#### Paso 4: comprobar el límite de 16 CGI activos

Lanzar 16 CGI lentos en paralelo y esperar un segundo:

```sh
for i in $(seq 1 16); do
    curl -sS -o "/tmp/cgi-$i.out" \
        -w "cgi-$i %{http_code}\n" \
        http://127.0.0.1:8081/cgi-bin/hold.py &
done
sleep 1
```

Una nueva petición debe devolver `503`, porque el límite cuenta procesos CGI,
no pipes:

```sh
curl -sS -o /dev/null -w '%{http_code}\n' \
    http://127.0.0.1:8081/cgi-bin/hold.py
curl -sS -o /dev/null -w '%{http_code}\n' http://127.0.0.1:8081/
```

El primer código esperado es `503` y el segundo `200`. Esto confirma que el
límite CGI no bloquea las peticiones estáticas.

#### Paso 5: probar la progresión y el backpressure

Repetir la carga con `2`, `5`, `10`, `20`, `32` y `64` CGI. Registrar latencia,
códigos HTTP, timeouts y procesos activos:

```sh
for total in 2 5 10 20 32 64; do
    echo "=== $total CGI ==="
    start=$(date +%s%N)
    pids=""
    for i in $(seq 1 "$total"); do
        curl -sS -o "/tmp/cgi-$total-$i.out" \
            -w "%{http_code}\n" \
            http://127.0.0.1:8081/cgi-bin/hold.py &
        pids="$pids $!"
    done
    for pid in $pids; do wait "$pid"; done
    end=$(date +%s%N)
    echo "elapsed_ms=$(( (end - start) / 1000000 ))"
done
```

Por encima de 16 deben aparecer rechazos `503`. Para observar backpressure de
entrada y salida, enviar un body grande a varios CGI:

```sh
dd if=/dev/zero of=/tmp/cgi-body.bin bs=1M count=1
for i in $(seq 1 10); do
    curl -sS -o "/tmp/echo-$i.out" -X POST \
        --data-binary @/tmp/cgi-body.bin \
        http://127.0.0.1:8081/cgi-bin/echo.py &
done
wait
```

Si las peticiones estáticas siguen devolviendo `200` y los CGI avanzan, el
backpressure está controlado. Si se bloquean antes de 16 procesos, revisar la
arquitectura del bucle de eventos.

#### Paso 6: limpiar y comprobar zombies

Detener el servidor con `Ctrl+C` y eliminar el CGI temporal y sus salidas:

```sh
rm -f www/cgi-bin/hold.py /tmp/cgi-*.out /tmp/cgi-body.bin /tmp/echo-*.out
ps -ef | grep '[w]ebserv\|[h]old.py' || true
```

No debe quedar ningún proceso hijo CGI ni un `webserv` antiguo.

## 5. Resumen rápido

Si revisas estos puntos, tendremos una buena cobertura: arranque, virtual hosts, CGI, uploads, errores HTTP y solidez frente a casos límite.

## 6. Cosas por hacer

- Modelo actual de límite y rechazo:
    - `CGI_MAX_PROCESSES` vale `16`.
    - Con `0..15` CGI activos, una nueva petición puede crear el proceso.
    - Con `16` CGI activos, la nueva petición recibe inmediatamente `503
        Service Unavailable`.
    - Cuando `_tryFinalizeCgi()` llama a `_eraseCgiState(childPid)`, se libera
        el slot y una petición nueva puede iniciar otro CGI.
    - No existe una cola de peticiones pendientes: una petición rechazada no se
        guarda ni se reintenta automáticamente.

- La progresión `2 -> 5 -> 10 -> 20 -> 32 -> 64` es una prueba de carga, no
    un mecanismo de queueing:
    - Con `2`, `5` y `10`, todos los CGI deberían poder arrancar.
    - Con `20`, aproximadamente 16 arrancan y el resto recibe `503`.
    - Con `32` y `64`, se observa con más claridad el límite y el backpressure.
    - Cuando terminan procesos, entran peticiones nuevas; las que ya recibieron
        `503` no se recuperan.

- Resumen del comportamiento:

```text
máximo 16 CGI activos
resto -> 503 inmediato
sin queueing
```

- Añadir una prueba específica que abra más de 16 procesos CGI y compruebe la
    respuesta `503` del límite.
- Ejecutar una prueba de carga progresiva con `2 -> 5 -> 10 -> 20 -> 32 -> 64`
    CGI simultáneos. En cada escalón registrar latencia, throughput, errores,
    timeouts, procesos activos y pipes registrados para identificar el primer
    punto de degradación y distinguir saturación arquitectónica de backpressure.
- Ejecutar la prueba de fugas con Valgrind en un entorno Linux que lo tenga
    instalado.
- Revisar manualmente variantes adicionales de configuración y permisos.