#pragma once
#include "AST.hpp"
#include "M65Emitter.hpp"
#include <ostream>
#include <vector>
#include <string>
#include <map>
#include <memory>

class CodeGenerator : public ASTVisitor {
public:
    enum class TriState { UNKNOWN, SET, CLEAR };
    enum class FlagSource { NONE, A, X, Y, Z };

    CodeGenerator(std::ostream& out);
    void generate(TranslationUnit& unit);
    void setSourceInfo(const std::string& filename, const std::vector<std::string>& lines);

    struct VarInfo {
        std::string type;
        int pointerLevel;
        bool isVolatile = false; // New: volatile qualifier
    };
    struct ExpressionType {
        std::string type;
        int pointerLevel;
    };
    struct MemberInfo {
        std::string type;
        int pointerLevel;
        int offset;
    };
    struct StructInfo {
        std::string name;
        std::map<std::string, MemberInfo> members;
        int totalSize;
    };
    std::map<std::string, VarInfo> variableTypes;
    std::map<std::string, StructInfo> structs;
    uint32_t zeroPageStart = 0x02;
    uint32_t zeroPageAvail = 9;

    void visit(IntegerLiteral& node) override;
    void visit(StringLiteral& node) override;
    void visit(VariableReference& node) override;
    void visit(Assignment& node) override;
    void visit(BinaryOperation& node) override;
    void visit(UnaryOperation& node) override;
    void visit(FunctionCall& node) override;
    void visit(MemberAccess& node) override;
    void visit(VariableDeclaration& node) override;
    void visit(ReturnStatement& node) override;
    void visit(ExpressionStatement& node) override;
    void visit(IfStatement& node) override;
    void visit(WhileStatement& node) override;
    void visit(DoWhileStatement& node) override;
    void visit(ForStatement& node) override;
    void visit(AsmStatement& node) override;
    void visit(StructDefinition& node) override;
    void visit(CompoundStatement& node) override;
    void visit(FunctionDeclaration& node) override;
    void visit(TranslationUnit& node) override;
    void emitAddress(Expression* expr);
    void embedSource(ASTNode& node);

    // Register tracking
    struct RegState {
        bool known = false;
        bool isVariable = false;
        std::string varName;
        int varOffset = 0;
        uint8_t value = 0;
    };
    RegState regA, regX, regY, regZ;
    void updateRegA(uint8_t val);
    void updateRegX(uint8_t val);
    void updateRegY(uint8_t val);
    void updateRegZ(uint8_t val);
    void updateRegAVar(const std::string& name, int offset);
    void updateRegXVar(const std::string& name, int offset);
    void updateRegYVar(const std::string& name, int offset);
    void updateRegZVar(const std::string& name, int offset);
    void transferRegs(FlagSource dest, FlagSource src);
    void invalidateVar(const std::string& name);
    void invalidateRegs();

    // Flag tracking
    struct ProcessorStatus {
        TriState carry = TriState::UNKNOWN;
        TriState zero = TriState::UNKNOWN;
        TriState negative = TriState::UNKNOWN;
        TriState overflow = TriState::UNKNOWN;
        FlagSource znSource = FlagSource::NONE;
    };
    ProcessorStatus flags;
    void updateFlags(TriState c, TriState z, TriState n, TriState v = TriState::UNKNOWN);
    void updateZNFlags(FlagSource source, TriState z = TriState::UNKNOWN, TriState n = TriState::UNKNOWN);
    void invalidateFlags();

    private:
    std::ostream& out;
    std::unique_ptr<M65Emitter> emitter;
    int stringCount = 0;
    int labelCount = 0;
    std::map<std::string, std::string> stringPool;
    std::vector<std::string> currentVars;
    std::string sourceFilename;
    std::vector<std::string> sourceLines;
    int lastEmbeddedLine = -1;
    bool resultNeeded = true;

    void emit(const std::string& line);
    void emitData();
    void emitJumpIfTrue(Expression* cond, const std::string& labelTrue);
    void emitJumpIfFalse(Expression* cond, const std::string& labelElse);
    std::string newLabel();
    std::string newDontCareLabel();
    ExpressionType getExprType(Expression* expr);
    bool isStruct(const std::string& type);
    std::string resolveVarName(const std::string& name);

    int allocateZP(int size);
    void freeZP(int index, int size);

    struct ZPReg {
        bool inUse = false;
    };
    std::vector<ZPReg> zpRegs;
    FunctionDeclaration* currentFunction = nullptr;
    bool isVariableUsed(const std::string& varName, FunctionDeclaration& func);
};
