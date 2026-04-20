CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude
SRC_DIR = src/main
OBJ_DIR = obj
BIN_DIR = bin

CC_TARGET = $(BIN_DIR)/cc45
CA_TARGET = $(BIN_DIR)/ca45
CP_TARGET = $(BIN_DIR)/cp45

CC_SOURCES = $(SRC_DIR)/cc45_main.cpp
CA_SOURCES = $(SRC_DIR)/ca45_main.cpp

# Common objects
COMMON_SOURCES = $(SRC_DIR)/Lexer.cpp $(SRC_DIR)/Parser.cpp $(SRC_DIR)/AST.cpp $(SRC_DIR)/CodeGenerator.cpp $(SRC_DIR)/M65Emitter.cpp $(SRC_DIR)/Preprocessor.cpp $(SRC_DIR)/ConstantFolder.cpp $(SRC_DIR)/AssemblerOpcodeDatabase.cpp
COMMON_OBJECTS = $(OBJ_DIR)/Lexer.o $(OBJ_DIR)/Parser.o $(OBJ_DIR)/AST.o $(OBJ_DIR)/CodeGenerator.o $(OBJ_DIR)/M65Emitter.o $(OBJ_DIR)/Preprocessor.o $(OBJ_DIR)/ConstantFolder.o $(OBJ_DIR)/AssemblerOpcodeDatabase.o

CC_OBJECTS = $(OBJ_DIR)/cc45_main.o $(OBJ_DIR)/AssemblerLexer.o $(OBJ_DIR)/AssemblerParser.o $(OBJ_DIR)/AssemblerExpression.o $(OBJ_DIR)/AssemblerOptimizer.o $(OBJ_DIR)/AssemblerSimulatedOps.o $(OBJ_DIR)/AssemblerGenerator.o $(COMMON_OBJECTS)
CA_OBJECTS = $(OBJ_DIR)/ca45_main.o $(OBJ_DIR)/AssemblerLexer.o $(OBJ_DIR)/AssemblerParser.o $(OBJ_DIR)/AssemblerExpression.o $(OBJ_DIR)/AssemblerOptimizer.o $(OBJ_DIR)/AssemblerSimulatedOps.o $(OBJ_DIR)/AssemblerGenerator.o $(COMMON_OBJECTS)

MAN_DIR = man

.PHONY: all clean test man

all: $(CC_TARGET) $(CA_TARGET) $(CP_TARGET)

man: $(MAN_DIR)/cc45.1 $(MAN_DIR)/ca45.1

$(MAN_DIR)/%.1: doc/%.md
	@mkdir -p $(MAN_DIR)
	pandoc -s -t man $< -o $@ -M title="$(basename $(notdir $@))" -M section="1" -M date="$(shell date +%F)" -M footer="$(basename $(notdir $@)) manual" -M header="User Commands"

$(CC_TARGET): $(CC_OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(CP_TARGET): $(CC_TARGET)
	@mkdir -p $(BIN_DIR)
	ln -sf cc45 $(CP_TARGET)

$(CA_TARGET): $(CA_OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR) build

test: all
	@echo "Running compiler tests..."
	@bash src/test/test_compiler.sh
	@echo "Running assembler feature tests..."
	@bash src/test/test_assembler.sh
	@$(MAKE) test-opcodes

test-assembler: all
	@bash src/test/test_assembler.sh

test-opcodes: all
	@echo "Validating opcodes and addressing modes..."
	@bash src/test/test_opcodes.sh
