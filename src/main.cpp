/* #include "../includes/Server.hpp"

int main()
{
	Server server(3000);
	server.init();
	server.run();
	return 0;
}
 */

#include "../includes/ConfigParser.hpp"
#include "../includes/ConfigValidator.hpp"
#include "../includes/Server.hpp"

#include <iostream>
#include <csignal>
#include <vector>

int main(int argc, char** argv)
{
	const char* path = (argc >= 2) ? argv[1] : "config/webserv.conf";

	std::signal(SIGPIPE, SIG_IGN);

	try {
		ConfigParser parser;
		std::vector<ServerConfig> servers = parser.parseFile(path);

		ConfigValidator validator;
		validator.validateAndNormalize(servers);

		// MVP: usamos solo el primer server block
		Server server(servers[0].port);
		server.init();
		server.run();
	}
	catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}