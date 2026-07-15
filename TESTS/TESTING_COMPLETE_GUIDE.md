# 🧪 GUÍA COMPLETA DE PRUEBAS - CGI + Multiple Virtual Hosts

## ✅ Pre-requisitos antes de probar

1. **Compilar el proyecto:**
   ```bash
   # Linux/Mac/WSL
   bash build.sh
   
   # Windows (CMD)
   build.bat
   
   # O manualmente
   g++ -Wall -Wextra -Werror -std=c++98 -lpthread \
       -Iincludes \
       src/main.cpp src/Server.cpp src/ConfigParser.cpp \
       src/ConfigValidator.cpp src/Request.cpp src/CGIHandler.cpp \
       -o webserv
   ```

2. **Verificar que los intérpretes están instalados:**
   ```bash
   which python3     # Python
   which bash        # Bash
   which perl        # Perl (opcional)
   ```

3. **Dar permisos de ejecución a scripts (en Linux/Mac):**
   ```bash
   chmod +x www/site1/cgi/test.py
   chmod +x www/site1/cgi/test.sh
   ```

---

## 🚀 EJECUCIÓN

### Terminal 1: Iniciar servidor

```bash
./webserv config/multivhost.conf
```

**Salida esperada:**
```
Virtual Host 0 initialized - listening on port 8080 (Server name: example.com)
Virtual Host 1 initialized - listening on port 8081 (Server name: api.example.com)
Virtual Host 2 initialized - listening on port 8082 (Server name: admin.example.com)

=== Starting 3 virtual host(s) ===
Thread started for port 8080 (Thread ID: 140127485295360)
Thread started for port 8081 (Thread ID: 140127485295361)
Thread started for port 8082 (Thread ID: 140127485295362)
```

---

## 📋 SUITE DE PRUEBAS

### **PRUEBA 1: Verificar que todos los hosts escuchan**

**Comando:**
```bash
for port in 8080 8081 8082; do
  echo "Testing port $port:"
  curl -s -o /dev/null -w "Status: %{http_code}\n" http://localhost:$port/
done
```

**Salida esperada:**
```
Testing port 8080:
Status: 200
Testing port 8081:
Status: 200
Testing port 8082:
Status: 200
```

---

### **PRUEBA 2: GET archivo HTML (Virtual Host 1)**

**Comando:**
```bash
curl -v http://localhost:8080/
```

**Salida esperada:**
```
> GET / HTTP/1.1
> Host: localhost:8080
>
< HTTP/1.1 200 OK
< Content-Type: text/html
< Content-Length: 456
<
<!DOCTYPE html>
<html>
<head>
    <title>Virtual Host 1 - Main Site</title>
</head>
<body>
    <h1>Welcome to Virtual Host 1 (Port 8080)</h1>
    ...
```

---

### **PRUEBA 3: GET archivo HTML (Virtual Host 2)**

**Comando:**
```bash
curl http://localhost:8081/
```

**Salida esperada:**
```html
<!DOCTYPE html>
<html>
<head>
    <title>Virtual Host 2 - API Site</title>
</head>
...
Welcome to Virtual Host 2 (Port 8081)
...
```

---

### **PRUEBA 4: GET archivo HTML (Virtual Host 3)**

**Comando:**
```bash
curl http://localhost:8082/
```

**Salida esperada:**
```html
<!DOCTYPE html>
<html>
<head>
    <title>Virtual Host 3 - Admin Site</title>
</head>
...
Welcome to Virtual Host 3 (Port 8082)
...
```

---

### **PRUEBA 5: Ejecutar CGI - Python Script (GET)**

**Comando:**
```bash
curl -v http://localhost:8080/cgi/test.py
```

**Salida esperada:**
```
> GET /cgi/test.py HTTP/1.1
>
< HTTP/1.1 200 OK
< Content-Type: text/plain
<
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

---

### **PRUEBA 6: Ejecutar CGI - Bash Script (GET)**

**Comando:**
```bash
curl http://localhost:8080/cgi/test.sh
```

**Salida esperada:**
```html
<html><body>
<h1>Bash CGI Script Test</h1>
<p>This Bash script is running on the web server!</p>
<h2>Environment Information:</h2>
<ul>
<li>REQUEST_METHOD: GET</li>
<li>PATH_INFO: /cgi/test.sh</li>
<li>QUERY_STRING: </li>
<li>Hostname: mycomputer</li>
<li>User: myuser</li>
</ul>
<p>Script executed successfully!</p>
</body></html>
```

---

### **PRUEBA 7: CGI con Query String**

**Comando:**
```bash
curl "http://localhost:8080/cgi/test.py?name=Juan&age=25&city=Madrid"
```

**Salida esperada:**
```html
...
<li>QUERY_STRING: name=Juan&age=25&city=Madrid</li>
...
```

**Explicación:** Los scripts pueden acceder a `QUERY_STRING` para procesar parámetros GET.

---

### **PRUEBA 8: POST a CGI Script**

**Comando:**
```bash
curl -X POST \
  -H "Content-Type: application/x-www-form-urlencoded" \
  -d "name=test&value=123&action=create" \
  http://localhost:8080/cgi/test.py
