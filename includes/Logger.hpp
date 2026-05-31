#ifndef LOGGER_HPP
#define LOGGER_HPP

#define RESET "\033[0m"
#define BLACK "\033[0;30m"
#define RED "\033[0;31m"
#define GREEN "\033[0;32m"
#define YELLOW "\033[0;33m"
#define BLUE "\033[0;34m"
#define MAGENTA "\033[0;35m"
#define CYAN "\033[0;36m"
#define WHITE "\033[0;37m"

#include <string>
#include <iostream>
#include <sstream>
#include <ctime>
#include <string>

enum LogLevel
{
	INFO,
	TRACE,
	FATAL
};

class Logger
{
	private:
		Logger();
		static std::string _getTimestamp();

	public:
		static void log(LogLevel level, const std::string& message);
};

#endif // LOGGER_HPP
