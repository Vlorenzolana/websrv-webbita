#ifndef SERVER_HPP
#define SERVER_HPP

#include "CGIHandler.hpp"
#include "Config.hpp"
#include "Request.hpp"

#include <ctime>
#include <map>
#include <string>
#include <sys/epoll.h>
#include <sys/types.h>
#include <vector>

class Server
{
public:
    Server(const std::vector<ServerConfig>& servers);
    ~Server();

    void init();
    void run();

private:
    struct ListenerState
    {
        int port;
        std::string host;

        ListenerState() : port(0) {}
    };

    struct ClientState
    {
        Request request;
        int listenPort;
        std::string listenHost;
        std::time_t lastActivity;
        bool processing;

        ClientState() : listenPort(0), lastActivity(0), processing(false) {}
    };

    struct PendingResponse
    {
        std::string data;
        std::size_t offset;

        PendingResponse() : offset(0) {}
    };

    struct CgiState
    {
        int clientFd;
        pid_t childPid;
        int stdinFd;
        int stdoutFd;
        std::string input;
        std::size_t inputOffset;
        std::string output;
        std::time_t startTime;
        bool stdinClosed;
        bool stdoutClosed;
        bool childExited;
        bool timedOut;
        bool outputTooLarge;
        int exitStatus;

        CgiState()
            : clientFd(-1), childPid(-1), stdinFd(-1), stdoutFd(-1),
              inputOffset(0), startTime(0), stdinClosed(false),
              stdoutClosed(false), childExited(false), timedOut(false),
              outputTooLarge(false), exitStatus(0)
        {
        }
    };

    struct CgiPipeRef
    {
        pid_t childPid;
        bool isInput;

        CgiPipeRef() : childPid(-1), isInput(false) {}
        CgiPipeRef(pid_t pid, bool input) : childPid(pid), isInput(input) {}
    };

    std::vector<ServerConfig> _servers;
    std::vector<int> _listenFds;
    int _epollFd;

    std::map<int, ListenerState> _listeners;
    std::map<int, ClientState> _clients;
    std::map<int, PendingResponse> _pendingResponses;
    std::map<pid_t, CgiState> _cgiByPid;
    std::map<int, CgiPipeRef> _cgiPipeRefs;

    static const int CLIENT_TIMEOUT_SECONDS = 30;
    static const int CGI_TIMEOUT_SECONDS = 10;
    static const std::size_t CGI_MAX_OUTPUT_SIZE = 16 * 1024 * 1024;

    static bool _setNonBlocking(int fd);
    static bool _setCloseOnExec(int fd);
    static std::string _trim(const std::string& value);
    static std::string _toLower(const std::string& value);
    static std::string _intToString(long value);

    void _openListener(const ServerConfig& server);
    void _acceptClients(int listenerFd);
    void _handleClientEvent(int clientFd, unsigned int events);
    void _handleClientReadable(int clientFd);
    void _handleClientWritable(int clientFd);
    void _handleCgiEvent(int pipeFd, unsigned int events);
    void _handleCgiWritable(int pipeFd);
    void _handleCgiReadable(int pipeFd);

    void _closeConnection(int fd);
    void _queueResponse(int clientFd, const std::string& response);
    bool _flushResponse(int clientFd);
    void _updateClientBodyLimit(int clientFd);

    void _processRequest(int clientFd, const Request& request,
        int listenPort, const std::string& listenHost);
    bool _startCgi(int clientFd, const Request& request,
        const ServerConfig& server, const LocationConfig* location,
        const std::string& fullPath);
    void _closeCgiPipe(int fd);
    void _reapCgiProcesses();
    void _checkTimeouts();
    void _tryFinalizeCgi(pid_t childPid);
    void _terminateCgi(pid_t childPid, bool timedOut);
    void _eraseCgiState(pid_t childPid);

    const ServerConfig* _selectServerConfig(int listenPort,
        const std::string& listenHost, const Request& request) const;
    const ServerConfig* _selectDefaultServer(int listenPort,
        const std::string& listenHost) const;
    const LocationConfig* _matchLocation(const ServerConfig& server,
        const std::string& path) const;
    bool _isMethodAllowed(const LocationConfig* location,
        const std::string& method) const;
    std::string _resolvePath(const ServerConfig& server,
        const LocationConfig* location, const std::string& requestPath) const;
    bool _hasPathTraversal(const std::string& path) const;

    bool _findCgiInterpreter(const LocationConfig* location,
        const std::string& fullPath, std::string& interpreter) const;
    std::string _handleGet(const ServerConfig& server,
        const Request& request, const LocationConfig* location) const;
    std::string _handlePost(const ServerConfig& server,
        const Request& request, const LocationConfig* location) const;
    std::string _handleDelete(const ServerConfig& server,
        const Request& request, const LocationConfig* location) const;

    bool _saveRawUpload(const Request& request,
        const LocationConfig* location, std::string& savedName) const;
    bool _saveMultipartUpload(const Request& request,
        const LocationConfig* location, std::vector<std::string>& savedNames) const;
    static std::string _safeFileName(const std::string& value);
    static std::string _extractMultipartParameter(const std::string& header,
        const std::string& parameter);

    void _sendErrorResponse(int clientFd, int code,
        const ServerConfig* server, const LocationConfig* location);
    std::string _buildErrorResponse(int code, const ServerConfig* server,
        const LocationConfig* location) const;
    std::string _loadErrorBody(int code, const ServerConfig* server,
        const LocationConfig* location) const;
    std::string _buildCgiHttpResponse(const std::string& rawOutput) const;
    std::string _buildResponse(int code, const std::string& statusText,
        const std::string& contentType, const std::string& body,
        const std::map<std::string, std::string>& extraHeaders =
            std::map<std::string, std::string>()) const;
    std::string _defaultErrorBody(int code,
        const std::string& statusText) const;
    std::string _statusText(int code) const;
    std::string _getMimeType(const std::string& path) const;
    std::string _buildAutoindexPage(const std::string& directoryPath,
        const std::string& requestPath) const;
};

#endif
