CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude
SRC_DIR = src/main
OBJ_DIR = obj
BIN_DIR = bin

CC_TARGET = $(BIN_DIR)/cc45
CA_TARGET = $(BIN_DIR)/ca45

CC_SOURCES = $(SRC_DIR)/cc45_main.cpp
CA_SOURCES = $(SRC_DIR)/ca45_main.cpp

# Common objects
COMMON_SOURCES = $(SRC_DIR)/Lexer.cpp $(SRC_DIR)/Parser.cpp $(SRC_DIR)/AST.cpp $(SRC_DIR)/CodeGenerator.cpp
COMMON_OBJECTS = $(OBJ_DIR)/Lexer.o $(OBJ_DIR)/Parser.o $(OBJ_DIR)/AST.o $(OBJ_DIR)/CodeGenerator.o

CC_OBJECTS = $(OBJ_DIR)/cc45_main.o $(COMMON_OBJECTS)
CA_OBJECTS = $(OBJ_DIR)/ca45_main.o $(OBJ_DIR)/AssemblerLexer.o $(OBJ_DIR)/AssemblerParser.o $(COMMON_OBJECTS)

.PHONY: all clean test

all: $(CC_TARGET) $(CA_TARGET)

$(CC_TARGET): $(CC_OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(CA_TARGET): $(CA_OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR) build

test: all
	@echo "Running tests..."
	@bash src/test/test_compiler.sh
	@$(MAKE) test-opcodes

test-opcodes: all
	@echo "Validating opcodes and addressing modes..."
	@bash src/test/test_opcodes.sh
