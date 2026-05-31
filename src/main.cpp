#include "../includes/Server.hpp"
#include "../includes/Config.hpp"
#include "../includes/Logger.hpp"
#include "../includes/CGI.hpp"

int main(int argc, char **argv)
{
	if (argc != 2)
	{
		Logger::log(FATAL, "Uso: " + std::string(argv[0]) + " <archivo_de_configuracion>");
		return (1);
	}

	setupSignalHandler();

	Config config;
	if (!config.parse(argv[1]))
	{
		Logger::log(FATAL, "Error al cargar la configuracion.");
		return (1);
	}

	try
	{
		Server server(config);
		Logger::log(INFO, "Server is ready to run");
		server.run();
	}
	catch (const std::exception& e)
	{
		Logger::log(FATAL, "Error " + std::string(e.what()));
		return (1);
	}
	

	return (0);
}
