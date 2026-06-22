NAME        =   webserv
CC          =   c++
CFLAGS      =   -Wall -Wextra -Werror -std=c++98

OBJ_DIR     =   obj
SRC_DIR     =   src
INC_DIR     =   include

SRC         =   $(SRC_DIR)/main.cpp \
                $(SRC_DIR)/Server.cpp \
                $(SRC_DIR)/ConfigParser.cpp \
                $(SRC_DIR)/ConfigValidator.cpp \
                $(SRC_DIR)/Request.cpp

OBJ         =   $(SRC:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

INCLUDES    =   -I$(INC_DIR)

GREEN       =   \033[1;32m
YELLOW      =   \033[1;33m
RESET       =   \033[0m


all: $(NAME)

$(NAME): $(OBJ)
	@echo "$(YELLOW)Linking object files to create binary...$(RESET)"
	$(CC) $(CFLAGS) $(OBJ) -o $(NAME)
	@echo "$(GREEN)✔ Webserv compiled successfully!$(RESET)"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	@echo "$(YELLOW)Compiling: $<...$(RESET)"
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	@echo "$(YELLOW)Cleaning object binaries...$(RESET)"
	@rm -rf $(OBJ_DIR)
	@echo "$(GREEN)✔ Object files removed.$(RESET)"

fclean: clean
	@echo "$(YELLOW)Purging full executable...$(RESET)"
	@rm -f $(NAME)
	@echo "$(GREEN)✔ Executable purged clean.$(RESET)"

re: fclean all

.PHONY: all clean fclean re