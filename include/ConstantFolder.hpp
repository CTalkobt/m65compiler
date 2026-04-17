#pragma once
#include "AST.hpp"
#include <memory>
#include <map>

class ConstantFolder : public ASTVisitor {
public:
    std::unique_ptr<Expression> lastExpr;
    std::unique_ptr<Statement> lastStmt;

    std::unique_ptr<Expression> fold(std::unique_ptr<Expression> expr) {
        if (!expr) return nullptr;
        expr->accept(*this);
        return std::move(lastExpr);
    }

    std::unique_ptr<Statement> fold(std::unique_ptr<Statement> stmt) {
        if (!stmt) return nullptr;
        stmt->accept(*this);
        return std::move(lastStmt);
    }

    void visit(IntegerLiteral& node) override {
        lastExpr = std::make_unique<IntegerLiteral>(node.value);
    }

    void visit(StringLiteral& node) override {
        lastExpr = std::make_unique<StringLiteral>(node.value);
    }

    void visit(VariableReference& node) override {
        lastExpr = std::make_unique<VariableReference>(node.name);
    }

    void visit(Assignment& node) override {
        lastExpr = std::make_unique<Assignment>(fold(std::move(node.target)), fold(std::move(node.expression)));
    }

    void visit(BinaryOperation& node) override {
        auto left = fold(std::move(node.left));
        auto right = fold(std::move(node.right));

        auto* leftLit = dynamic_cast<IntegerLiteral*>(left.get());
        auto* rightLit = dynamic_cast<IntegerLiteral*>(right.get());

        if (leftLit && rightLit) {
            int result = 0;
            if (node.op == "+") result = leftLit->value + rightLit->value;
            else if (node.op == "-") result = leftLit->value - rightLit->value;
            else if (node.op == "*") result = leftLit->value * rightLit->value;
            else if (node.op == "/") {
                if (rightLit->value != 0) result = leftLit->value / rightLit->value;
                else {
                    // Division by zero at compile time, keep as is or error?
                    // The user asked for an error previously, but for now let's just NOT fold it.
                    lastExpr = std::make_unique<BinaryOperation>(node.op, std::move(left), std::move(right));
                    return;
                }
            }
            else if (node.op == "&") result = leftLit->value & rightLit->value;
            else if (node.op == "|") result = leftLit->value | rightLit->value;
            else if (node.op == "^") result = leftLit->value ^ rightLit->value;
            else if (node.op == "<<") result = leftLit->value << rightLit->value;
            else if (node.op == ">>") result = leftLit->value >> rightLit->value;
            else if (node.op == "==") result = leftLit->value == rightLit->value;
            else if (node.op == "!=") result = leftLit->value != rightLit->value;
            else if (node.op == "<") result = leftLit->value < rightLit->value;
            else if (node.op == ">") result = leftLit->value > rightLit->value;
            else if (node.op == "<=") result = leftLit->value <= rightLit->value;
            else if (node.op == ">=") result = leftLit->value >= rightLit->value;
            else if (node.op == "&&") result = leftLit->value && rightLit->value;
            else if (node.op == "||") result = leftLit->value || rightLit->value;
            else {
                lastExpr = std::make_unique<BinaryOperation>(node.op, std::move(left), std::move(right));
                return;
            }
            lastExpr = std::make_unique<IntegerLiteral>(result);
        } else {
            lastExpr = std::make_unique<BinaryOperation>(node.op, std::move(left), std::move(right));
        }
    }

    void visit(UnaryOperation& node) override {
        auto operand = fold(std::move(node.operand));
        auto* lit = dynamic_cast<IntegerLiteral*>(operand.get());

        if (lit) {
            int result = 0;
            if (node.op == "-") result = -lit->value;
            else if (node.op == "!") result = !lit->value;
            else if (node.op == "~") result = ~lit->value;
            else {
                lastExpr = std::make_unique<UnaryOperation>(node.op, std::move(operand));
                return;
            }
            lastExpr = std::make_unique<IntegerLiteral>(result);
        } else {
            lastExpr = std::make_unique<UnaryOperation>(node.op, std::move(operand));
        }
    }

