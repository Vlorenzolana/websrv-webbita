#include "../includes/CGI.hpp"
#include "../includes/Logger.hpp"
#include <fcntl.h>

volatile sig_atomic_t g_shutdown_flag = 0;

std::vector<char *> setEnv(Client &client)
{
    const Request &request = client.getRequest();
    const ServerConfig &config = client.getConfig();

    std::vector<std::string> env_vars;
    env_vars.push_back("GATEWAY_INTERFACE=CGI/1.1");
    env_vars.push_back("SERVER_SOFTWARE=webserv/1.0");
    env_vars.push_back("SERVER_NAME=localhost");
    {
        std::ostringstream ss;
        ss << config.port;
        env_vars.push_back("SERVER_PORT=" + ss.str());
    }
    env_vars.push_back("SERVER_PROTOCOL=" + request.getHttpVersion());
    env_vars.push_back("REQUEST_METHOD=" + request.getMethod());
    env_vars.push_back("QUERY_STRING=" + request.getQueryString());
    env_vars.push_back("PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin");

    // Rutas
    env_vars.push_back("SCRIPT_FILENAME=" + (config.root_directory + request.getPath()));
    env_vars.push_back("SCRIPT_NAME=" + request.getPath());
    {
        std::string uri = request.getPath();
        if (!request.getQueryString().empty())
            uri += "?" + request.getQueryString();
        env_vars.push_back("REQUEST_URI=" + uri);
    }
    env_vars.push_back("DOCUMENT_ROOT=" + config.root_directory);

    // Opcional: si manejas POST hacia CGI
    if (!request.getHeaderValue("Content-Type").empty())
        env_vars.push_back("CONTENT_TYPE=" + request.getHeaderValue("Content-Type"));
    if (!request.getHeaderValue("Content-Length").empty())
        env_vars.push_back("CONTENT_LENGTH=" + request.getHeaderValue("Content-Length"));
    // Necesario para algunos intérpretes CGI (p.ej. php-cgi)
    env_vars.push_back("REDIRECT_STATUS=200");

    std::vector<char *> envp;
    for (unsigned long i = 0; i < env_vars.size(); i++)
        envp.push_back(strdup(env_vars[i].c_str()));
    envp.push_back(NULL);
    return (envp);
}

int manageCGI(Client &client, Response &response)
{
    (void)response;
    const Request &request = client.getRequest();
    const ServerConfig &config = client.getConfig();
    std::string full_path = config.root_directory + request.getPath();
    std::string extension = full_path.substr(full_path.rfind('.'));
    std::string execPath = MethodHandler::getMimeType(full_path);

    std::cout << YELLOW << "[INFO] Cgi management execPath: " << execPath << RESET << std::endl;

    int pipe_in[2];
    int pipe_out[2];
    if (pipe(pipe_in) < 0 || pipe(pipe_out) < 0)
        return -1;

    pid_t pid = fork();
    if (pid == 0)
    {
        dup2(pipe_in[0], STDIN_FILENO);
        dup2(pipe_out[1], STDOUT_FILENO);

        close(pipe_in[1]);
        close(pipe_out[0]);

        char *argv[] = {(char *)execPath.c_str(), (char *)full_path.c_str(), NULL};
        std::vector<char *> env = setEnv(client);
        execve(argv[0], argv, env.data());
        exit(1);
    }
    else if (pid > 0)
    {
        // Proceso padre: gestionar stdin/stdout del CGI
        close(pipe_in[0]);
        close(pipe_out[1]);

        // Para POST, enviar el cuerpo al stdin del CGI
        if (request.getMethod() == "POST")
        {
            const std::string &body = request.getBody();
            size_t total = 0;
            while (total < body.size())
            {
                ssize_t n = write(pipe_in[1], body.data() + total, body.size() - total);
                if (n < 0)
                {
                    if (n == EINTR)
                        continue;
                    Logger::log(FATAL, "Write to CGI stdin");
                    break;
                }
                total += static_cast<size_t>(n);
            }
        }
        // Cerrar siempre stdin del CGI en el padre
        close(pipe_in[1]);

        // No bloqueante en stdout del CGI
        fcntl(pipe_out[0], F_SETFL, O_NONBLOCK);

        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = pipe_out[0];
        epoll_ctl(g_epoll_fd, EPOLL_CTL_ADD, pipe_out[0], &ev);

        client.setCGIContext(pid, -1, pipe_out[0]);
        client.setCGIStartTime(time(NULL));
        client.setState(CGI_RUNNING);
        return 0;
    }
    return -1;
}

void signalHandler(int signum)
{
    (void)signum;
    g_shutdown_flag = 1;
}

void setupSignalHandler()
{
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
}
