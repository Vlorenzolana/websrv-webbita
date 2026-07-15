# CGI Routing + Multiple Virtual Hosts - Testing Guide

## 📋 Cambios Implementados

### 1. **CGI Routing**
El servidor ahora detecta y ejecuta scripts CGI en lugar de servirlos como archivos estáticos.

**Extensiones soportadas:**
- `.py` → ejecuta con Python 3
- `.sh` → ejecuta con Bash
- `.pl` → ejecuta con Perl
- `.cgi` → ejecuta con Bash

**Flujo:**
```
GET /script.py
    ↓
Server detecta extensión .py
    ↓
Ejecuta: /usr/bin/python3 /ruta/script.py
    ↓
Devuelve stdout del script como respuesta HTTP
```

### 2. **Multiple Virtual Hosts con Threads**
El servidor ahora puede escuchar en múltiples puertos simultáneamente, cada uno con su propia configuración.

**Cambios:**
- `main.cpp`: Crea 1 thread por cada `server {}` block
- Cada thread ejecuta su propio epoll loop
- Todos escuchan simultáneamente

## 🛠️ Compilación

### En Linux / WSL / Mac:

```bash
cd websrv-webbita-feature-merge-mvp
make clean
make
```

**O manualmente con g++:**
```bash
g++ -Wall -Wextra -Werror -std=c++98 -lpthread \
    -Iincludes \
    src/main.cpp src/Server.cpp src/ConfigParser.cpp \
    src/ConfigValidator.cpp src/Request.cpp src/CGIHandler.cpp \
    -o webserv
```

### En Windows con compilador C++ (MinGW / Visual Studio):

```bash
clang++ -Wall -Wextra -Werror -std=c++98 -lpthread ^
    -Iincludes ^
    src/main.cpp src/Server.cpp src/ConfigParser.cpp ^
    src/ConfigValidator.cpp src/Request.cpp src/CGIHandler.cpp ^
    -o webserv.exe
```

## 🚀 Ejecución

### Ejecutar con configuración de múltiples hosts:
```bash
./webserv config/multivhost.conf
```

### Salida esperada:
```
Virtual Host 0 initialized - listening on port 8080 (Server name: example.com)
Virtual Host 1 initialized - listening on port 8081 (Server name: api.example.com)
Virtual Host 2 initialized - listening on port 8082 (Server name: admin.example.com)

=== Starting 3 virtual host(s) ===
Thread started for port 8080 (Thread ID: 139876543210496)
Thread started for port 8081 (Thread ID: 139876543210497)
Thread started for port 8082 (Thread ID: 139876543210498)
```

## 🧪 Pruebas

### Prueba 1: Verificar que múltiples hosts escuchan

**En Linux/WSL (en otra terminal):**
```bash
# Terminal 2
bash test_multivhost.sh
```

**En Windows PowerShell (en otra terminal):**
```powershell
powershell -ExecutionPolicy Bypass -File test_multivhost.ps1
```

### Prueba 2: Verificar CGI - Python

```bash
curl http://localhost:8080/cgi/test.py
```

**Salida esperada:**
```html
<html><body>
<h1>Python CGI Script Test</h1>
<p>This script is running on the web server!</p>
<h2>Environment Variables:</h2>
<ul>
<li>REQUEST_METHOD: GET</li>
<li>PATH_INFO: /cgi/test.py</li>
<li>QUERY_STRING: </li>
<li>SERVER_PROTOCOL: HTTP/1.1</li>
</ul>
<p>Script executed successfully!</p>
</body></html>
```

### Prueba 3: Verificar CGI - Bash

```bash
curl http://localhost:8080/cgi/test.sh
```

### Prueba 4: Verificar Virtual Host 1 (Main)

```bash
curl http://localhost:8080/
```

**Salida esperada:** HTML con título "Welcome to Virtual Host 1 (Port 8080)"

### Prueba 5: Verificar Virtual Host 2 (API)

