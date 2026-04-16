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

    struct VarInfo {
        std::string type;
        bool isPointer;
    };
    std::map<std::string, VarInfo> variableTypes;

    void visit(IntegerLiteral& node) override;
    void visit(StringLiteral& node) override;
    void visit(VariableReference& node) override;
    void visit(Assignment& node) override;
    void visit(BinaryOperation& node) override;
    void visit(UnaryOperation& node) override;
    void visit(FunctionCall& node) override;
    void visit(VariableDeclaration& node) override;
    void visit(ReturnStatement& node) override;
    void visit(ExpressionStatement& node) override;
    void visit(IfStatement& node) override;
    void visit(WhileStatement& node) override;
    void visit(DoWhileStatement& node) override;
    void visit(ForStatement& node) override;
    void visit(AsmStatement& node) override;
    void visit(CompoundStatement& node) override;
    void visit(FunctionDeclaration& node) override;
    void visit(TranslationUnit& node) override;

private:
    std::ostream& out;
    std::unique_ptr<M65Emitter> emitter;
    int stringCount = 0;
    int labelCount = 0;
    std::map<std::string, std::string> stringPool;
    std::vector<std::string> currentVars;

    void emit(const std::string& line);
    void emitData();
    std::string newLabel();
};
