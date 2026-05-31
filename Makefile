NAME = webserv

CC = c++ -Wall -Wextra -Werror -std=c++98 -g

RM = rm -rf

SRC_DIR = ./src/
OBJ_DIR = ./objects/

SRC = src/Server.cpp \
	src/Request.cpp \
	src/DirectoryHandler.cpp \
	src/main.cpp \
	src/Client.cpp \
	src/MethodHandler.cpp \
	src/RequestHandler.cpp \
	src/Response.cpp \
	src/Logger.cpp \
	src/CGI.cpp \
	src/User.cpp \
	src/ConfigParserLocation.cpp \
	src/ConfigParserServer.cpp \
	src/ConfigParserUtils.cpp \
	src/ConfigParser.cpp

OBJS = $(SRC:$(SRC_DIR)%.cpp=$(OBJ_DIR)%.o)

all: $(NAME)

$(NAME): $(OBJS) 
	$(CC) $(OBJS) -o $(NAME)

$(OBJ_DIR)%.o: $(SRC_DIR)%.cpp
	@mkdir -p $(dir $@)
	$(CC) -c $< -o $@ 

clean:
	$(RM) $(OBJ_DIR)

fclean: clean
	$(RM) $(NAME)

re: fclean all

.PHONY: all clean fclean re