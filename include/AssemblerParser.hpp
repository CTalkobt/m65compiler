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
    BASE_PAGE_X_INDIRECT,
    BASE_PAGE_INDIRECT_Y,
    BASE_PAGE_INDIRECT_Z,
    BASE_PAGE_INDIRECT_SP_Y,
    ABSOLUTE_INDIRECT,
    ABSOLUTE_X_INDIRECT,
    RELATIVE,
    RELATIVE16,
    BASE_PAGE_RELATIVE,
    FLAT_INDIRECT_Z,
    QUAD_Q,
    STACK_RELATIVE,
    STACK_RELATIVE_INDIRECT_Y,
    LINEAR_ABSOLUTE,
    LINEAR_ABSOLUTE_X,
    LINEAR_ABSOLUTE_Y,
    NONE
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
    int operandTokenIndex = -1;
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
    void optimize();
    std::vector<uint8_t> pass2();
    uint32_t getZPStart() const;
    Symbol* resolveSymbol(const std::string& name, const std::string& scopePrefix = "");

private:
    std::vector<AssemblerToken> tokens;
    size_t pos;
    uint32_t pc;
    std::map<std::string, Symbol> symbolTable;
    std::vector<std::string> scopeStack;
    int nextScopeId = 0;

    struct ProcContext {
        std::string name;
        int totalParamSize;
        std::map<std::string, int> localArgs;
    };
    std::shared_ptr<ProcContext> currentProc;
    std::map<uint32_t, std::shared_ptr<ProcContext>> procedures;

    struct Statement {
        enum Type { NONE, INSTRUCTION, DIRECTIVE, EXPR, BASIC_UPSTART, MUL, DIV, STACK_INC, STACK_DEC, STACK_INC8, STACK_DEC8,
                    ADD16, SUB16, AND16, ORA16, EOR16, CPW, LDW, STW, SWAP, ZERO,
                    NEG16, NOT16, CHKZERO8, CHKZERO16, CHKNONZERO8, CHKNONZERO16, BRANCH16, SELECT,
                    PTRSTACK, PTRDEREF, LDWF, STWF, INCF, DECF, PHW_STACK,
                    LDAX, LDAY, LDAZ, STAX, STAY, STAZ, FILL, COPY } type = NONE;
        Instruction instr;
        Directive dir;
        std::string label;
        uint32_t address = 0;
        int size = 0;
        int line = 0;
        std::string scopePrefix;
        std::shared_ptr<ProcContext> procCtx;

        // EXPR specific
        std::string exprTarget;
        int exprTokenIndex = -1;

        // BASIC_UPSTART specific
        int basicUpstartTokenIndex = -1;

        // MUL specific
        int mulWidth = 8;

        bool deleted = false;
    };
    std::deque<std::unique_ptr<Statement>> statements;

    const AssemblerToken& peek() const;
    const AssemblerToken& advance();
    bool match(AssemblerTokenType type);
    const AssemblerToken& expect(AssemblerTokenType type, const std::string& message);

    int calculateInstructionSize(const Instruction& instr, uint32_t currentAddr = 0, const std::string& scopePrefix = "");
    int calculateDirectiveSize(const Directive& dir);
    int calculateExprSize(int tokenIndex, const std::string& scopePrefix = "");
    uint8_t getOpcode(const std::string& mnemonic, AddressingMode mode);
    std::string AddressingModeToString(AddressingMode mode);
    std::vector<AddressingMode> getValidAddressingModes(const std::string& mnemonic);
    uint32_t evaluateExpressionAt(int index, const std::string& scopePrefix = "");
    void emitExpressionCode(std::vector<uint8_t>& binary, const std::string& target, int tokenIndex, const std::string& scopePrefix = "");
    void emitMulCode(std::vector<uint8_t>& binary, int width, const std::string& dest, int tokenIndex, const std::string& scopePrefix = "");
    void emitDivCode(std::vector<uint8_t>& binary, int width, const std::string& dest, int tokenIndex, const std::string& scopePrefix = "");
    void emitStackIncDecCode(std::vector<uint8_t>& binary, bool isInc, int tokenIndex, const std::string& scopePrefix = "");
    void emitStackIncDec8Code(std::vector<uint8_t>& binary, bool isInc, int tokenIndex, const std::string& scopePrefix = "");
    void emitAddSub16Code(std::vector<uint8_t>& binary, bool isAdd, const std::string& dest, int tokenIndex, const std::string& scopePrefix = "");
    void emitBitwise16Code(std::vector<uint8_t>& binary, const std::string& mnemonic, const std::string& dest, int tokenIndex, const std::string& scopePrefix = "");
    void emitCPWCode(std::vector<uint8_t>& binary, const std::string& src1, int tokenIndex, const std::string& scopePrefix = "");
    void emitLDWCode(std::vector<uint8_t>& binary, const std::string& dest, int tokenIndex, const std::string& scopePrefix = "", bool forceStack = false);
    void emitSTWCode(std::vector<uint8_t>& binary, const std::string& src, int tokenIndex, const std::string& scopePrefix = "", bool forceStack = false);
    void emitSwapCode(std::vector<uint8_t>& binary, const std::string& r1, int tokenIndex, const std::string& scopePrefix = "");
    void emitZeroCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix = "");
    void emitNegNot16Code(std::vector<uint8_t>& binary, bool isNeg, const std::string& operand, int tokenIndex, const std::string& scopePrefix = "");
    void emitChkZeroCode(std::vector<uint8_t>& binary, bool is16, bool isInverse, int tokenIndex, const std::string& scopePrefix = "");
    void emitBranch16Code(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix = "");
    void emitSelectCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix = "");
    void emitPtrStackCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix = "");
    void emitPtrDerefCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix = "");
    void emitFillCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix = "", bool forceStack = false);
    void emitMoveCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix = "", bool forceStack = false);
    void emitFlatMemoryCode(std::vector<uint8_t>& binary, const std::string& mnemonic, int tokenIndex, const std::string& scopePrefix = "");
    void emitPHWStackCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix = "");
    bool isStackRelativeOperand(int tokenIndex, uint32_t& offset, const std::string& scopePrefix);
};
