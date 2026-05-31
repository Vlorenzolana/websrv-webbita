#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <iostream>
#include <sys/socket.h>
#include "Config.hpp"
#include "Request.hpp"
#include "User.hpp"

enum ClientState
{
	READING_HEADERS,
	READING_BODY,
	REQUEST_READY,
	WRITING,  // El servidor esta escribiendo datos al cliente
	CGI_RUNNING
};

class Server;

class Client
{
	private:
		int		_socket_fd;
		std::string	_request_buffer;
		std::string	_response_buffer;
		size_t		_bytes_sent;
		ClientState	_state;	
		Request		_request;
		const ServerConfig& _config;

		// --- Contexto CGI ---
		int       _cgi_stdout_fd;
		int       _cgi_stdin_fd;
		pid_t     _cgi_pid;
		std::string _cgi_buffer;
		Server	*_server;
		time_t              _cgi_start_time;

	//Referencia a listado de usuarios


	public:
		Client(int fd, const ServerConfig& config, Server *server);
		~Client();

		// --- Getters ---
		int		getSocketFd() const;
		std::string&	getResponseBuffer();
		std::string&	getRequestBuffer();
		Request&	getRequest();
		ClientState	getState() const;
		const ServerConfig& getConfig() const;
		size_t		getBytesSent() const;
		Server& getServer() const;

		// --- Setters y metodos de accion ---
		void		setState(ClientState new_state);
		void		appendToRequestBuffer(const char* data, size_t len);
		void		setResponseBuffer(const std::string& response);
		void		incrementBytesSent(size_t bytes);

        // --- CGI helpers ---
        void setCGIContext(pid_t pid, int stdin_fd, int stdout_fd);
        int  getCGIStdoutFd() const;
        int  getCGIStdinFd() const;
        pid_t getCGIPid() const;
        std::string& getCGIBuffer();
        void clearCGIContext();
        void                setCGIStartTime(time_t time);
        time_t              getCGIStartTime() const;
};

#endif // CLIENT_HPP
