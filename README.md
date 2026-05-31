# Webserv - A High-Performance HTTP Server

![Language](https://img.shields.io/badge/language-C++98-blue)
![Standard](https://img.shields.io/badge/standard-HTTP%2F1.1-green)
![Network](https://img.shields.io/badge/network-Non--blocking%20I%2FO-orange)
![Build](https://img.shields.io/badge/build-passing-brightgreen)

<br />
<p align="center">
  <h3 align="center">A custom C++ HTTP server implementing RFC 7230-7235</h3>
</p>

## 🗣️ About The Project

**Webserv** is a comprehensive project from the 42 curriculum designed to write a functional HTTP/1.1 server from scratch in **C++ 98**.

The primary challenge of this project is to handle **web networking** without relying on high-level libraries (like Apache or Nginx modules). It requires a deep understanding of the **TCP/IP stack**, **socket programming**, and asynchronous event handling.

This project proves the ability to build robust backend infrastructure capable of handling multiple clients simultaneously without blocking the CPU.

### 🎯 Key Engineering Goals
- **Concurrency:** Implementing a single-threaded, non-blocking architecture using **I/O Multiplexing** (`poll()` / `select()`).
- **Protocol Compliance:** Strictly following the HTTP/1.1 RFC standards to handle GET, POST, and DELETE methods.
- **Data Management:** Parsing complex HTTP headers, chunked transfer encoding, and multipart form data.
- **Reliability:** Ensuring the server stays up indefinitely (100% uptime) even under stress or bad requests.

---

## 🏗️ Architecture

The server operates on an **Event-Driven Architecture** (Reactor Pattern).

1.  **Socket Initialization:** Sets up listening sockets on configured ports.
2.  **The Event Loop:** The main loop waits for events using `poll()`. It monitors file descriptors for:
    * **Incoming Connections:** Accepts new clients.
    * **Incoming Data (Read):** Reads raw bytes from the socket.
    * **Outgoing Data (Write):** Sends prepared responses back to the client.
3.  **Request Parser:** Converts raw bytes into a structured HTTP Request object.
4.  **Router / Config:** Matches the request URL to the configuration file (routes, root directories, allowed methods).
5.  **Response Builder:** Generates the appropriate HTTP status codes (200, 404, 500) and body content.

---

## 🛠️ Technical Implementation Highlights

### 1. Non-Blocking I/O & Multiplexing
Unlike traditional servers that fork a process or create a thread for every client (which is resource-heavy), Webserv uses **synchronous I/O multiplexing**. This allows a single process to monitor multiple file descriptors, handling thousands of connections efficiently with minimal context switching overhead—a critical concept for high-performance telemetry systems.

### 2. CGI (Common Gateway Interface)
The server can execute external scripts (like Python or PHP) to generate dynamic content.
* **Implementation:** I manually managed the environment variables (`REQUEST_METHOD`, `QUERY_STRING`), set up pipes to capture the script's output, and `fork()`/`execve()` the interpreter, ensuring the main server loop doesn't hang while the script runs.

### 3. Configuration Parsing
Implemented a custom parser for Nginx-like configuration files. The server supports:
* Multiple virtual servers (ports/hostnames).
* Custom error pages.
* Client body size limits.
* Directory listing and auto-indexing.
* HTTP redirection.

---

## ⚡ Features

* **HTTP Methods:** Support for `GET`, `POST`, and `DELETE`.
* **Static Website Hosting:** Serves HTML, CSS, JS, and image files.
* **File Uploads:** Handles multipart/form-data for uploading files to the server.
* **CGI Support:** Executes backend scripts (e.g., .py, .php).
* **Auto-Index:** Generates directory listings if no index file is found.
* **Stress Tested:** Validated with `siege` to ensure memory stability under load.

---

## 🚀 Installation & Usage

### Requirements
* C++ Compiler (c++98 standard)
* GNU Make

### Build and Run
~~~bash
# Clone the repository
git clone https://github.com/eandres83/webserv.git

# Enter the directory
cd webserv

# Compile
make

# Run with the default configuration
./webserv config/default.conf
~~~

### Testing
You can test the server by opening a browser and navigating to `http://localhost:8080` (or the port defined in your config), or using `curl`:

~~~bash
curl -v http://localhost:8080/index.html
~~~

---

## 🧪 Testing Strategy

* **Siege:** Used for load testing and checking memory leaks under pressure.
* **Telnet:** Used for sending raw, malformed requests to test parser robustness.
* **Browser Compatibility:** Tested across Chrome, Firefox, and Safari.

---