```bash
curl http://localhost:8081/
```

**Salida esperada:** HTML con título "Welcome to Virtual Host 2 (Port 8081)"

### Prueba 6: Verificar Virtual Host 3 (Admin)

```bash
curl http://localhost:8082/
```

**Salida esperada:** HTML con título "Welcome to Virtual Host 3 (Port 8082)"

### Prueba 7: POST con datos a CGI

```bash
curl -X POST -d "name=test&value=123" http://localhost:8080/cgi/test.py
```

### Prueba 8: Autoindex (lista de directorio)

```bash
curl http://localhost:8080/
```

Debería mostrar un directorio listado si está habilitado.

### Prueba 9: Upload de archivo

```bash
curl -X POST --data-binary @archivo.txt http://localhost:8080/upload/
```

## 📊 Estructura de Configuración

**config/multivhost.conf:**
```nginx
server {
    server_name example.com;
    port 8080;
    root ./www/site1;
    
    location / {
        allowed_methods GET POST DELETE;
        index index.html;
    }
    
    location /cgi {
        allowed_methods GET POST;
        # Los scripts .py, .sh aquí se ejecutan
    }
}
```

## 🔍 Verificar Threads

### En Linux/WSL:
```bash
# En otra terminal, mientras webserv está ejecutándose
ps aux | grep webserv  # Ver procesos
lsof -p <PID>          # Ver sockets abiertos (puertos)
```

**Salida esperada:**
```
webserv     3000  0.0  0.1   5432   2048 ?  Sl   14:30   0:00 ./webserv
```

### Ver puertos escuchando:
```bash
netstat -tlnp | grep webserv
# O en algunas distros:
ss -tlnp | grep webserv
```

**Salida esperada:**
```
LISTEN     0      128          0.0.0.0:8080       0.0.0.0:*       users:(("webserv",pid=3000,fd=3))
LISTEN     0      128          0.0.0.0:8081       0.0.0.0:*       users:(("webserv",pid=3000,fd=5))
LISTEN     0      128          0.0.0.0:8082       0.0.0.0:*       users:(("webserv",pid=3000,fd=7))
```

## 📂 Archivos de Prueba Incluidos

- `www/site1/index.html` - Página principal Virtual Host 1
- `www/site1/cgi/test.py` - Script CGI Python
- `www/site1/cgi/test.sh` - Script CGI Bash
- `www/site2/index.html` - Página principal Virtual Host 2
- `www/site3/index.html` - Página principal Virtual Host 3
- `config/multivhost.conf` - Configuración con 3 hosts
- `test_multivhost.sh` - Script de pruebas Bash
- `test_multivhost.ps1` - Script de pruebas PowerShell

## 🐛 Solución de problemas

### "Port already in use"
```bash
# Encontrar qué proceso usa el puerto
lsof -i :8080
# Matarlo si es necesario
kill -9 <PID>
```

### CGI scripts no se ejecutan
- Verificar que `.py`, `.sh`, `.pl` están en una `location` que permite GET/POST
- Verificar que el intérprete existe: `which python3`, `which bash`
- Revisar permisos del archivo: `chmod +x script.py`

### Thread issues en Windows
- Windows maneja threads diferente; algunos sistemas requieren `-lws2_32` en lugar de `-lpthread`
- Considera usar WSL para desarrollo

## 📝 Notas Importantes

1. **CGI Output Format**: Los scripts CGI deben incluir headers HTTP:
   ```
   Content-Type: text/html
   
   <html>...</html>
   ```

2. **Timeout**: Los scripts CGI no tienen timeout actualmente
   - Un script infinito bloqueará ese cliente

3. **Security**: Los scripts CGI pueden ser vulnerables
   - No ejecutar scripts no confiables
   - Validar inputs cuidadosamente

4. **Performance**: Con muchos threads:
   - El primer servidor siempre se ejecuta (es el main thread)
   - Para parallelismo verdadero, considera usar `select()` o `poll()` en un único thread
