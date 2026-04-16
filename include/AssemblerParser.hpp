#pragma once
#include <vector>
#include <string>
#include <map>
#include <deque>
#include <memory>
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
    bool isStackRelative = false;
    int stackOffset = 0;
};

struct Instruction {
    std::string mnemonic;
    AddressingMode mode;
    std::string operand;
    std::string bitBranchTarget;
    std::vector<std::string> callArgs;
    int procParamSize = 0; 
};

struct Directive {
    std::string name;
    std::vector<std::string> arguments;
    std::string varName; 
    enum VarType { NONE, ASSIGN, INC, DEC } varType = NONE;
    int tokenIndex = -1; 
};

class AssemblerParser {
public:
    AssemblerParser(const std::vector<AssemblerToken>& tokens);
    AssemblerParser(const std::vector<AssemblerToken>& tokens, const std::map<std::string, uint32_t>& predefinedSymbols);
    void pass1();
    std::vector<uint8_t> pass2();
    uint32_t getZPStart() const;

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
        enum Type { NONE, INSTRUCTION, DIRECTIVE, EXPR, BASIC_UPSTART, MUL, DIV } type = NONE;
        Instruction instr;
        Directive dir;
        std::string label;
        uint32_t address = 0;
        int size = 0;
        int line = 0;
        
        // EXPR specific
        std::string exprTarget;
        int exprTokenIndex = -1;
        
        // BASIC_UPSTART specific
        int basicUpstartTokenIndex = -1;

        // MUL specific
        int mulWidth = 8;
    };
    std::deque<std::unique_ptr<Statement>> statements;

    const AssemblerToken& peek() const;
    const AssemblerToken& advance();
    bool match(AssemblerTokenType type);
    const AssemblerToken& expect(AssemblerTokenType type, const std::string& message);
    
    int calculateInstructionSize(const Instruction& instr, uint32_t currentAddr = 0);
    int calculateDirectiveSize(const Directive& dir);
    int calculateExprSize(int tokenIndex);
    uint8_t getOpcode(const std::string& mnemonic, AddressingMode mode);
    uint32_t evaluateExpressionAt(int index);
    void emitExpressionCode(std::vector<uint8_t>& binary, const std::string& target, int tokenIndex);
    void emitMulCode(std::vector<uint8_t>& binary, int width, const std::string& dest, int tokenIndex);
    void emitDivCode(std::vector<uint8_t>& binary, int width, const std::string& dest, int tokenIndex);
};
