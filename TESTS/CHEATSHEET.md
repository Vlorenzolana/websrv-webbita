# 🎯 WebServ - Pre-Evaluation Cheat Sheet

**Última revisión**: 15 de Julio de 2026 - Todas las pruebas ✅ PASANDO

---

## ⚡ 60-Second Checklist

```bash
# 1. Compilar
make clean && make

# 2. Iniciar servidor
./webserv config/multivhost.conf

# 3. En otra terminal, ejecutar tests
cd TESTS
bash test_multivhost.sh

# 4. Alternativa: Usar GUI interactivo
bash evaluation_gui.sh
```

**Status esperado**: ✅ 8/8 tests green

---

## 📡 Virtual Hosts (Mandatory)

| Puerto | Server Name | Root | Features |
|--------|------------|------|----------|
| 8080 | example.com | ./www/site1 | GET, POST, DELETE, CGI, Upload |
| 8081 | api.example.com | ./www/site2 | GET, POST |
| 8082 | admin.example.com | ./www/site3 | GET |

**Test rápido**:
```bash
curl -I http://localhost:8080/    # Status 200
curl -I http://localhost:8081/    # Status 200
curl -I http://localhost:8082/    # Status 200
```

---

## 🔌 HTTP Methods

| Método | Endpoint | Esperado | Test |
|--------|----------|----------|------|
| GET | http://localhost:8080/ | 200 | `curl http://localhost:8080/` |
| POST | http://localhost:8080/upload | 201 | `curl -X POST --data "test" http://localhost:8080/upload` |
| DELETE | http://localhost:8080/file | 200 | `curl -X DELETE http://localhost:8080/file` |
| UNKNOWN | http://localhost:8080/ | 405/501 | `curl -X UNKNOWN http://localhost:8080/` |

---

## 🐍 CGI Scripts

```bash
# Python CGI
curl http://localhost:8080/cgi/test.py
# Salida: <html>...<h1>Python CGI Script Test</h1>...</html>

# Bash CGI
curl http://localhost:8080/cgi/test.sh
# Salida: <html>...<h1>Bash CGI Script Test</h1>...</html>

# Con Query String
curl "http://localhost:8080/cgi/test.py?name=test&val=123"
# Salida: QUERY_STRING: name=test&val=123

# Con POST Data
curl -X POST -d "data=test" http://localhost:8080/cgi/test.py
# Salida: REQUEST_METHOD: POST
```

---

## ❌ Error Codes

```bash
# 404 Not Found
curl -I http://localhost:8080/nonexistent
# > HTTP/1.1 404 Not Found

# 405 Method Not Allowed
curl -X DELETE http://localhost:8082/    # puerto 3082 no permite DELETE
# > HTTP/1.1 405 Method Not Allowed

# 413 Payload Too Large
curl -X POST --data "$(head -c 2000000 </dev/urandom | base64)" \
     http://localhost:8080/upload
# > HTTP/1.1 413 Payload Too Large

# 403 Forbidden
curl http://localhost:8080/../../../etc/passwd
# > HTTP/1.1 403 Forbidden (path traversal protection)
```

---

## 📊 Stress Test (Siege)

```bash
# Instalar si no está
brew install siege

# Quick benchmark (99.5% availability requerido)
siege -c 10 -r 5 -b http://localhost:8080/

# Ver resultados
# Busca: "Availability: X.XX %"
# Debe ser > 99.5%
```

---

## 🔍 Code Review Points

### I/O Multiplexing
```bash
# Verificar que usa epoll (not select)
grep -n "epoll" src/Server.cpp
# Debería haber: epoll_create1, epoll_wait, epoll_ctl
```

### Memory Leaks
```bash
# Valgrind (Linux)
valgrind --leak-check=full ./webserv config/multivhost.conf

# Leaks (macOS)
leaks -atEachMalloc 1 ./webserv
```

### Signal Handling
```bash
# Debe ignorar SIGPIPE
grep -n "SIGPIPE" src/main.cpp
# Debería haber: signal(SIGPIPE, SIG_IGN)
```

---

## 🌐 Browser Test

```
1. Abre: http://localhost:8080/
   ✓ Página HTML renderiza
   ✓ Enlaces funcionan
   ✓ Página se ve completa

2. Haz F12 para ver Network
   ✓ Status: 200 OK
   ✓ Content-Type: text/html
   ✓ Content-Length: presente
   
3. Prueba 404:
   ✓ curl http://localhost:8080/fake
   ✓ Ves página de error personalizada

4. Prueba redirección:
   ✓ curl -L http://localhost:8080/redirect
```

