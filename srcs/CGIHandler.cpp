#include "../includes/CGIHandler.hpp"

#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <fcntl.h>
#include <limits.h>
#include <sstream>
#include <unistd.h>

CGIHandler::CGIHandler(const std::string& scriptPath,
    const std::string& interpreterPath)
    : _scriptPath(scriptPath), _interpreterPath(interpreterPath)
{
}

CGIHandler::~CGIHandler() {}

bool CGIHandler::_setNonBlocking(int fd)
{
    const int flags = fcntl(fd, F_GETFL, 0);
    return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) >= 0;
}

bool CGIHandler::_setCloseOnExec(int fd)
{
    const int flags = fcntl(fd, F_GETFD, 0);
    return flags >= 0 && fcntl(fd, F_SETFD, flags | FD_CLOEXEC) >= 0;
}

void CGIHandler::_closePipe(int pipeFds[2])
{
    if (pipeFds[0] >= 0)
        close(pipeFds[0]);
    if (pipeFds[1] >= 0)
        close(pipeFds[1]);
    pipeFds[0] = -1;
    pipeFds[1] = -1;
}

std::string CGIHandler::_directoryName(const std::string& path)
{
    const std::size_t slash = path.find_last_of('/');
    if (slash == std::string::npos)
        return ".";
    if (slash == 0)
        return "/";
    return path.substr(0, slash);
}

std::string CGIHandler::_baseName(const std::string& path)
{
    const std::size_t slash = path.find_last_of('/');
    if (slash == std::string::npos)
        return path;
    return path.substr(slash + 1);
}

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

void CGIHandler::_freeEnvp(char** envp)
{
    if (envp == NULL)
        return;
    for (std::size_t i = 0; envp[i] != NULL; ++i)
        delete[] envp[i];
    delete[] envp;
}

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
    char resolvedScript[PATH_MAX];
    if (realpath(_scriptPath.c_str(), resolvedScript) != NULL)
        envMap["SCRIPT_FILENAME"] = resolvedScript;
    else
        envMap["SCRIPT_FILENAME"] = _scriptPath;
    envMap["PATH_INFO"] = "";
    envMap["CONTENT_LENGTH"] = bodySize.str();
    envMap["CONTENT_TYPE"] = request.getHeaderValue("content-type");
    envMap["REDIRECT_STATUS"] = "200";
    char resolvedUpload[PATH_MAX];
    if (!uploadPath.empty() && realpath(uploadPath.c_str(), resolvedUpload) != NULL)
        envMap["UPLOAD_PATH"] = resolvedUpload;
    else
        envMap["UPLOAD_PATH"] = uploadPath;

    const Request::HeaderMap& headers = request.getHeaders();
    for (Request::HeaderMap::const_iterator it = headers.begin();
         it != headers.end(); ++it)
    {
        if (it->first != "content-length" && it->first != "content-type")
            envMap[_headerToCgiName(it->first)] = it->second;
    }
}

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
    if (!_setNonBlocking(inputPipe[1]) || !_setNonBlocking(outputPipe[0]) ||
        !_setCloseOnExec(inputPipe[0]) || !_setCloseOnExec(inputPipe[1]) ||
        !_setCloseOnExec(outputPipe[0]) || !_setCloseOnExec(outputPipe[1]))
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
        setpgid(0, 0);
        if (dup2(inputPipe[0], STDIN_FILENO) < 0 ||
            dup2(outputPipe[1], STDOUT_FILENO) < 0)
            _exit(126);

        close(inputPipe[0]);
        close(inputPipe[1]);
        close(outputPipe[0]);
        close(outputPipe[1]);

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

    setpgid(pid, pid);
    _freeEnvp(envp);
    close(inputPipe[0]);
    close(outputPipe[1]);
    inputPipe[0] = -1;
    outputPipe[1] = -1;

    process.pid = pid;
    process.stdinFd = inputPipe[1];
    process.stdoutFd = outputPipe[0];
    return true;
}
