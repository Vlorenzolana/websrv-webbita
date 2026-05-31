#include "../includes/Client.hpp"

Client::Client(int fd, const ServerConfig& config, Server *server) : _socket_fd(fd), _bytes_sent(0), _state(READING_HEADERS), _config(config)
, _cgi_stdout_fd(-1), _cgi_stdin_fd(-1), _cgi_pid(-1), _server(server)
{
}

Client::~Client()
{
}

// --- Implementacion de Getters ---

int Client::getSocketFd() const
{
	return (_socket_fd);
}

ClientState Client::getState() const
{
	return (_state);
}

std::string& Client::getResponseBuffer()
{
	return (_response_buffer);
}

std::string& Client::getRequestBuffer()
{
	return (_request_buffer);
}

size_t Client::getBytesSent() const
{
	return (_bytes_sent);
}

const ServerConfig& Client::getConfig() const
{
	return (_config);
}

Request& Client::getRequest()
{
	return (_request);
}

// --- Implementacion de Setters y Metodos ---

void	Client::setState(ClientState new_state)
{
	_state = new_state;
}

void	Client::appendToRequestBuffer(const char* data, size_t len)
{
	_request_buffer.append(data, len);
}

void	Client::setResponseBuffer(const std::string& response)
{
	_response_buffer = response;
}

void	Client::incrementBytesSent(size_t bytes)
{
	_bytes_sent += bytes;
}

void Client::setCGIContext(pid_t pid, int stdin_fd, int stdout_fd)
{
    _cgi_pid = pid;
    _cgi_stdin_fd = stdin_fd;
    _cgi_stdout_fd = stdout_fd;
}

int Client::getCGIStdoutFd() const { return _cgi_stdout_fd; }
int Client::getCGIStdinFd() const { return _cgi_stdin_fd; }
pid_t Client::getCGIPid() const { return _cgi_pid; }
std::string& Client::getCGIBuffer() { return _cgi_buffer; }

void Client::clearCGIContext()
{
    _cgi_pid = -1;
    _cgi_stdin_fd = -1;
    _cgi_stdout_fd = -1;
    _cgi_buffer.clear();
}

Server& Client::getServer() const
{
	return (*_server);
}

void Client::setCGIStartTime(time_t time)
{
    this->_cgi_start_time = time;
}

time_t Client::getCGIStartTime() const
{
    return this->_cgi_start_time;
}

