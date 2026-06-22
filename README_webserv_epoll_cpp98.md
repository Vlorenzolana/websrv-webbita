# Webserv EPOLL C++98 Skeleton

## Objetivo

Proyecto mínimo inspirado en Webserv de 42.

Implementa:

- socket()
- setsockopt()
- bind()
- listen()
- epoll_create1()
- epoll_ctl()
- epoll_wait()
- aceptación de clientes
- estructura básica orientada a objetos

No implementa todavía:

- HTTP parsing
- GET / POST / DELETE
- CGI
- Configuración nginx-like
- Virtual hosts
- Keep-Alive

---

## Estructura

```text
webserv_epoll_cpp98/
├── Makefile
├── include/
│   └── Server.hpp
└── src/
    ├── main.cpp
    └── Server.cpp
```

---

## Compilación

```bash
make
```

Generará:

```bash
./webserv
```

---

## Ejecución

```bash
./webserv www/minimal_webserv.conf 
```

Por defecto escucha en:

```text
localhost:3000
```

---

## Flujo del servidor

```text
socket()
    ↓
setsockopt()
    ↓
bind()
    ↓
listen()
    ↓
epoll_create1()
    ↓
epoll_ctl()
    ↓
epoll_wait()
    ↓
accept()
```

---

## Conceptos importantes para la defensa de Webserv

### Socket

Punto de entrada de conexiones TCP.

```cpp
socket(AF_INET, SOCK_STREAM, 0);
```

### Bind

Asocia el socket a una IP y puerto.

```cpp
bind(...);
```

### Listen

Pone el socket en modo servidor.

```cpp
listen(...);
```

### Epoll

Permite monitorizar miles de conexiones sin recorrer todos los fds.

```cpp
epoll_create1(0);
epoll_ctl(...);
epoll_wait(...);
```

### Non Blocking

Evita que una operación de red bloquee el servidor completo.

```cpp
fcntl(fd, F_SETFL, O_NONBLOCK);
```

---

## Evolución recomendada para Webserv

### Fase 1

- aceptar conexiones
- responder Hello World

### Fase 2

- parser HTTP/1.1
- GET

### Fase 3

- POST
- DELETE

### Fase 4

- múltiples servidores
- configuración nginx-like

### Fase 5

- CGI
- uploads
- autoindex

---

## Arquitectura recomendada

```text
ConfigParser
        │
        ▼
Server
        │
        ▼
EpollManager
        │
        ▼
Client
        │
        ▼
HttpRequest
        │
        ▼
Router
        │
        ▼
HttpResponse
```

---

## Pregunta típica de evaluación

¿Por qué epoll y no poll?

Respuesta:

- poll recorre todos los file descriptors en cada iteración.
- epoll mantiene internamente los eventos listos.
- epoll escala mucho mejor para miles de conexiones.
- ambos son válidos para Webserv, pero epoll suele ser más eficiente en Linux.

## LOGS
#include "../includes/Logger.hpp"

std::string Logger::_getTimestamp()
{
	char		buffer[100];
	std::time_t	now = std::time(NULL);
	std::tm*	ltm = std::localtime(&now);

	std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", ltm);
	return (std::string(buffer));
}

void Logger::log(LogLevel level, const std::string& message)
{
	std::string levelStr;

	if (level == INFO)
		std::cout << GREEN << "[INFO] " << "\t" << _getTimestamp() << " : " << message << RESET << std::endl;
	else if (level == TRACE)
		std::cout << MAGENTA << "[TRACE] " << "" << _getTimestamp() << " : " << message << RESET << std::endl;
	else if	(level == FATAL)
		std::cout << RED << "[FATAL] " << "" << _getTimestamp() << " : " << message << RESET << std::endl;
}