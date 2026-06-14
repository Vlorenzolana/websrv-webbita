NAME        =   webserv

CC          =   c++
CFLAGS      =   -Wall -Wextra -Werror -std=c++98

SRC_DIR     =   src
OBJ_DIR     =   obj
INC_DIR     =   includes

SRC         =   $(SRC_DIR)/main.cpp \
                $(SRC_DIR)/ConfigParser.cpp \
                $(SRC_DIR)/ConfigValidator.cpp

OBJ         =   $(SRC:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

GREEN       =   \033[0;32m
RED         =   \033[0;31m
RESET       =   \033[0;30m

all: $(NAME)

$(NAME): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(NAME)
	@echo "$(GREEN)[SUCCESS] $(NAME) compiled successfully!$(RESET)"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -I $(INC_DIR) -c $< -o $@

clean:
	@rm -rf $(OBJ_DIR)
	@echo "$(RED)[CLEAN] Object files removed.$(RESET)"

fclean: clean
	@rm -f $(NAME)
	@echo "$(RED)[FCLEAN] Executable $(NAME) removed.$(RESET)"

re: fclean all

.PHONY: all clean fclean re