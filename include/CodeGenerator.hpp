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
    CodeGenerator(std::ostream& out);
    void generate(TranslationUnit& unit);
    void setSourceInfo(const std::string& filename, const std::vector<std::string>& lines);

    struct VarInfo {
        std::string type;
        int pointerLevel;
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

    void emit(const std::string& line);
    void emitData();
    std::string newLabel();
    ExpressionType getExprType(Expression* expr);
};