```

**Salida esperada:**
```html
...
<li>REQUEST_METHOD: POST</li>
<li>CONTENT_LENGTH: 32</li>
<li>CONTENT_TYPE: application/x-www-form-urlencoded</li>
...
```

**Explicación:** El script recibe datos en STDIN y puede acceder a `CONTENT_LENGTH` y `CONTENT_TYPE`.

---

### **PRUEBA 9: POST JSON a CGI**

**Comando:**
```bash
curl -X POST \
  -H "Content-Type: application/json" \
  -d '{"user":"john","email":"john@example.com"}' \
  http://localhost:8080/cgi/test.py
```

**Salida esperada:**
```html
...
<li>CONTENT_TYPE: application/json</li>
...
```

---

### **PRUEBA 10: Subir archivo (POST binario)**

**Comando:**
```bash
# Crear un archivo de prueba
echo "Este es un archivo de prueba" > /tmp/test.txt

# Subirlo
curl -X POST --data-binary @/tmp/test.txt http://localhost:8080/upload/myfile.txt
```

**Salida esperada:**
```
HTTP/1.1 201 Created
Content-Type: text/plain
Content-Length: 50

File uploaded successfully: myfile.txt
```

**Verificar:**
```bash
ls -la www/uploads1/
cat www/uploads1/myfile.txt
```

---

### **PRUEBA 11: Autoindex (Listado de directorio)**

**Comando:**
```bash
curl http://localhost:8080/
```

Si la location tiene `autoindex on`, debería mostrar:
```html
<html><head><title>Index of /</title></head><body>
<h1>Index of /</h1><ul>
<li><a href="/cgi/">cgi</a></li>
<li><a href="/index.html">index.html</a></li>
<li><a href="/uploads/">uploads</a></li>
</ul></body></html>
```

---

### **PRUEBA 12: Autoindex deshabilitado**

**Comando:**
```bash
# Virtual Host 3 tiene autoindex off
curl http://localhost:8082/
```

**Salida esperada:**
```
HTTP/1.1 403 Forbidden
Content-Type: text/plain
Content-Length: 18

403 Forbidden
```

---

### **PRUEBA 13: Archivo no encontrado (404)**

**Comando:**
```bash
curl -v http://localhost:8080/noexiste.html
```

**Salida esperada:**
```
< HTTP/1.1 404 Not Found
< Content-Type: text/plain
< Content-Length: 15
<
404 Not Found
```

---

### **PRUEBA 14: Método no permitido (405)**

**Comando:**
```bash
# Si DELETE no está permitido en esta location
curl -X DELETE http://localhost:8080/index.html
```

**Salida esperada:**
```
< HTTP/1.1 405 Method Not Allowed
```

---

### **PRUEBA 15: DELETE archivo permitido**

**Primero, crear un archivo:**
```bash
curl -X POST --data "contenido" http://localhost:8080/upload/test_delete.txt
```

**Luego, borrarlo:**
```bash
curl -X DELETE http://localhost:8080/upload/test_delete.txt
```

**Salida esperada:**
```
< HTTP/1.1 200 OK
<
Resource deleted successfully
```

**Verificar que se borró:**
```bash
ls www/uploads1/test_delete.txt  # Debe no existir
```

---

### **PRUEBA 16: Payload demasiado grande (413)**

**Comando:**
```bash
# Crear un archivo de 2MB (límite es 1MB en site1)
dd if=/dev/zero of=/tmp/large.bin bs=1M count=2

