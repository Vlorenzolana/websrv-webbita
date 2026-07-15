# 🧪 Evaluation GUI - WebServ 42 School

Un **GUI interactivo en Bash** para repasar todos los puntos del rubric de evaluación antes de presentar el proyecto.

## 🚀 Quick Start

```bash
cd TESTS
bash evaluation_gui.sh
```

## 📋 Qué Cubre

### Mandatory Part
- ✅ **Configuration Tests** - Puertos, hostnames, error pages, body limits
- ✅ **HTTP Methods** - GET, POST, DELETE, métodos desconocidos
- ✅ **CGI Tests** - Python, Bash, query strings, POST data
- ✅ **Browser Compatibility** - Headers, rendering, links
- ✅ **Port Issues** - Múltiples puertos, virtual hosts
- ✅ **Siege Stress Tests** - Disponibilidad, memory leaks

### Code Review
- ✅ I/O Multiplexing (epoll verification)
- ✅ Error Handling
- ✅ Memory Leaks (valgrind guide)

## 📊 Menú Principal

```
1. Configuration Tests
2. Basic HTTP Tests (GET, POST, DELETE)
3. CGI Tests (Python y Bash)
4. Browser Compatibility (Headers, Status Codes)
5. Port Issues & Multiple Servers
6. Siege Stress Test
7-9. Code Review
10. Start Server
11. Stop Server
12. View Full Report
0. Exit
```

## ⚡ Workflow Recomendado

1. **Inicia el servidor** (Opción 10)
2. **Ejecuta Configuration Tests** (Opción 1)
3. **Ejecuta HTTP Methods tests** (Opción 2)
4. **Ejecuta CGI tests** (Opción 3)
5. **Abre navegador** (Opción 4)
6. **Verifica puertos** (Opción 5)
7. **Stress test** (Opción 6)
8. **Code review** (Opción 7)
9. **Ver reporte completo** (Opción 12)

## 🎯 Checklist Pre-Evaluación

### Antes de la evaluación:

- [ ] Servidor compila sin errores
- [ ] 3 virtual hosts funcionan (8080, 8081, 8082)
- [ ] GET, POST, DELETE funcionan
- [ ] CGI Python y Bash se ejecutan
- [ ] Manejo de errores 404, 405, 413
- [ ] Sin crashes con métodos desconocidos
- [ ] Body size limit funciona
- [ ] Memory leaks verificados con valgrind
- [ ] Siege test > 99.5% availability
- [ ] Headers HTTP correctos
- [ ] Autoindex funciona
- [ ] Error pages personalizadas sirven

## 🔧 Troubleshooting

### "Servidor no está ejecutándose"
```bash
# Usa opción 10 del menú para iniciar
# O manualmente:
./webserv config/multivhost.conf
```

### CGI scripts no se ejecutan
```bash
chmod +x www/site1/cgi/test.py
chmod +x www/site1/cgi/test.sh
```

### Siege no está instalado
```bash
# macOS
brew install siege

# Ubuntu/Debian
sudo apt-get install siege

# CentOS/RHEL
sudo yum install siege
```

### Puertos en uso
```bash
killall webserv
# O indicar en el menú que detenga (Opción 11)
```

## 📈 Interpretación del Reporte

- **90-100%**: Excellent - Listo para evaluación
- **70-89%**: Good - Revisa los puntos fallidos
- **<70%**: Needs Work - Hay issues que arreglar

## 💡 Tips para la Evaluación

1. **Mantén el servidor ejecutándose** - No reinicies a menos que sea necesario
2. **Ten a mano curl y navegador** - Para verificar manualmente
3. **Monitorea memoria** - `watch -n 0.5 'ps aux | grep webserv'`
4. **Revisa logs** - Si algo falla, usa `tail -f /tmp/webserv.log`
5. **Compila antes de evaluar** - `make clean && make`

## 📝 Documentación Completa

Ver [TESTING_COMPLETE_GUIDE.md](TESTING_COMPLETE_GUIDE.md) para detalles de cada test.

---

**Última actualización**: 15 de Julio de 2026  
**Versión**: 1.0  
**Compatibilidad**: Linux, macOS (con bash ≥ 4.0)
