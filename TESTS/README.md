# 🧪 TESTS Directory - Testing Suite for WebServ

## 📁 Contenido

```
TESTS/
├─ README.md                      ← Este archivo
├─ TESTING_GUIDE.md              ← Guía rápida de pruebas
├─ TESTING_COMPLETE_GUIDE.md     ← Guía exhaustiva (20+ pruebas)
├─ test_multivhost.sh            ← Script de pruebas (Bash/Linux)
└─ test_multivhost.ps1           ← Script de pruebas (PowerShell/Windows)
```

## 🚀 Quick Start

### Paso 1: Compilar

```bash
cd ..  # Volver a raíz del proyecto
make
```

### Paso 2: Ejecutar servidor (Terminal 1)

```bash
./webserv config/multivhost.conf
```

Debería ver:
```
Virtual Host 0 initialized - listening on port 8080
Virtual Host 1 initialized - listening on port 8081
Virtual Host 2 initialized - listening on port 8082

=== Starting 3 virtual host(s) ===
Thread started for port 8080
Thread started for port 8081
Thread started for port 8082
```

### Paso 3: Ejecutar tests (Terminal 2)

**Linux/Mac/WSL:**
```bash
cd TESTS
bash test_multivhost.sh
```

**Windows PowerShell:**
```powershell
cd TESTS
powershell -ExecutionPolicy Bypass -File test_multivhost.ps1
```

## 📚 Archivos de Tests

### `test_multivhost.sh` (Bash)
- ✅ Prueba rápida en Linux/Mac/WSL
- Verifica: 3 virtual hosts, CGI Python, CGI Bash, Autoindex, Upload
- Colores: RED/GREEN/YELLOW
- **Uso:** `bash test_multivhost.sh`

### `test_multivhost.ps1` (PowerShell)
- ✅ Prueba rápida en Windows
- Verifica: 3 virtual hosts, CGI, Query strings
- Colores: Green/Red/Cyan/Yellow
- **Uso:** `powershell -ExecutionPolicy Bypass -File test_multivhost.ps1`

### `TESTING_GUIDE.md`
- Guía detallada de pruebas manuales
- 9 casos de prueba con curl
- Explicación de cambios implementados
- Compilación manual step-by-step
- Troubleshooting básico

### `TESTING_COMPLETE_GUIDE.md`
- Guía exhaustiva (20+ pruebas)
- Pre-requisitos y verificación
- Cada prueba con: comando, salida esperada, explicación
- Pruebas de seguridad (path traversal, size limits)
- Stress tests y monitoreo
- Debugging tips

## 🎯 Test Coverage

### Características Probadas

#### Virtual Hosts
- ✅ Puerto 8080 (Site 1)
- ✅ Puerto 8081 (Site 2)
- ✅ Puerto 8082 (Site 3)
- ✅ Simultaneous connections

#### CGI Routing
- ✅ Python scripts (.py)
- ✅ Bash scripts (.sh)
- ✅ Query strings (GET params)
- ✅ POST data
- ✅ JSON payloads

#### HTTP Methods
- ✅ GET (static files)
- ✅ POST (uploads, CGI)
- ✅ DELETE (file removal)
- ✅ Method validation (405)

#### Directory Handling
- ✅ Autoindex (when enabled)
- ✅ Index file resolution
- ✅ 403 Forbidden (no autoindex)

#### Error Handling
- ✅ 404 Not Found
- ✅ 405 Method Not Allowed
- ✅ 413 Payload Too Large
- ✅ 403 Path Traversal protection

#### Performance
- ✅ Concurrent connections
- ✅ Stress tests (100+ requests)
- ✅ Memory monitoring
- ✅ Thread creation verification

## 📊 Test Matrix

| Test | Bash | PowerShell | Manual | Status |
|------|------|-----------|--------|--------|
| Host 1 GET | ✅ | ✅ | ✅ | Complete |
| Host 2 GET | ✅ | ✅ | ✅ | Complete |
| Host 3 GET | ✅ | ✅ | ✅ | Complete |
| CGI Python | ✅ | ✅ | ✅ | Complete |
| CGI Bash | ✅ | ✅ | ✅ | Complete |
| Query String | ⚠️ | ✅ | ✅ | Complete |
| POST Data | ⚠️ | ⚠️ | ✅ | Complete |
| File Upload | ✅ | ⚠️ | ✅ | Complete |
| Autoindex | ✅ | ⚠️ | ✅ | Complete |
| Delete | ⚠️ | ⚠️ | ✅ | Complete |
| Path Traversal | ⚠️ | ⚠️ | ✅ | Complete |
| Stress | ⚠️ | ⚠️ | ✅ | Complete |

