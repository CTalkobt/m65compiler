#pragma once
#include <vector>
#include <string>
#include <map>
#include <deque>
#include <memory>
#include <cstdint>
#include "AssemblerToken.hpp"
#include "AssemblerTypes.hpp"
#include "AssemblerOpcodeDatabase.hpp"

struct Instruction {
    std::string mnemonic;
    AddressingMode mode;
    std::string operand;
    int operandTokenIndex = -1;
    std::string bitBranchTarget;
    std::vector<std::string> callArgs;
    int procParamSize = 0;
    bool forceMode = false;
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
    friend class AssemblerOptimizer;
    friend class AssemblerSimulatedOps;
    friend class AssemblerGenerator;
    AssemblerParser(const std::vector<AssemblerToken>& tokens);
    AssemblerParser(const std::vector<AssemblerToken>& tokens, const std::map<std::string, uint32_t>& predefinedSymbols);
    void pass1();
    bool optimize();
    std::vector<uint8_t> pass2();
    uint32_t getZPStart() const;
    uint32_t getPC() const { return pc; }
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

    struct Segment {
        std::string name;
        uint32_t pc = 0;
        uint32_t startAddress = 0xFFFFFFFF;
        bool hasOrg = false;
    };
    std::map<std::string, std::shared_ptr<Segment>> segments;
    std::shared_ptr<Segment> currentSegment;
    std::vector<std::shared_ptr<Segment>> segmentOrder;
    std::vector<std::string> segmentStack;
    std::vector<std::string> requestedSegmentOrder;

    struct Statement {
        enum Type { NONE, INSTRUCTION, DIRECTIVE, EXPR, BASIC_UPSTART, MUL, DIV, STACK_INC, STACK_DEC, STACK_INC8, STACK_DEC8,
                    ADD16, SUB16, AND16, ORA16, EOR16, CMP16, LDW, STW, SWAP, ZERO,
                    NEG16, NOT16, CHKZERO8, CHKZERO16, CHKNONZERO8, CHKNONZERO16, BRANCH16, SELECT,
                    PTRSTACK, PTRDEREF, LDWF, STWF, INCF, DECF, PHW_STACK, ASW, ROW, ASR16, LSL16, LSR16, ROL16, ROR16, ABS16,
                    ADDS16, SUBS16, CMP_S16, NEG_S16, ABS_S16, ASR_S16, LSL_S16, LSR_S16, ROL_S16, ROR_S16, SXT8,
                    LDX_STACK, LDY_STACK, LDZ_STACK,
                    STX_STACK, STY_STACK, STZ_STACK,
                    LDAX, LDAY, LDAZ, STAX, STAY, STAZ, FILL, COPY, PUSH, POP } type = NONE;
        Instruction instr;
        Directive dir;
        std::string label;
        uint32_t address = 0;
        int size = 0;
        int line = 0;
        std::string scopePrefix;
        std::string segmentName;
        std::shared_ptr<ProcContext> procCtx;

        // EXPR specific
        std::string exprTarget;
        int exprTokenIndex = -1;

        // BASIC_UPSTART specific
        int basicUpstartTokenIndex = -1;

        // MUL specific
        int mulWidth = 8;

        bool deleted = false;

        bool isSimulatedOp() const {
            switch (type) {
                case MUL: case DIV: case STACK_INC: case STACK_DEC:
                case STACK_INC8: case STACK_DEC8: case ADD16: case SUB16:
                case AND16: case ORA16: case EOR16: case CMP16: case LDW:
                case STW: case SWAP: case ZERO: case NEG16: case NOT16:
                case ABS16: case CHKZERO8: case CHKZERO16:
                case ADDS16: case SUBS16: case CMP_S16: case NEG_S16: case ABS_S16:
                case ASR_S16: case LSL_S16: case LSR_S16: case ROL_S16: case ROR_S16: case SXT8:
                case LDX_STACK: case LDY_STACK: case LDZ_STACK:
                case STX_STACK: case STY_STACK: case STZ_STACK:
                case CHKNONZERO8: case CHKNONZERO16: case BRANCH16: case SELECT:
                case PTRSTACK: case PTRDEREF: case LDWF: case STWF: case INCF:
                case DECF: case PHW_STACK: case ASW: case ROW: case ASR16:
                case LSL16: case LSR16: case ROL16: case ROR16:
                case LDAX: case LDAY: case LDAZ: case STAX: case STAY: case STAZ:
                case FILL: case COPY: case PUSH: case POP:
                    return true;
                default:
                    return false;
            }
        }

        bool is16BitOp() const {
            switch (type) {
                case ADD16: case SUB16: case AND16: case ORA16: case EOR16:
                case CMP16: case NEG16: case NOT16: case ABS16:
                case ADDS16: case SUBS16: case CMP_S16: case NEG_S16: case ABS_S16:
                case ASR16: case LSL16: case LSR16: case ROL16: case ROR16:
                case ASR_S16: case LSL_S16: case LSR_S16: case ROL_S16: case ROR_S16:
                case LDW: case STW: case LDAX: case LDAY: case LDAZ:
                case STAX: case STAY: case STAZ:
                    return true;
                default:
                    return false;
            }
        }
    };
    std::deque<std::unique_ptr<Statement>> statements;

