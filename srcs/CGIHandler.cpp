#include "../includes/CGIHandler.hpp"
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <fcntl.h>
#include <sstream>
#include <unistd.h>

CGIHandler::CGIHandler(const std::string& scriptPath,
    const std::string& interpreterPath)
    : _scriptPath(scriptPath), _interpreterPath(interpreterPath)
{
}

CGIHandler::~CGIHandler(void)
{
}

// Sets non-blocking I/O mode on the specified file descriptor
bool CGIHandler::_setNonBlocking(int fd)
{
    return fcntl(fd, F_SETFL, O_NONBLOCK) >= 0;
}

// Closes and resets both ends of a pipe array
void CGIHandler::_closePipe(int pipeFds[2])
{
    if (pipeFds[0] >= 0)
        close(pipeFds[0]);
    if (pipeFds[1] >= 0)
        close(pipeFds[1]);
    pipeFds[0] = -1;
    pipeFds[1] = -1;
}

// Extracts the directory path component of a filename
std::string CGIHandler::_directoryName(const std::string& path)
{
    const std::size_t slash = path.find_last_of('/');
    if (slash == std::string::npos)
        return ".";
    if (slash == 0)
        return "/";
    return path.substr(0, slash);
}

// Extracts the base filename from a path
std::string CGIHandler::_baseName(const std::string& path)
{
    const std::size_t slash = path.find_last_of('/');
    if (slash == std::string::npos)
        return path;
    return path.substr(slash + 1);
}

// Converts an HTTP header name to standard CGI meta-variable format (e.g., User-Agent -> HTTP_USER_AGENT)
std::string CGIHandler::_headerToCgiName(const std::string& name)
{
    std::string result = "HTTP_";
    for (std::size_t i = 0; i < name.size(); ++i)
    {
        char c = name[i];
        if (c == '-')
            c = '_';
        else if (c >= 'a' && c <= 'z')
            c = static_cast<char>(c - 'a' + 'A');
        result += c;
    }
    return result;
}

// Converts a std::map of key-value pairs into a NULL-terminated char** array for execve
char** CGIHandler::_mapToEnvp(
    const std::map<std::string, std::string>& envMap)
{
    char** envp = new char*[envMap.size() + 1];
    std::size_t index = 0;
    for (std::map<std::string, std::string>::const_iterator it = envMap.begin();
         it != envMap.end(); ++it)
    {
        const std::string entry = it->first + "=" + it->second;
        envp[index] = new char[entry.size() + 1];
        std::strcpy(envp[index], entry.c_str());
        ++index;
    }
    envp[index] = NULL;
    return envp;
}

// Deallocates dynamically allocated envp array
void CGIHandler::_freeEnvp(char** envp)
{
    if (envp == NULL)
        return;
    for (std::size_t i = 0; envp[i] != NULL; ++i)
        delete[] envp[i];
    delete[] envp;
}

// Constructs standard RFC 3875 environment variables from HTTP request details
void CGIHandler::_buildEnvironment(const Request& request,
    const std::string& uploadPath, const std::string& serverName,
    int serverPort, std::map<std::string, std::string>& envMap) const
{
    std::ostringstream port;
    port << serverPort;

    std::ostringstream bodySize;
    bodySize << request.getBody().size();

    envMap["GATEWAY_INTERFACE"] = "CGI/1.1";
    envMap["SERVER_SOFTWARE"] = "webserv/1.0";
    envMap["SERVER_PROTOCOL"] = request.getHttpVersion();
    envMap["SERVER_NAME"] = serverName;
    envMap["SERVER_PORT"] = port.str();
    envMap["REQUEST_METHOD"] = request.getMethod();
    envMap["QUERY_STRING"] = request.getQueryString();
    envMap["SCRIPT_NAME"] = request.getPath();
    envMap["SCRIPT_FILENAME"] = _scriptPath;
    envMap["PATH_INFO"] = "";
    envMap["CONTENT_LENGTH"] = bodySize.str();
    envMap["CONTENT_TYPE"] = request.getHeaderValue("content-type");
    envMap["REDIRECT_STATUS"] = "200";
    envMap["UPLOAD_PATH"] = uploadPath;

    // Convert custom request headers into CGI HTTP_* variables
    const Request::HeaderMap& headers = request.getHeaders();
    for (Request::HeaderMap::const_iterator it = headers.begin();
         it != headers.end(); ++it)
        if (it->first != "content-length" && it->first != "content-type")
            envMap[_headerToCgiName(it->first)] = it->second;
}

// Creates asynchronous pipes, forks the process, redirects STDIO, and executes the CGI script
bool CGIHandler::execute(const Request& request, const std::string& uploadPath,
    const std::string& serverName, int serverPort, CgiProcess& process) const
{
    int inputPipe[2] = {-1, -1};
    int outputPipe[2] = {-1, -1};

    if (pipe(inputPipe) < 0)
        return false;
    if (pipe(outputPipe) < 0)
    {
        _closePipe(inputPipe);
        return false;
    }

    if (!_setNonBlocking(inputPipe[1]) || !_setNonBlocking(outputPipe[0]))
    {
        _closePipe(inputPipe);
        _closePipe(outputPipe);
        return false;
    }

    std::map<std::string, std::string> envMap;
    _buildEnvironment(request, uploadPath, serverName, serverPort, envMap);
    char** envp = _mapToEnvp(envMap);

    const pid_t pid = fork();
    if (pid < 0)
    {
        _freeEnvp(envp);
        _closePipe(inputPipe);
        _closePipe(outputPipe);
        return false;
    }

    if (pid == 0)
    {
        if (dup2(inputPipe[0], STDIN_FILENO) < 0 ||
            dup2(outputPipe[1], STDOUT_FILENO) < 0)
            _exit(126);

        close(inputPipe[0]);
        close(inputPipe[1]);
        close(outputPipe[0]);
        close(outputPipe[1]);

        // Change directory to script folder for relative path support
        const std::string directory = _directoryName(_scriptPath);
        const std::string scriptName = _baseName(_scriptPath);

        if (chdir(directory.c_str()) < 0)
            _exit(126);

        char* arguments[3];
        arguments[0] = const_cast<char*>(_interpreterPath.c_str());
        arguments[1] = const_cast<char*>(scriptName.c_str());
        arguments[2] = NULL;

        execve(arguments[0], arguments, envp);

        _freeEnvp(envp);
        _exit(127);
    }

    _freeEnvp(envp);

    // Parent keeps write-end of input pipe and read-end of output pipe
    close(inputPipe[0]);
    close(outputPipe[1]);
    inputPipe[0] = -1;
    outputPipe[1] = -1;

    process.pid = pid;
    process.stdinFd = inputPipe[1];
    process.stdoutFd = outputPipe[0];

    return true;
}