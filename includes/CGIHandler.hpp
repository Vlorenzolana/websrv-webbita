#ifndef CGI_HANDLER_HPP
#define CGI_HANDLER_HPP

#include "Request.hpp"
#include <map>
#include <string>
#include <sys/types.h>

// Represents an asynchronous active CGI child process and its pipe descriptors
struct CgiProcess
{
    pid_t pid;
    int stdinFd;
    int stdoutFd;

    CgiProcess() : pid(-1), stdinFd(-1), stdoutFd(-1) {}
};

class CGIHandler
{
public:
    // Orthodox Canonical Form
    CGIHandler(const std::string& scriptPath,
        const std::string& interpreterPath);
    ~CGIHandler();

    // Execution entry point
    bool execute(const Request& request, const std::string& uploadPath,
        const std::string& serverName, int serverPort,
        CgiProcess& process) const;

private:
    std::string _scriptPath;
    std::string _interpreterPath;

    // File descriptor and pipe management helpers
    static bool _setNonBlocking(int fd);
    static void _closePipe(int pipeFds[2]);

    // Path resolution and header conversion helpers
    static std::string _directoryName(const std::string& path);
    static std::string _baseName(const std::string& path);
    static std::string _headerToCgiName(const std::string& name);

    // Environment array memory management
    static char** _mapToEnvp(const std::map<std::string, std::string>& envMap);
    static void _freeEnvp(char** envp);

    // CGI environment builder
    void _buildEnvironment(const Request& request,
        const std::string& uploadPath, const std::string& serverName,
        int serverPort, std::map<std::string, std::string>& envMap) const;
};

#endif