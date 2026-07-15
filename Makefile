NAME        =   webserv
CC          =   c++
CFLAGS      =   -Wall -Wextra -Werror -std=c++98
LDFLAGS     =   -lpthread

OBJ_DIR     =   obj
SRC_DIR     =   src
INC_DIR     =   includes

SRC         =   $(SRC_DIR)/main.cpp \
                $(SRC_DIR)/Server.cpp \
                $(SRC_DIR)/ConfigParser.cpp \
                $(SRC_DIR)/ConfigValidator.cpp \
                $(SRC_DIR)/Request.cpp \
                $(SRC_DIR)/CGIHandler.cpp

OBJ         =   $(SRC:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

INCLUDES    =   -I$(INC_DIR)

# Colors
GREEN       =   \033[1;32m
YELLOW      =   \033[1;33m
BLUE        =   \033[1;34m
RED         =   \033[1;31m
RESET       =   \033[0m


# Targets
all: $(NAME)

$(NAME): $(OBJ)
	@echo "$(YELLOW)Linking object files to create binary...$(RESET)"
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJ) -o $(NAME)
	@echo "$(GREEN)✔ Webserv compiled successfully!$(RESET)"
	@echo "$(BLUE)Run: ./$(NAME) config/multivhost.conf$(RESET)"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	@echo "$(YELLOW)Compiling: $<$(RESET)"
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

help:
	@echo ""
	@echo "$(BLUE)╔════════════════════════════════════════════╗$(RESET)"
	@echo "$(BLUE)║  WebServ - HTTP Server with CGI Support   ║$(RESET)"
	@echo "$(BLUE)╚════════════════════════════════════════════╝$(RESET)"
	@echo ""
	@echo "$(YELLOW)Build Targets:$(RESET)"
	@echo "  $(GREEN)make$(RESET)              Compile webserv"
	@echo "  $(GREEN)make clean$(RESET)        Remove .o files in obj/"
	@echo "  $(GREEN)make fclean$(RESET)       Remove .o files + binary"
	@echo "  $(GREEN)make re$(RESET)           Full rebuild (fclean + all)"
	@echo "  $(GREEN)make help$(RESET)         Show this help"
	@echo ""
	@echo "$(YELLOW)Execution:$(RESET)"
	@echo "  $(GREEN)./webserv config/multivhost.conf$(RESET)"
	@echo ""
	@echo "$(YELLOW)Testing:$(RESET)"
	@echo "  $(GREEN)bash test_multivhost.sh$(RESET)          Bash tests"
	@echo "  $(GREEN)powershell test_multivhost.ps1$(RESET)  PowerShell tests"
	@echo ""
	@echo "$(YELLOW)Documentation:$(RESET)"
	@echo "  $(GREEN)README_42_STYLE.md$(RESET)           Deep technical overview"
	@echo "  $(GREEN)ARCHITECTURE.md$(RESET)              Flow diagrams"
	@echo "  $(GREEN)TESTING_COMPLETE_GUIDE.md$(RESET)    Test examples"
	@echo ""

.PHONY: all clean fclean re help