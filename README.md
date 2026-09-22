*This project has been created as part of the 42 curriculum by vlorenzo and oiahidal.*

# webserv

## Description

`webserv` is an HTTP/1.1 server written in C++98 for the 42 curriculum. Its goal is to serve static resources, receive uploads, execute CGI programs, and handle several virtual hosts and listening sockets through a non-blocking event-driven architecture.

The server supports:

- `GET`, `POST`, and `DELETE` methods.
- Static files, index files, directory listings, redirects, and custom errors.
- Binary and `multipart/form-data` uploads.
- Incremental HTTP parsing, `Content-Length`, and chunked transfer encoding.
- Per-location method rules and body-size limits.
- Virtual hosts selected by the `Host` header.
- Asynchronous CGI execution through non-blocking pipes.
- CGI timeouts, output limits, process cleanup, and error handling.
- Symlink protection for file deletion.

## Instructions

### Requirements

The project requires a Linux environment because it uses POSIX sockets, `fork`, `pipe`, `execve`, `epoll`, and `waitpid`. On Windows, use WSL.

### Compilation

From the repository root:

```sh
make
```

The project is compiled with:

```text
-Wall -Wextra -Werror -std=c++98
```

Useful commands are:

```sh
make clean
make fclean
make re
make test
make leaks
```

`make leaks` requires Valgrind. To run the smoke test on a chosen test port:

```sh
./tests/smoke_test.sh 18090
LEAK_CHECK=1 ./tests/smoke_test.sh 18090
```

The smoke test uses the selected port and `port + 1` for its second listener.

### Running the server

Start the default configuration with:

```sh
./webserv config/webserv.conf
```

The default configuration listens on `127.0.0.1:8081`.

The command also accepts a numeric port override:

```sh
./webserv config/webserv.conf 8082
```

`PORT+1` increments the port from the configuration file:

```sh
./webserv config/webserv.conf PORT+1
```

If the configuration contains `listen 127.0.0.1:8081;`, this starts the server on port `8082`.

### Upload and download example

The `/uploads` location in `config/webserv.conf` allows only `GET` and `POST`:

```nginx
location /uploads {
    allowed_methods GET POST;
    root ./www/uploads;
    upload_path ./www/uploads;
    autoindex on;
}
```

Upload a file with `POST`:

```sh
printf 'hello from webserv\n' > sample.txt
curl -i -X POST --data-binary @sample.txt \
    http://127.0.0.1:8081/uploads/sample.txt
```

Download it with `GET`:

```sh
curl -o downloaded.txt \
    http://127.0.0.1:8081/uploads/sample.txt
cmp sample.txt downloaded.txt
```

`cmp` prints nothing when both files are identical. Use `curl.exe` instead of `curl` in PowerShell, because PowerShell aliases `curl` to another command.

## Technical overview

### Program entry point

`srcs/main.cpp` parses the configuration, creates `Server`, initializes its listeners, and enters the event loop:

```cpp
ConfigParser parser;
std::vector<ServerConfig> servers = parser.parseFile(configPath);
Server server(servers);
server.init();
server.run();
```

The optional third argument is handled there. A numeric value overrides every configured server port; `PORT+1` increments every configured port.

### epoll and file descriptors

The server implementation is split across `srcs/Server.cpp`, `srcs/ServerNetwork.cpp`, `srcs/ServerCgi.cpp`, `srcs/ServerRequest.cpp`, and `srcs/ServerResources.cpp`, with state declarations in `includes/Server.hpp`. Linux represents sockets and pipes with integer file descriptors. The server stores their state in maps such as:

```cpp
std::map<int, ListenerState> _listeners;
std::map<int, ClientState> _clients;
std::map<int, CgiPipeRef> _cgiPipeRefs;
```

Instead of creating one thread per connection, the server registers all non-blocking descriptors in one `epoll` instance. The kernel reports only the descriptors that are ready, so one event loop can serve many clients:

```cpp
const int count = epoll_wait(_epollFd, events, 128, 1000);
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
```

The dispatch rules are:

```text
listener descriptor -> accept new clients
CGI pipe descriptor  -> read CGI output or write CGI input
client descriptor    -> read the HTTP request or send the response
```

Listeners use `EPOLLIN` to mean that a connection is waiting. Clients use `EPOLLIN` for incoming request data and `EPOLLOUT` when a response can be sent. Responses are stored in `_pendingResponses` with an offset, so partial `send()` calls resume on a later event.

### Request processing order

`Server::_processRequest()` in `srcs/ServerRequest.cpp` follows this order:

1. Select the server configuration using the listening socket and `Host`.
2. Match the request path to a `location`.
3. Apply `client_max_body_size`; an oversized body returns `413`.
4. Check `allowed_methods`; a disallowed method returns `405`.
5. Reject path traversal such as `..` with `403`.
6. Check whether the resolved file is a CGI resource.
7. Execute CGI, or dispatch to static `GET`, upload `POST`, or `DELETE`.

The body limit is first updated after the request headers are available in `Server::_updateClientBodyLimit()`, then checked again when the full request is processed. This allows the correct virtual host limit to be selected before the complete body arrives.

### CGI execution

CGI setup is implemented in `srcs/CGIHandler.cpp`; orchestration is in `srcs/ServerCgi.cpp`. The server progressively:

1. Resolves the requested path and confirms it is a regular file with `stat`.
2. Finds the configured interpreter for the file extension.
3. Rejects new work when `CGI_MAX_PROCESSES` is reached with `503`.
4. Starts the child and connects its standard input/output with pipes.
5. Writes the request body through a pipe when `EPOLLOUT` is reported.
6. Reads CGI output through a pipe when `EPOLLIN` is reported.
7. Uses `waitpid(..., WNOHANG)` so child collection never blocks the loop.
8. Builds the final HTTP response after the child exits and stdout reaches EOF.

CGI failures produce `500`, timeouts produce `504`, excessive output produces `502`, and the process limit produces `503`.

### Method and body-size checks

Methods are configured per location:

```nginx
location /uploads {
    allowed_methods GET POST;
}
```

Therefore `POST /uploads/file.txt` can upload and `GET /uploads/file.txt` can download, while `DELETE /uploads/file.txt` returns `405` unless `DELETE` is listed.

The server applies:

```nginx
client_max_body_size 1K;
```

to reject request bodies larger than 1024 bytes with `413`. A value of `0` means unlimited in this project.

### Symlink protection

`Server::_handleDelete()` in `srcs/Server.cpp` protects deletion without using `lstat`, which is outside the allowed function list for this project:

```cpp
const int fileFd = open(fullPath.c_str(), O_RDONLY | O_NOFOLLOW);
if (fileFd < 0)
    return _buildErrorResponse(errno == ELOOP ? 403 : 404,
        &server, location);
close(fileFd);
```

`O_NOFOLLOW` prevents `open` from following a symbolic link. A symlink returns `403`, a missing path returns `404`, and a normal regular file can continue to the `stat` and deletion checks.

### Virtual hosts and listeners

Configuration parsing is implemented in `srcs/ConfigParser.cpp`. Several `server` blocks may share the same IP and port:

```nginx
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

`Server::init()` opens one listener for each unique host and port pair. `_acceptClients()` records that pair in `ClientState`. Then `_selectServerConfig()` reads `Host`, removes an optional port, and compares the name only among servers belonging to that listener:

```sh
curl -i -H 'Host: alpha.localhost' http://127.0.0.1:8081/
curl -i -H 'Host: beta.localhost'  http://127.0.0.1:8081/
```

An unknown host uses the first server configured for that host and port as the fallback. Different IP addresses or ports create different listeners. The same configuration file may therefore create several listeners in one process and one `epoll` instance. Two separate processes cannot bind the same IP and port; they need different ports or addresses.

The demonstration configuration is [config/vhosts.conf](config/vhosts.conf).

## Resources

### References

- [RFC 7230: HTTP/1.1 Message Syntax and Routing](https://www.rfc-editor.org/rfc/rfc7230)
- [RFC 7231: HTTP/1.1 Semantics and Content](https://www.rfc-editor.org/rfc/rfc7231)
- [Linux `epoll` documentation](https://man7.org/linux/man-pages/man7/epoll.7.html)
- [Linux `socket` manual](https://man7.org/linux/man-pages/man2/socket.2.html)
- [Linux `fork` manual](https://man7.org/linux/man-pages/man2/fork.2.html)
- [Linux `execve` manual](https://man7.org/linux/man-pages/man2/execve.2.html)
- [Linux `waitpid` manual](https://man7.org/linux/man-pages/man2/waitpid.2.html)
- [curl manual](https://curl.se/docs/manpage.html)

### AI usage

AI assistance was used as a study and review aid for this project. It helped:

- Explain the existing `epoll` event loop, listener/client/CGI descriptor dispatch, and non-blocking I/O flow.
- Explain HTTP method validation, upload/download testing, body-size checks, and virtual-host selection.
- Prepare manual `curl` commands and clarify PowerShell versus Linux `curl`.
- Review the symlink-deletion protection and replace an initially considered `lstat` call with the allowed `open(..., O_NOFOLLOW)` approach.
- Add the optional `PORT+1` command-line behavior in `srcs/main.cpp`.
- Draft and organize technical documentation in this README.

The implementation was compiled and behavior was checked with the project build and smoke-test commands. AI was not used as a substitute for understanding or validating the code.

## Project files

- `Makefile`: compilation, smoke tests, and Valgrind target.
- `config/webserv.conf`: main server configuration.
- `config/vhosts.conf`: virtual-host demonstration configuration.
- `srcs/main.cpp`: argument handling and server startup.
- `srcs/Server.cpp`: server lifecycle and the main epoll loop.
- `srcs/ServerNetwork.cpp`: listeners, client sockets, epoll events, and response I/O.
- `srcs/ServerCgi.cpp`: CGI process lifecycle, pipes, output limits, and timeouts.
- `srcs/ServerRequest.cpp`: virtual-host selection, routing, validation, and request dispatch.
- `srcs/ServerResources.cpp`: static resources, uploads, deletion, MIME types, and HTTP responses.
- `srcs/Request.cpp`: incremental HTTP request parser.
- `srcs/CGIHandler.cpp`: CGI environment and process launch.
- `srcs/ConfigParser.cpp`: configuration parser.
- `srcs/ConfigValidator.cpp`: semantic configuration validation.
- `tests/smoke_test.sh`: automated regression checks.
