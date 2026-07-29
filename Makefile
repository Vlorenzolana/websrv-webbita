NAME := webserv
CXX := c++
CXXFLAGS := -Wall -Wextra -Werror -std=c++98

SRCS := srcs/main.cpp \
        srcs/ConfigParser.cpp \
        srcs/ConfigValidator.cpp \
        srcs/Request.cpp \
        srcs/CGIHandler.cpp \
        srcs/Server.cpp

OBJS := $(SRCS:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -Iincludes -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

test: all
	./tests/smoke_test.sh

re: fclean all

.PHONY: all clean fclean test re
