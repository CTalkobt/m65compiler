#pragma once
#include <vector>
#include <string>
#include <map>
#include <cstdint>
#include "AssemblerToken.hpp"

enum class AddressingMode {
    IMPLIED,
    IMMEDIATE,
    ABSOLUTE,
    ABSOLUTE_X,
    ABSOLUTE_Y,
    RELATIVE,
    INDIRECT,
    STACK_RELATIVE,
};

struct Symbol {
    uint32_t value;
    bool isAddress; 
    int size;       
    bool isVariable = false;
    uint32_t initialValue = 0;
};

struct Instruction {
    std::string mnemonic;
    AddressingMode mode;
    std::string operand;
    std::vector<std::string> callArgs;
    uint32_t address;
    int size;
    int line;
    int procParamSize = 0; 
};

struct Directive {
    std::string name;
    std::vector<std::string> arguments;
    uint32_t address;
    int size;
    int line;
    std::string varName; 
    enum VarType { NONE, ASSIGN, INC, DEC } varType = NONE;
    int tokenIndex = -1; // Index of the token after the operator
};

class AssemblerParser {
public:
    AssemblerParser(const std::vector<AssemblerToken>& tokens);
    void pass1();
    std::vector<uint8_t> pass2();

private:
    std::vector<AssemblerToken> tokens;
    size_t pos;
    uint32_t pc;
    std::map<std::string, Symbol> symbolTable;
    
    struct ProcContext {
        std::string name;
        int totalParamSize;
        std::map<std::string, int> localArgs; 
    };
    ProcContext* currentProc = nullptr;
    std::map<uint32_t, ProcContext> procedures; 

    struct Statement {
        bool isInstruction;
        Instruction instr;
        Directive dir;
        std::string label;
    };
    std::vector<Statement> statements;

    const AssemblerToken& peek() const;
    const AssemblerToken& advance();
    bool match(AssemblerTokenType type);
    
    int calculateInstructionSize(const Instruction& instr);
    int calculateDirectiveSize(const Directive& dir);
    uint8_t getOpcode(const std::string& mnemonic, AddressingMode mode);
    uint32_t parseNumericLiteral(const std::string& literal);
    uint32_t evaluateExpressionAt(int index);
};