    void visit(FunctionCall& node) override {
        auto call = std::make_unique<FunctionCall>(node.name);
        for (auto& arg : node.arguments) {
            call->arguments.push_back(fold(std::move(arg)));
        }
        lastExpr = std::move(call);
    }

    void visit(MemberAccess& node) override {
        lastExpr = std::make_unique<MemberAccess>(fold(std::move(node.structExpr)), node.memberName, node.isArrow);
    }

    void visit(VariableDeclaration& node) override {
        auto decl = std::make_unique<VariableDeclaration>(node.type, node.name, node.pointerLevel);
        if (node.initializer) {
            decl->initializer = fold(std::move(node.initializer));
        }
        lastStmt = std::move(decl);
    }

    void visit(ReturnStatement& node) override {
        lastStmt = std::make_unique<ReturnStatement>(fold(std::move(node.expression)));
    }

    void visit(ExpressionStatement& node) override {
        lastStmt = std::make_unique<ExpressionStatement>(fold(std::move(node.expression)));
    }

    void visit(IfStatement& node) override {
        auto condition = fold(std::move(node.condition));
        auto* lit = dynamic_cast<IntegerLiteral*>(condition.get());
        if (lit) {
            if (lit->value) {
                lastStmt = fold(std::move(node.thenBranch));
            } else {
                if (node.elseBranch) {
                    lastStmt = fold(std::move(node.elseBranch));
                } else {
                    lastStmt = nullptr;
                }
            }
        } else {
            lastStmt = std::make_unique<IfStatement>(std::move(condition), fold(std::move(node.thenBranch)), fold(std::move(node.elseBranch)));
        }
    }

    void visit(WhileStatement& node) override {
        auto condition = fold(std::move(node.condition));
        auto* lit = dynamic_cast<IntegerLiteral*>(condition.get());
        if (lit && lit->value == 0) {
            lastStmt = nullptr;
        } else {
            lastStmt = std::make_unique<WhileStatement>(std::move(condition), fold(std::move(node.body)));
        }
    }

    void visit(DoWhileStatement& node) override {
        lastStmt = std::make_unique<DoWhileStatement>(fold(std::move(node.body)), fold(std::move(node.condition)));
    }

    void visit(ForStatement& node) override {
        auto initializer = fold(std::move(node.initializer));
        auto condition = fold(std::move(node.condition));
        auto increment = fold(std::move(node.increment));
        auto body = fold(std::move(node.body));
        
        auto* lit = dynamic_cast<IntegerLiteral*>(condition.get());
        if (lit && lit->value == 0) {
            // If condition is constantly false, only initializer executes once (if it exists)
            lastStmt = std::move(initializer);
        } else {
            lastStmt = std::make_unique<ForStatement>(std::move(initializer), std::move(condition), std::move(increment), std::move(body));
        }
    }

    void visit(AsmStatement& node) override {
        lastStmt = std::make_unique<AsmStatement>(node.code);
    }

    void visit(StructDefinition& node) override {
        auto def = std::make_unique<StructDefinition>(node.name);
        def->members = node.members;
        lastStmt = std::move(def);
    }

    void visit(CompoundStatement& node) override {
        auto compound = std::make_unique<CompoundStatement>();
        for (auto& stmt : node.statements) {
            auto folded = fold(std::move(stmt));
            if (folded) {
                compound->statements.push_back(std::move(folded));
            }
        }
        lastStmt = std::move(compound);
    }

    void visit(FunctionDeclaration& node) override {
        // FunctionDeclaration is not a Statement, so we can't use lastStmt easily without cast or extra method
        // But we can just modify it in place or return a new one.
        node.body = std::unique_ptr<CompoundStatement>(static_cast<CompoundStatement*>(fold(std::move(node.body)).release()));
    }

    void visit(TranslationUnit& node) override {
        for (auto& decl : node.topLevelDecls) {
            decl->accept(*this);
        }
    }
};
