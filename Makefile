NAME = webserv

CXX = c++
CXXFLAGS = -std=c++98 -Wall -Wextra -Werror -g

SRC_DIR = src
OBJ_DIR = obj
INCLUDE_DIR = include

SRCS = \
	$(SRC_DIR)/main.cpp \
	$(SRC_DIR)/Core.cpp \
	$(SRC_DIR)/Socket.cpp \
	$(SRC_DIR)/ListeningSocket.cpp \
	$(SRC_DIR)/ClientSocket.cpp \
	$(SRC_DIR)/HttpRequest.cpp

OBJS = $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

INCLUDES = -I$(INCLUDE_DIR)

.PHONY: all clean fclean re

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all
