#pragma once
#include <vector>
#include <string>
#include <map>
#include <cstdint>
#include "AssemblerToken.hpp"

enum class AddressingMode {
    IMPLIED,
    ACCUMULATOR,
    IMMEDIATE,
    IMMEDIATE16,
    BASE_PAGE,
    BASE_PAGE_X,
    BASE_PAGE_Y,
    ABSOLUTE,
    ABSOLUTE_X,
    ABSOLUTE_Y,
    INDIRECT,
    BASE_PAGE_X_INDIRECT,   // (bp,X)
    BASE_PAGE_INDIRECT_Y,   // (bp),Y
    BASE_PAGE_INDIRECT_Z,   // (bp),Z
    BASE_PAGE_INDIRECT_SP_Y,// (bp,SP),Y
    ABSOLUTE_INDIRECT,      // (abs)
    ABSOLUTE_X_INDIRECT,    // (abs,X)
    RELATIVE,
    RELATIVE16,             // relfar
    BASE_PAGE_RELATIVE,     // bp+rel8 (BBR/BBS)
    FLAT_INDIRECT_Z,        // [bp],Z (EOM prefix)
    QUAD_Q,                 // Q register (double NEG prefix)
    STACK_RELATIVE,         // offset, s
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
    std::string bitBranchTarget;
    std::vector<std::string> callArgs;
    uint32_t address;
    int size;
    int line;
    int procParamSize = 0; 
    std::string bitNum; 
};

struct Directive {
    std::string name;
    std::vector<std::string> arguments;
    uint32_t address;
    int size;
    int line;
    std::string varName; 
    enum VarType { NONE, ASSIGN, INC, DEC } varType = NONE;
    int tokenIndex = -1; 
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
        bool isExpr = false;
        std::string exprTarget;
        int exprTokenIndex = -1;
    };
    std::vector<Statement> statements;

    const AssemblerToken& peek() const;
    const AssemblerToken& advance();
    bool match(AssemblerTokenType type);
    const AssemblerToken& expect(AssemblerTokenType type, const std::string& message);
    
    int calculateInstructionSize(const Instruction& instr);
    int calculateDirectiveSize(const Directive& dir);
    int calculateExprSize(int tokenIndex);
    uint8_t getOpcode(const std::string& mnemonic, AddressingMode mode);
    uint32_t evaluateExpressionAt(int index);
    void emitExpressionCode(std::vector<uint8_t>& binary, const std::string& target, int tokenIndex);
};
