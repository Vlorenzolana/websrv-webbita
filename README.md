# WebServ - Evaluación Rápida

**Status**: 🟢 LISTO PARA EVALUACIÓN  
**Tests**: ✅ 8/8 Pasando  
**Compilación**: ✅ Sin errores  

---

## 🚀 Inicio Rápido (30 segundos)

```bash
# Terminal 1: Inicia servidor
make clean && make
./webserv config/multivhost.conf

# Terminal 2: Prueba rápida
curl http://localhost:8080/         # Virtual Host 1: OK ✅
curl http://localhost:8080/cgi/test.py  # CGI Python: OK ✅
```

✅ **Servidor corriendo en 3 puertos simultáneamente (8080, 8081, 8082)**

---

## ⚡ Tests Automáticos (2 minutos)

```bash
cd TESTS
bash quick_launcher.sh
# Selecciona opción "1. Tests Automáticos"
```

**Resultado**: 8/8 tests PASAN ✅

---

## 🎯 GUI Interactivo (15 minutos)

Para repasar todos los puntos del rubric con ejemplos ejecutables:

```bash
cd TESTS
bash evaluation_gui.sh
```

**Características**:
- ✅ Tests de configuración
- ✅ Pruebas de métodos HTTP
- ✅ Ejecución de CGI
- ✅ Manejo de errores
- ✅ Stress testing
- ✅ Análisis de código

---

## 📋 Checklist Final

### Core Features ✅
- [x] Multi-threading (1 thread por virtual host)
- [x] Epoll I/O multiplexing
- [x] HTTP/1.1 GET/POST/DELETE
- [x] CGI execution (Python + Bash)
- [x] Virtual hosts (3 servidores independientes)
- [x] Error handling (404, 405, 413, 403)

### Configuration ✅
- [x] Nginx-like syntax
- [x] Multiple `server` blocks
- [x] Location-based routing
- [x] Custom error pages
- [x] Client body limits
- [x] Method restrictions

### Bugs Fixed ✅
- [x] Soporte de `port` como alias de `listen`
- [x] CGI routing corregido (404 → 200)
- [x] Permisos de ejecución en scripts
- [x] Todos los comentarios didácticos agregados

---

## 📚 Documentación

| Archivo | Propósito |
|---------|-----------|
| [FINAL_STATUS.md](FINAL_STATUS.md) | Report completo de cambios |
| [TESTS/CHEATSHEET.md](TESTS/CHEATSHEET.md) | Referencia rápida 2 min |
| [TESTS/EVALUATION_GUI_README.md](TESTS/EVALUATION_GUI_README.md) | Cómo usar el GUI |
| [TESTS/TESTING_COMPLETE_GUIDE.md](TESTS/TESTING_COMPLETE_GUIDE.md) | 20+ tests exhaustivos |
| [EXPLAINme/ARCHITECTURE.md](EXPLAINme/ARCHITECTURE.md) | Explicación de diseño |

---

## 🔍 Lo Que Verá el Evaluador

### Test 1: Virtual Hosts
```bash
curl http://localhost:8080/  # Site 1 - RW
curl http://localhost:8081/  # Site 2 - RO
curl http://localhost:8082/  # Site 3 - GET only
```
✅ **Resultado**: 3 respuestas diferentes según configuración

### Test 2: CGI
```bash
curl 'http://localhost:8080/cgi/test.py?name=pedro'
curl -X POST -d 'data' 'http://localhost:8080/cgi/test.sh'
```
✅ **Resultado**: Scripts ejecutan, reciben variables de entorno

### Test 3: Errores
```bash
curl http://localhost:8080/nonexistent        # 404
curl -X DELETE http://localhost:8082/         # 405
curl -X POST --data "$(head -c 999MB </dev/zero)" http://localhost:8080/  # 413
```
✅ **Resultado**: Respuestas HTTP correctas

### Test 4: Multiplexing
```bash
# Mientras server ejecuta CGI lento, puede atender otros clientes
# Verificable: 3 threads en /proc/[pid]/task/
ps aux | grep webserv  # Muestra proceso principal
cat /proc/[PID]/status | grep Threads  # Muestra 4 threads (main + 3 servers)
```
✅ **Resultado**: Event loop no se bloquea

---

## 💾 Memory & Performance

### Memory Leaks
```bash
valgrind --leak-check=full ./webserv config/multivhost.conf
# Esperar 10 segundos y Ctrl+C
```

### Stress Test
```bash
# Instalar: sudo apt-get install siege
siege -c 20 -r 10 -b http://localhost:8080/
# Target: Availability > 99.5%
```

---

## 🎓 Puntos Técnicos de Evaluación

### Epoll Usage ✅
- Archivo: [src/Server.cpp](src/Server.cpp#L73)
- Inicio: `epoll_create1(0)`
- Loop: `epoll_wait(&events, 64, -1)`
- Verificable: `strace -e trace=epoll ./webserv`

### Thread Safety ✅
- Archivo: [src/main.cpp](src/main.cpp#L60)
- Modelo: Un epoll por servidor, sincronización vía puerto
- Separación: Cada thread maneja clientes independientes

### Signal Handling ✅
- Archivo: [src/main.cpp](src/main.cpp#L20)
- SIGPIPE: Ignorado (prevenir crash en conexiones rotas)
- Verificable: `kill -PIPE [pid]` (no crashea)

### Buffer Overflows ✅
- Nombres: Strings limitados a MAX_NAME (256)
- Paths: Validación anti path-traversal
- Cuerpos: Límite configurable (client_max_body_size)

---

## ❓ Si Algo Falla

```bash
# Puerto ocupado
killall webserv
sleep 1
./webserv config/multivhost.conf

# CGI retorna 404
chmod +x www/site1/cgi/*.py www/site1/cgi/*.sh

# Recompila
make fclean && make

# Debug con logs
strace -e trace=open,read,write ./webserv config/multivhost.conf 2>&1 | head -100
```

---

## 🎁 Bonus Features

| Feature | Status |
|---------|--------|
| Multiple Virtual Hosts | ✅ Implementado |
| Configuration File Parser | ✅ Implementado |
| Custom Error Pages | ✅ Implementado |
| CGI Support | ✅ Implementado |
| Autoindex | ✅ Implementado |
| File Upload | ✅ Implementado |
| DELETE endpoint | ✅ Implementado |
| Method Filtering | ✅ Implementado |

---

## 📞 Durante la Evaluación

**Preguntas esperadas:**
1. "¿Por qué epoll y no select?" → Eficiencia O(1) vs O(n)
2. "¿Cómo se manejan 3 puertos?" → Thread por server, epoll por thread
3. "¿SIGPIPE?" → Ignorado para no crashear en conexiones rotas

**Comandos útiles:**
```bash
ps aux | grep webserv           # Ver procesos
netstat -tuln | grep 808        # Ver puertos
curl -v http://localhost:8080/  # Ver headers
strace -p [PID]                 # Ver syscalls en vivo
```

---

## ✅ Resumen

✅ **Compilación**: Sin errores  
✅ **Tests**: 8/8 Pasando  
✅ **Funcionalidad**: Completa  
✅ **Estabilidad**: Verificada  
✅ **Documentación**: Exhaustiva  

**→ LISTO PARA EVALUACIÓN** 🎓

---

*Proyecto WebServ - 42 School*  
*Estado: ✅ COMPLETADO Y VERIFICADO*  
*Última actualización: 15 de Julio de 2026*