    void switchSegment(const std::string& name);

    const AssemblerToken& peek() const;
    const AssemblerToken& advance();
    bool match(AssemblerTokenType type);
    const AssemblerToken& expect(AssemblerTokenType type, const std::string& message);

    int calculateInstructionSize(const Instruction& instr, uint32_t currentAddr = 0, const std::string& scopePrefix = "");
    int calculateDirectiveSize(const Directive& dir, uint32_t currentAddr = 0);
    int calculateExprSize(int tokenIndex, const std::string& scopePrefix = "");
    uint32_t evaluateExpressionAt(int index, const std::string& scopePrefix = "");
    void emitExpressionCode(std::vector<uint8_t>& binary, const std::string& target, int tokenIndex, const std::string& scopePrefix = "");
    void emitMulCode(std::vector<uint8_t>& binary, int width, const std::string& dest, int tokenIndex, const std::string& scopePrefix = "");
    void emitDivCode(std::vector<uint8_t>& binary, int width, const std::string& dest, int tokenIndex, const std::string& scopePrefix = "");
    void emitStackIncDecCode(std::vector<uint8_t>& binary, bool isInc, int tokenIndex, const std::string& scopePrefix = "");
    void emitStackIncDec8Code(std::vector<uint8_t>& binary, bool isInc, int tokenIndex, const std::string& scopePrefix = "");
    void emitAddSub16Code(std::vector<uint8_t>& binary, bool isAdd, const std::string& dest, int tokenIndex, const std::string& scopePrefix = "");
    void emitBitwise16Code(std::vector<uint8_t>& binary, const std::string& mnemonic, const std::string& dest, int tokenIndex, const std::string& scopePrefix = "");
    void emitCMP16Code(std::vector<uint8_t>& binary, const std::string& src1, int tokenIndex, const std::string& scopePrefix = "");
    void emitCMP_S16Code(std::vector<uint8_t>& binary, const std::string& src1, int tokenIndex, const std::string& scopePrefix = "");
    void emitLDWCode(std::vector<uint8_t>& binary, const std::string& dest, int tokenIndex, const std::string& scopePrefix = "", bool forceStack = false);
    void emitSTWCode(std::vector<uint8_t>& binary, const std::string& src, int tokenIndex, const std::string& scopePrefix = "", bool forceStack = false);
    void emitLDX_StackCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix = "");
    void emitLDY_StackCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix = "");
    void emitLDZ_StackCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix = "");
    void emitSTX_StackCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix = "");
    void emitSTY_StackCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix = "");
    void emitSTZ_StackCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix = "");
    void emitSwapCode(std::vector<uint8_t>& binary, const std::string& r1, int tokenIndex, const std::string& scopePrefix = "");
    void emitZeroCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix = "");
    void emitNegNot16Code(std::vector<uint8_t>& binary, bool isNeg, const std::string& operand, int tokenIndex, const std::string& scopePrefix = "");
    void emitABS16Code(std::vector<uint8_t>& binary, const std::string& dest, int tokenIndex, const std::string& scopePrefix = "");
    void emitChkZeroCode(std::vector<uint8_t>& binary, bool is16, bool isInverse, int tokenIndex, const std::string& scopePrefix = "");
    void emitBranch16Code(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix = "");
    void emitSelectCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix = "");
    void emitPtrStackCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix = "");
    void emitPtrDerefCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix = "");
    void emitFillCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix = "", bool forceStack = false);
    void emitMoveCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix = "", bool forceStack = false);
    void emitFlatMemoryCode(std::vector<uint8_t>& binary, const std::string& mnemonic, int tokenIndex, const std::string& scopePrefix = "");
    void emitPHWStackCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix = "");
    void emitASWCode(std::vector<uint8_t>& binary, const std::string& dest, int tokenIndex, const std::string& scopePrefix = "");
    void emitROWCode(std::vector<uint8_t>& binary, const std::string& dest, int tokenIndex, const std::string& scopePrefix = "");
    void emitLSL16Code(std::vector<uint8_t>& binary, const std::string& dest, int tokenIndex, const std::string& scopePrefix = "");
    void emitLSR16Code(std::vector<uint8_t>& binary, const std::string& dest, int tokenIndex, const std::string& scopePrefix = "");
    void emitROL16Code(std::vector<uint8_t>& binary, const std::string& dest, int tokenIndex, const std::string& scopePrefix = "");
    void emitROR16Code(std::vector<uint8_t>& binary, const std::string& dest, int tokenIndex, const std::string& scopePrefix = "");
    void emitASR16Code(std::vector<uint8_t>& binary, const std::string& dest, int tokenIndex, const std::string& scopePrefix = "");
    void emitSXT8Code(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix = "");
    void emitPushPopCode(std::vector<uint8_t>& binary, bool isPush, int tokenIndex, const std::string& scopePrefix = "");
    bool isStackRelativeOperand(int tokenIndex, uint32_t& offset, const std::string& scopePrefix);
};
