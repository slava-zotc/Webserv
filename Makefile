NAME = webserv

CXX = c++
CXXFLAGS = -std=c++98 -Wall -Wextra -Werror -g

TEST_DIR = test
SRC_DIR = src
OBJ_DIR = obj
INCLUDE_DIR = include

COMMON_SRCS = \
	$(SRC_DIR)/Core.cpp \
	$(SRC_DIR)/Socket.cpp \
	$(SRC_DIR)/ListeningSocket.cpp \
	$(SRC_DIR)/ClientSocket.cpp \
	$(SRC_DIR)/HttpRequest.cpp \
	$(SRC_DIR)/HttpResponse.cpp \
	$(SRC_DIR)/Route.cpp \
	$(SRC_DIR)/Router.cpp \
	$(SRC_DIR)/Server.cpp \
	$(SRC_DIR)/ConvigParser.cpp \
	$(SRC_DIR)/ConfigBuilder.cpp

SRCS = $(SRC_DIR)/main.cpp $(COMMON_SRCS)
OBJS = $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

TEST_NAME = test_project
TEST_OBJS = $(OBJ_DIR)/test_main.o \
	$(COMMON_SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

INCLUDES = -I$(INCLUDE_DIR)

.PHONY: all clean fclean re test

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

test: $(TEST_OBJS)
	$(CXX) $(CXXFLAGS) $(TEST_OBJS) -o $(TEST_NAME)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/%.o: $(TEST_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME) $(TEST_NAME)

re: fclean all