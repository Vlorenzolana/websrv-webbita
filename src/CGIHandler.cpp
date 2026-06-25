#include "../includes/CGIHandler.hpp"
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <fcntl.h>

CGIHandler::CGIHandler(const std::string& scriptPath, const std::string& interpreterPath) 
	: _script_path(scriptPath), _interpreter_path(interpreterPath) {}

CGIHandler::~CGIHandler() {}

char **CGIHandler::_mapToEnvp()
{
	char **envp = new char*[_env_map.size() + 1];
	size_t i = 0;
	
	for (std::map<std::string, std::string>::const_iterator it = _env_map.begin(); it != _env_map.end(); ++it)
	{
		std::string entry = it->first + "=" + it->second;
		envp[i] = new char[entry.size() + 1];
		std::strcpy(envp[i], entry.c_str());
		i++;
	}
	envp[i] = NULL;
	return envp;
}

int CGIHandler::execute(const Request &request, const std::string &uploadPath)
{
	_env_map["REQUEST_METHOD"] = request.getMethod();
	_env_map["PATH_INFO"] = request.getPath();
	_env_map["QUERY_STRING"] = request.getQueryString();
	_env_map["SERVER_PROTOCOL"] = "HTTP/1.1";
	_env_map["CONTENT_LENGTH"] = request.getHeaderValue("Content-Length");
	_env_map["CONTENT_TYPE"] = request.getHeaderValue("Content-Type");
	_env_map["UPLOAD_PATH"] = uploadPath.empty() ? "./www/uploads" : uploadPath;

	int pipe_in[2];
	int pipe_out[2];
	if (pipe(pipe_in) < 0 || pipe(pipe_out) < 0)
		return -1;
	
	pid_t pid = fork();
	if (pid < 0)
	{
		close(pipe_in[0]);
		close(pipe_in[1]);
		close(pipe_out[0]);
		close(pipe_out[1]);
		return -1;
	}

	if (pid == 0) // Child
	{
		dup2(pipe_in[0], STDIN_FILENO);
		dup2(pipe_out[1], STDOUT_FILENO);
		
		close(pipe_in[1]);
		close(pipe_out[0]);
		close(pipe_in[0]);
		close(pipe_out[1]);

		char *args[3];
		args[0] = const_cast<char*>(_interpreter_path.c_str());
		args[1] = const_cast<char*>(_script_path.c_str());
		args[2] = NULL;

		char **envp = _mapToEnvp();
		execve(args[0], args, envp);
		for (size_t i = 0; envp[i] != NULL; i++)
			delete[] envp[i];
		delete[] envp;

		std::exit(1);
	}
	else // Parent
	{
		close(pipe_in[0]);
		close(pipe_out[1]);

		if (request.getMethod() == "POST" && !request.getBody().empty())
			write(pipe_in[1], request.getBody().c_str(), request.getBody().size());
		close(pipe_in[1]);
		fcntl(pipe_out[0], F_SETFL, O_NONBLOCK);
		return pipe_out[0];
	}
}