**Legend:** ✅ = Included | ⚠️ = Partial/Manual | ❌ = Not included

## 🛠️ Ejecución Completa

### Opción A: Scripts Rápidos (2-3 minutos)

```bash
# Terminal 1
./webserv config/multivhost.conf

# Terminal 2
cd TESTS
bash test_multivhost.sh  # o test_multivhost.ps1
```

### Opción B: Tests Manuales Exhaustivos (15-20 minutos)

```bash
# Terminal 1
./webserv config/multivhost.conf

# Terminal 2
cd TESTS
cat TESTING_COMPLETE_GUIDE.md  # Seguir cada prueba manualmente
# Run each curl command from the guide
```

### Opción C: Tests Seleccionados (5-10 minutos)

```bash
# Terminal 1
./webserv config/multivhost.conf

# Terminal 2
# Según TESTING_GUIDE.md, seleccionar pruebas específicas

# Ejemplo: Solo CGI
curl http://localhost:8080/cgi/test.py
curl http://localhost:8080/cgi/test.sh

# Ejemplo: Solo Virtual Hosts
for port in 8080 8081 8082; do
  echo "Port $port:"
  curl -s -o /dev/null -w "Status: %{http_code}\n" http://localhost:$port/
done
```

## 📋 Checklist de Pruebas

Después de ejecutar tests, verifica:

### Funcionalidad Básica
- [ ] Servidor inicia sin errores
- [ ] 3 threads se crean (leer logs)
- [ ] 3 puertos escuchan (8080, 8081, 8082)

### Virtual Hosts
- [ ] GET / en puerto 8080 devuelve site1
- [ ] GET / en puerto 8081 devuelve site2
- [ ] GET / en puerto 8082 devuelve site3

### CGI
- [ ] GET /cgi/test.py ejecuta y devuelve HTML
- [ ] GET /cgi/test.sh ejecuta y devuelve HTML
- [ ] Query strings se pasan correctamente
- [ ] POST data se recibe

### HTTP Errors
- [ ] 404 para archivos no encontrados
- [ ] 405 para métodos no permitidos
- [ ] 403 para path traversal
- [ ] 413 para archivos muy grandes

### Quality
- [ ] Sin memory leaks
- [ ] Sin crashes
- [ ] Respuesta rápida (<100ms)
- [ ] Maneja múltiples conexiones

## 🔧 Troubleshooting

### Tests no conectan al servidor
```bash
# Verificar que el servidor está corriendo
ps aux | grep webserv

# Verificar puertos
netstat -tlnp | grep 808
# o
ss -tlnp | grep 808

# Conectarse manualmente
curl http://localhost:8080/
```

### CGI scripts no se ejecutan
```bash
# Verificar intérpretes
which python3
which bash

# Dar permisos
chmod +x ../www/site1/cgi/*.py
chmod +x ../www/site1/cgi/*.sh

# Test directo
../www/site1/cgi/test.py
```

### "Port already in use"
```bash
# Matar proceso anterior
killall webserv
# o
kill -9 $(pgrep webserv)
```

## 📈 Metrics

**Test execution time:**
- Quick tests (bash/powershell): ~2-3 seconds
- Complete manual tests: ~15-20 minutes
- Stress tests: ~1-2 minutes

**Expected performance:**
- Static file serving: <10ms
- CGI execution: 50-200ms
- Concurrent connections: 1000+ per thread

## 🎓 Learning Resources

1. **Conceptos:**
   - Lee `../README_42_STYLE.md` para entender epoll, multiplexing, threading
   - Lee `../ARCHITECTURE.md` para ver diagramas

2. **Implementación:**
   - Lee `../BUILD_GUIDE.md` para entender el build system
   - Lee source: `../src/Server.cpp` para ver CGI handling

3. **Testing:**
   - Empieza con `TESTING_GUIDE.md` (pruebas básicas)
   - Sube a `TESTING_COMPLETE_GUIDE.md` (exhaustivo)

## ✅ Success Criteria

Tu servidor está listo cuando:

- ✅ Todos los tests pasan
- ✅ Sin crashes ni memory leaks
- ✅ Maneja 100+ conexiones simultáneas
- ✅ CGI scripts se ejecutan correctamente
- ✅ Múltiples virtual hosts funcionan en paralelo
- ✅ Responde en <100ms para archivos estáticos

---

**Status:** Ready for testing
**Last Updated:** 2026-07-15