curl -X POST --data-binary @/tmp/large.bin http://localhost:8080/upload/
```

**Salida esperada:**
```
< HTTP/1.1 413 Payload Too Large
```

---

### **PRUEBA 17: Path Traversal Protection**

**Comando:**
```bash
curl http://localhost:8080/../../../etc/passwd
```

**Salida esperada:**
```
< HTTP/1.1 403 Forbidden
```

---

### **PRUEBA 18: Simultaneous connections a múltiples hosts**

**Comando (usa todos los 3 hosts a la vez):**
```bash
# Terminal 1
curl http://localhost:8080/ &
# Terminal 2
curl http://localhost:8081/ &
# Terminal 3
curl http://localhost:8082/ &

# O todos juntos:
time for i in {1..10}; do
  curl -s http://localhost:8080/ > /dev/null &
  curl -s http://localhost:8081/ > /dev/null &
  curl -s http://localhost:8082/ > /dev/null &
done
wait
```

**Salida esperada:**
```
real    0m0.XXXs
(Todas las peticiones se sirven simultáneamente)
```

---

### **PRUEBA 19: MIME Types**

**Comando:**
```bash
# HTML
curl -I http://localhost:8080/index.html | grep Content-Type

# Python (ejecuta CGI)
curl -I http://localhost:8080/cgi/test.py | grep Content-Type

# Bash (ejecuta CGI)
curl -I http://localhost:8080/cgi/test.sh | grep Content-Type
```

**Salida esperada:**
```
Content-Type: text/html
Content-Type: text/plain
Content-Type: text/plain
```

---

### **PRUEBA 20: Stress Test**

**Comando:**
```bash
# Apache Bench (si está instalado)
ab -n 1000 -c 10 http://localhost:8080/index.html

# O con curl y time
time for i in {1..100}; do
  curl -s http://localhost:8080/index.html > /dev/null
  curl -s http://localhost:8081/index.html > /dev/null
  curl -s http://localhost:8082/index.html > /dev/null
done
```

**Salida esperada:**
```
Time per request:   X ms
Requests per second: Y
```

---

## 🔍 Monitoreo en Tiempo Real

### En Linux/Mac:

```bash
# Ver threads del proceso
ps -eL | grep webserv

# Ver puertos abiertos
netstat -tlnp | grep webserv

# Ver conexiones activas
lsof -p <PID> | grep TCP

# Recursos del proceso
top -p <PID>

# Estadísticas de red
watch -n 1 'ss -s'
```

### En Windows:

```powershell
# Ver procesos
tasklist | Select-String webserv

# Ver puertos abiertos
netstat -ano | Select-String LISTENING

# Performance
Get-Process webserv | Select Handles, WorkingSet, CPU

# Network stats
Get-NetTCPConnection | Where {$_.OwningProcess -eq (Get-Process webserv).Id}
```

---

## 📝 Checklist Final

Después de ejecutar todas las pruebas, verifica:

- [ ] Los 3 virtual hosts escuchan simultáneamente
- [ ] GET devuelve archivos HTML correctamente
- [ ] CGI scripts Python se ejecutan
- [ ] CGI scripts Bash se ejecutan
- [ ] Query strings se pasan a scripts CGI
- [ ] POST data se recibe en scripts CGI
- [ ] Archivos se suben correctamente
- [ ] Autoindex funciona donde está activado
- [ ] Autoindex retorna 403 donde está desactivado
- [ ] 404 para archivos no encontrados
- [ ] 405 para métodos no permitidos
- [ ] DELETE borra archivos
- [ ] 413 para payloads demasiado grandes
- [ ] 403 para path traversal
- [ ] MIME types correctos
- [ ] Sin memory leaks (valgrind)
- [ ] Múltiples conexiones simultáneas funcionan

---

## 🐛 Debugging

### Ver logs de requests:

El servidor ya imprime logs en stdout. Para capturarlos:

```bash
./webserv config/multivhost.conf 2>&1 | tee server.log
```

### Aumentar verbosidad con curl:

```bash
curl -v http://localhost:8080/
```

### Ver exactamente qué se envía/recibe:

```bash
curl -v --trace-ascii /tmp/curl.trace http://localhost:8080/
cat /tmp/curl.trace
```

### Revisar archivos del servidor:

```bash
ls -la www/site1/
ls -la www/uploads1/
```

---

## 🎉 ¡Listo!

Una vez que todas las pruebas pasen, tu servidor webserv soporta:
- ✅ CGI Routing (ejecuta scripts automáticamente)
- ✅ Multiple Virtual Hosts (3+ servidores en paralelo)
- ✅ Threads POSIX (escalabilidad)
- ✅ epoll (manejo eficiente de conexiones)
- ✅ HTTP completo (GET, POST, DELETE)
- ✅ Seguridad básica (path traversal, size limits)
