#pragma once
#include <string>
#include <vector>
#include <memory>

class ASTVisitor;

class ASTNode {
public:
    int line = 0;
    int column = 0;
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

struct GenericAssociation {
    std::string typeName;
    int pointerLevel = 0;
    std::unique_ptr<Expression> result;
    bool isDefault = false;
};

class GenericSelection : public Expression {
public:
    std::unique_ptr<Expression> control;
    std::vector<GenericAssociation> associations;
    GenericSelection(std::unique_ptr<Expression> c) : control(std::move(c)) {}
    void accept(ASTVisitor& visitor) override;
};

class ArrayAccess : public Expression {
public:
    std::unique_ptr<Expression> arrayExpr;
    std::unique_ptr<Expression> indexExpr;
    ArrayAccess(std::unique_ptr<Expression> a, std::unique_ptr<Expression> i)
        : arrayExpr(std::move(a)), indexExpr(std::move(i)) {}
    void accept(ASTVisitor& visitor) override;
};

class ConditionalExpression : public Expression {
public:
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Expression> thenExpr;
    std::unique_ptr<Expression> elseExpr;
    ConditionalExpression(std::unique_ptr<Expression> c, std::unique_ptr<Expression> t, std::unique_ptr<Expression> e)
        : condition(std::move(c)), thenExpr(std::move(t)), elseExpr(std::move(e)) {}
    void accept(ASTVisitor& visitor) override;
};

class AlignofExpression : public Expression {
public:
    std::string typeName;
    int pointerLevel;
    AlignofExpression(const std::string& t, int p = 0) : typeName(t), pointerLevel(p) {}
    void accept(ASTVisitor& visitor) override;
};

class VariableDeclaration : public Statement {
public:
    std::string type;
    int pointerLevel;
    std::string name;
    bool isVolatile = false;
    bool isGlobal = false;
    int alignment = 0;
    std::unique_ptr<Expression> alignmentExpr;
    std::unique_ptr<Expression> initializer;
    int arraySize = -1; // -1 means not an array
    VariableDeclaration(const std::string& t, const std::string& n, int p = 0) : type(t), pointerLevel(p), name(n) {}
    void accept(ASTVisitor& visitor) override;
};

class ReturnStatement : public Statement {
public:
    std::unique_ptr<Expression> expression;
    ReturnStatement(std::unique_ptr<Expression> e) : expression(std::move(e)) {}
    void accept(ASTVisitor& visitor) override;
};

class BreakStatement : public Statement {
public:
    void accept(ASTVisitor& visitor) override;
};

class ContinueStatement : public Statement {
public:
    void accept(ASTVisitor& visitor) override;
};

class SwitchContinueStatement : public Statement {
public:
    std::unique_ptr<Expression> target; // nullptr means 'default'
    SwitchContinueStatement(std::unique_ptr<Expression> t) : target(std::move(t)) {}
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
    ForStatement(std::unique_ptr<Statement> init, std::unique_ptr<Expression> cond, std::unique_ptr<Expression> inc, std::unique_ptr<Statement> body)
        : initializer(std::move(init)), condition(std::move(cond)), increment(std::move(inc)), body(std::move(body)) {}
    void accept(ASTVisitor& visitor) override;
};

class SwitchStatement : public Statement {
public:
    std::unique_ptr<Expression> expression;
    std::unique_ptr<Statement> body;
    SwitchStatement(std::unique_ptr<Expression> expr, std::unique_ptr<Statement> body)
        : expression(std::move(expr)), body(std::move(body)) {}
    void accept(ASTVisitor& visitor) override;
};

class CaseStatement : public Statement {
public:
    std::unique_ptr<Expression> value;
    std::string label;
    CaseStatement(std::unique_ptr<Expression> val) : value(std::move(val)) {}
    void accept(ASTVisitor& visitor) override;
};

class DefaultStatement : public Statement {
public:
    std::string label;
    void accept(ASTVisitor& visitor) override;
};

class AsmStatement : public Statement {

public:
    std::string code;
    AsmStatement(const std::string& c) : code(c) {}
    void accept(ASTVisitor& visitor) override;
};

class StaticAssert : public Statement {
public:
    std::unique_ptr<Expression> condition;
    std::string message;
    StaticAssert(std::unique_ptr<Expression> c, const std::string& m)
        : condition(std::move(c)), message(m) {}
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
    int alignment = 0;
    std::unique_ptr<Expression> alignmentExpr;
    bool isAnonymous = false;
    int arraySize = -1;
};

class StructDefinition : public Statement {
public:
    std::string name;
    bool isUnion = false;
    std::vector<StructMember> members;
    StructDefinition(const std::string& n, bool isUnion = false) : name(n), isUnion(isUnion) {}
    void accept(ASTVisitor& visitor) override;
};

struct Parameter {
    std::string type;
    int pointerLevel;
    std::string name;
    bool isVolatile = false; // New: volatile qualifier
};

class FunctionDeclaration : public Statement {
public:
    std::string name;
    std::string returnType;
    std::vector<Parameter> parameters;
    std::unique_ptr<CompoundStatement> body;
    bool isNoreturn = false;
    FunctionDeclaration(const std::string& n, const std::string& rt) : name(n), returnType(rt) {}
    void accept(ASTVisitor& visitor) override;
};


class TranslationUnit : public ASTNode {
public:
    std::vector<std::unique_ptr<Statement>> topLevelDecls;
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
    virtual void visit(ConditionalExpression& node) = 0;
    virtual void visit(GenericSelection& node) = 0;
    virtual void visit(ArrayAccess& node) = 0;
    virtual void visit(FunctionCall& node) = 0;
    virtual void visit(MemberAccess& node) = 0;
    virtual void visit(AlignofExpression& node) = 0;
    virtual void visit(VariableDeclaration& node) = 0;
    virtual void visit(ReturnStatement& node) = 0;
    virtual void visit(BreakStatement& node) = 0;
    virtual void visit(ContinueStatement& node) = 0;
    virtual void visit(SwitchContinueStatement& node) = 0;
    virtual void visit(ExpressionStatement& node) = 0;
    virtual void visit(IfStatement& node) = 0;
    virtual void visit(WhileStatement& node) = 0;
    virtual void visit(DoWhileStatement& node) = 0;
    virtual void visit(ForStatement& node) = 0;
    virtual void visit(SwitchStatement& node) = 0;
    virtual void visit(CaseStatement& node) = 0;
    virtual void visit(DefaultStatement& node) = 0;
    virtual void visit(AsmStatement& node) = 0;
    virtual void visit(StaticAssert& node) = 0;
    virtual void visit(StructDefinition& node) = 0;
    virtual void visit(CompoundStatement& node) = 0;
    virtual void visit(FunctionDeclaration& node) = 0;
    virtual void visit(TranslationUnit& node) = 0;
};
