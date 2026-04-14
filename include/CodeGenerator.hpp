#pragma once
#include "AST.hpp"
#include <ostream>
#include <vector>
#include <string>
#include <map>

class CodeGenerator : public ASTVisitor {
public:
    CodeGenerator(std::ostream& out);
    void generate(TranslationUnit& unit);

    void visit(IntegerLiteral& node) override;
    void visit(StringLiteral& node) override;
    void visit(VariableReference& node) override;
    void visit(FunctionCall& node) override;
    void visit(ReturnStatement& node) override;
    void visit(ExpressionStatement& node) override;
    void visit(CompoundStatement& node) override;
    void visit(FunctionDeclaration& node) override;
    void visit(TranslationUnit& node) override;

private:
    std::ostream& out;
    int stringCount = 0;
    std::map<std::string, std::string> stringPool;

    void emit(const std::string& line);
    void emitData();
};
