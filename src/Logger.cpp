#include "../includes/Logger.hpp"

std::string Logger::_getTimestamp()
{
	char		buffer[100];
	std::time_t	now = std::time(NULL);
	std::tm*	ltm = std::localtime(&now);

	std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", ltm);
	return (std::string(buffer));
}

void Logger::log(LogLevel level, const std::string& message)
{
	std::string levelStr;

	if (level == INFO)
		std::cout << GREEN << "[INFO] " << "\t" << _getTimestamp() << " : " << message << RESET << std::endl;
	else if (level == TRACE)
		std::cout << MAGENTA << "[TRACE] " << "" << _getTimestamp() << " : " << message << RESET << std::endl;
	else if	(level == FATAL)
		std::cout << RED << "[FATAL] " << "" << _getTimestamp() << " : " << message << RESET << std::endl;
}