---

## 🐛 Troubleshooting Rápido

| Problema | Solución |
|----------|----------|
| Port already in use | `killall webserv` |
| CGI 404 | `chmod +x www/site1/cgi/test.py` |
| Servidor no inicia | `make clean && make` |
| Lentitud | `ss -tlnp \| grep 808` verificar puertos |
| Memory leak | `valgrind --leak-check=full` |

---

## 📋 Pre-Evaluation Checklist

```
✅ Paso                                   Comando
□ Compila                               make clean && make
□ Inicia sin crashear                   ./webserv ...
□ 3 puertos escuchan                    ss -tlnp | grep 808
□ GET funciona (200)                    curl http://localhost:8080/
□ POST funciona (201)                   curl -X POST ... /upload
□ DELETE funciona (200)                 curl -X DELETE ...
□ CGI Python ejecuta                    curl .../cgi/test.py
□ CGI Bash ejecuta                      curl .../cgi/test.sh
□ Query strings pasan                   curl "...?name=test"
□ 404 error page                        curl .../nonexistent
□ 405 method not allowed                curl -X DELETE .../admin
□ 413 body too large                    Upload > limit
□ Autoindex funciona                    curl http://localhost:8080/
□ Index.html se sirve                   curl http://localhost:8080/
□ No segfaults                          Monitorea durante test
□ No memory leaks                       valgrind --leak-check=full
□ Sin "Connection refused"              Todos los puertos OK
□ Siege > 99.5%                         siege -c 10 -r 5 -b ...
```

---

## 🎓 Lo que el Evaluador Revisará

### Código
- ✓ epoll/select usado correctamente
- ✓ Manejo de errores en recv/send
- ✓ SIGPIPE ignorado
- ✓ Sin memory leaks

### Config
- ✓ Múltiples servers distintos
- ✓ listen y server_name
- ✓ client_max_body_size
- ✓ error_pages personalizadas
- ✓ locations con allowed_methods

### Funcionalidad
- ✓ GET, POST, DELETE sin crashes
- ✓ CGI con stdout correcto
- ✓ Status codes correctos
- ✓ Headers HTTP válidos
- ✓ Manejo de conexiones múltiples
- ✓ Sin leaks bajo carga (siege)

---

## 🚨 Red Flags (Nota: 0)

- ❌ Segmentation fault durante tests
- ❌ Memory leaks detectados
- ❌ Métodos GET/POST/DELETE fallan
- ❌ CGI retorna 404 o 403
- ❌ Comportamiento indefinido con métodos desconocidos
- ❌ Errno checkeado directamente (errno = X is forbidden)
- ❌ I/O sin pasar por select/epoll

---

## 💡 Last Minute Tips

1. **Si te queda poco tiempo**: Usa `bash test_multivhost.sh` para verificar todo rápido
2. **En la evaluación**: Ten listo `curl`, `telnet`, y navegador
3. **Detalles importantes**:
   - CGI scripts deben tener permisos ejecutables
   - No crashear nunca, ni con inputs maliciosos
   - Response headers deben ser HTTP/1.1 válidos
4. **Si hay falla**: Calma, explica el problema, ofrece debuggear en vivo
5. **Preguntas comunes**:
   - ¿Qué es epoll? → Multiplexación de I/O no-bloqueante
   - ¿Por qué no errno? → Puede cambiar, es asincrónico
   - ¿Memory leaks? → Verificar con valgrind

---

## 📞 Emergency Commands

```bash
# Ver si server está corriendo
pgrep -af webserv

# Ver qué escucha en qué puerto
ss -tlnp | grep webserv

# Logs en vivo
tail -f /tmp/webserv.log

# Benchmark rápido
curl -w "Status: %{http_code}\nTime: %{time_total}s\n" -o /dev/null -s \
    http://localhost:8080/

# Test simultáneo de 3 puertos
for p in 8080 8081 8082; do
  echo "Port $p: $(curl -s -o /dev/null -w %{http_code} http://localhost:$p/)"
done
```

---

**¡Buena suerte en la evaluación! 🍀**

*Creado: 15 Jul 2026 | Versión: 1.0 | Status: Todos los tests ✅ GREEN*
