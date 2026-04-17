#pragma once
#include <string>
#include <vector>
#include <memory>

class ASTVisitor;

class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual void accept(ASTVisitor& visitor) = 0;
};

class Expression : public ASTNode {};
class Statement : public ASTNode {};

class IntegerLiteral : public Expression {
public:
    int value;
    IntegerLiteral(int v) : value(v) {}
    void accept(ASTVisitor& visitor) override;
};

class StringLiteral : public Expression {
public:
    std::string value;
    StringLiteral(const std::string& v) : value(v) {}
    void accept(ASTVisitor& visitor) override;
};

class VariableReference : public Expression {
public:
    std::string name;
    VariableReference(const std::string& n) : name(n) {}
    void accept(ASTVisitor& visitor) override;
};

class FunctionCall : public Expression {
public:
    std::string name;
    std::vector<std::unique_ptr<Expression>> arguments;
    FunctionCall(const std::string& n) : name(n) {}
    void accept(ASTVisitor& visitor) override;
};

class Assignment : public Expression {
public:
    std::unique_ptr<Expression> target;
    std::unique_ptr<Expression> expression;
    Assignment(std::unique_ptr<Expression> t, std::unique_ptr<Expression> e) : target(std::move(t)), expression(std::move(e)) {}
    void accept(ASTVisitor& visitor) override;
};

class BinaryOperation : public Expression {
public:
    std::string op;
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;
    BinaryOperation(const std::string& o, std::unique_ptr<Expression> l, std::unique_ptr<Expression> r) 
        : op(o), left(std::move(l)), right(std::move(r)) {}
    void accept(ASTVisitor& visitor) override;
};

class UnaryOperation : public Expression {
public:
    std::string op;
    std::unique_ptr<Expression> operand;
    UnaryOperation(const std::string& o, std::unique_ptr<Expression> opnd)
        : op(o), operand(std::move(opnd)) {}
    void accept(ASTVisitor& visitor) override;
};

class MemberAccess : public Expression {
public:
    std::unique_ptr<Expression> structExpr;
    std::string memberName;
    bool isArrow;
    MemberAccess(std::unique_ptr<Expression> s, const std::string& m, bool arrow) 
        : structExpr(std::move(s)), memberName(m), isArrow(arrow) {}
    void accept(ASTVisitor& visitor) override;
};

class VariableDeclaration : public Statement {
public:
    std::string type;
    int pointerLevel;
    std::string name;
    std::unique_ptr<Expression> initializer;
    VariableDeclaration(const std::string& t, const std::string& n, int p = 0) : type(t), pointerLevel(p), name(n) {}
    void accept(ASTVisitor& visitor) override;
};

class ReturnStatement : public Statement {
public:
    std::unique_ptr<Expression> expression;
    ReturnStatement(std::unique_ptr<Expression> e) : expression(std::move(e)) {}
    void accept(ASTVisitor& visitor) override;
};

class ExpressionStatement : public Statement {
public:
    std::unique_ptr<Expression> expression;
    ExpressionStatement(std::unique_ptr<Expression> e) : expression(std::move(e)) {}
    void accept(ASTVisitor& visitor) override;
};

class IfStatement : public Statement {
public:
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Statement> thenBranch;
    std::unique_ptr<Statement> elseBranch;
    IfStatement(std::unique_ptr<Expression> c, std::unique_ptr<Statement> t, std::unique_ptr<Statement> e = nullptr)
        : condition(std::move(c)), thenBranch(std::move(t)), elseBranch(std::move(e)) {}
    void accept(ASTVisitor& visitor) override;
};

class WhileStatement : public Statement {
public:
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Statement> body;
    WhileStatement(std::unique_ptr<Expression> c, std::unique_ptr<Statement> b)
        : condition(std::move(c)), body(std::move(b)) {}
    void accept(ASTVisitor& visitor) override;
};

class DoWhileStatement : public Statement {
public:
    std::unique_ptr<Statement> body;
    std::unique_ptr<Expression> condition;
    DoWhileStatement(std::unique_ptr<Statement> b, std::unique_ptr<Expression> c)
        : body(std::move(b)), condition(std::move(c)) {}
    void accept(ASTVisitor& visitor) override;
};

class ForStatement : public Statement {
public:
    std::unique_ptr<Statement> initializer;
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Expression> increment;
    std::unique_ptr<Statement> body;
    ForStatement(std::unique_ptr<Statement> i, std::unique_ptr<Expression> c, std::unique_ptr<Expression> inc, std::unique_ptr<Statement> b)
        : initializer(std::move(i)), condition(std::move(c)), increment(std::move(inc)), body(std::move(b)) {}
    void accept(ASTVisitor& visitor) override;
};

class AsmStatement : public Statement {
public:
    std::string code;
    AsmStatement(const std::string& c) : code(c) {}
    void accept(ASTVisitor& visitor) override;
};

class CompoundStatement : public Statement {
public:
    std::vector<std::unique_ptr<Statement>> statements;
    void accept(ASTVisitor& visitor) override;
};

struct StructMember {
    std::string type;
    int pointerLevel;
    std::string name;
};

class StructDefinition : public Statement {
public:
    std::string name;
    std::vector<StructMember> members;
    StructDefinition(const std::string& n) : name(n) {}
    void accept(ASTVisitor& visitor) override;
};

struct Parameter {
    std::string type;
    int pointerLevel;
    std::string name;
};

class FunctionDeclaration : public ASTNode {
public:
    std::string name;
    std::string returnType;
    std::vector<Parameter> parameters;
    std::unique_ptr<CompoundStatement> body;
    FunctionDeclaration(const std::string& n, const std::string& rt) : name(n), returnType(rt) {}
    void accept(ASTVisitor& visitor) override;
};

class TranslationUnit : public ASTNode {
public:
    std::vector<std::unique_ptr<ASTNode>> topLevelDecls;
    void accept(ASTVisitor& visitor) override;
};

class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;
    virtual void visit(IntegerLiteral& node) = 0;
    virtual void visit(StringLiteral& node) = 0;
    virtual void visit(VariableReference& node) = 0;
    virtual void visit(Assignment& node) = 0;
    virtual void visit(BinaryOperation& node) = 0;
    virtual void visit(UnaryOperation& node) = 0;
    virtual void visit(FunctionCall& node) = 0;
    virtual void visit(MemberAccess& node) = 0;
    virtual void visit(VariableDeclaration& node) = 0;
    virtual void visit(ReturnStatement& node) = 0;
    virtual void visit(ExpressionStatement& node) = 0;
    virtual void visit(IfStatement& node) = 0;
    virtual void visit(WhileStatement& node) = 0;
    virtual void visit(DoWhileStatement& node) = 0;
    virtual void visit(ForStatement& node) = 0;
    virtual void visit(AsmStatement& node) = 0;
    virtual void visit(StructDefinition& node) = 0;
    virtual void visit(CompoundStatement& node) = 0;
    virtual void visit(FunctionDeclaration& node) = 0;
    virtual void visit(TranslationUnit& node) = 0;
};
