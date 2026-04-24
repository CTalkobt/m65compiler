#pragma once
#include "AST.hpp"
#include "CodeGenerator.hpp"
#include <memory>
#include <map>
#include <set>
#include <iostream>

class ConstantFolder : public ASTVisitor {
public:
    ConstantFolder();
    std::unique_ptr<Expression> lastExpr;
    std::unique_ptr<Statement> lastStmt;
    std::map<std::string, int> knownConstants;
    std::set<std::string> volatileVars;
    std::map<std::string, std::shared_ptr<CodeGenerator::StructInfo>> structs;

    std::unique_ptr<Expression> fold(std::unique_ptr<Expression> expr) {
        if (!expr) return nullptr;
        expr->accept(*this);
        return std::move(lastExpr);
    }

    std::unique_ptr<Statement> fold(std::unique_ptr<Statement> stmt) {
        if (!stmt) return nullptr;
        // CompoundStatement and FunctionDeclaration are modified in-place.
        // Their visit methods don't set lastStmt.
        if (dynamic_cast<CompoundStatement*>(stmt.get()) || dynamic_cast<FunctionDeclaration*>(stmt.get())) {
            stmt->accept(*this);
            return stmt;
        } else {
            stmt->accept(*this);
            return std::move(lastStmt);
        }
    }

    void visit(IntegerLiteral& node) override {
        lastExpr = copyPos(std::make_unique<IntegerLiteral>(node.value), node);
    }

    void visit(StringLiteral& node) override {
        lastExpr = copyPos(std::make_unique<StringLiteral>(node.value), node);
    }

    void visit(VariableReference& node) override {
        if (knownConstants.count(node.name)) {
            lastExpr = copyPos(std::make_unique<IntegerLiteral>(knownConstants[node.name]), node);
        } else {
            lastExpr = copyPos(std::make_unique<VariableReference>(node.name), node);
        }
    }

    void visit(Assignment& node) override {
        // Don't fold the target! It must remain a VariableReference or MemberAccess.
        auto expression = fold(std::move(node.expression));
        
        if (auto* ref = dynamic_cast<VariableReference*>(node.target.get())) {
            auto* lit = dynamic_cast<IntegerLiteral*>(expression.get());
            if (lit && volatileVars.find(ref->name) == volatileVars.end()) {
                // Only propagate simple assignments that aren't inside potentially complex control flow.
                // For now, we clear constants in loops anyway, but this is a safeguard.
                if (node.op == "=") {
                    knownConstants[ref->name] = lit->value;
                } else {
                    knownConstants.erase(ref->name);
                }
            } else {
                knownConstants.erase(ref->name);
            }
        }
        lastExpr = copyPos(std::make_unique<Assignment>(std::move(node.target), std::move(expression), node.op), node);
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
                    lastExpr = copyPos(std::make_unique<BinaryOperation>(node.op, std::move(left), std::move(right)), node);
                    return;
                }
            }
            else if (node.op == "%") {
                if (rightLit->value != 0) result = leftLit->value % rightLit->value;
                else {
                    lastExpr = copyPos(std::make_unique<BinaryOperation>(node.op, std::move(left), std::move(right)), node);
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
                lastExpr = copyPos(std::make_unique<BinaryOperation>(node.op, std::move(left), std::move(right)), node);
                return;
            }

            lastExpr = copyPos(std::make_unique<IntegerLiteral>(result), node);
            return;
        } else {
            lastExpr = copyPos(std::make_unique<BinaryOperation>(node.op, std::move(left), std::move(right)), node);
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
                lastExpr = copyPos(std::make_unique<UnaryOperation>(node.op, std::move(operand)), node);
                return;
            }
            lastExpr = copyPos(std::make_unique<IntegerLiteral>(result), node);
        } else {
            lastExpr = copyPos(std::make_unique<UnaryOperation>(node.op, std::move(operand)), node);
        }
    }

    void visit(FunctionCall& node) override {
        auto call = copyPos(std::make_unique<FunctionCall>(node.name), node);
        for (auto& arg : node.arguments) {
            call->arguments.push_back(fold(std::move(arg)));
        }
        lastExpr = std::move(call);
    }

    void visit(ConditionalExpression& node) override {
        auto condition = fold(std::move(node.condition));
        auto* lit = dynamic_cast<IntegerLiteral*>(condition.get());
        if (lit) {
            if (lit->value) lastExpr = fold(std::move(node.thenExpr));
            else lastExpr = fold(std::move(node.elseExpr));
        } else {
            lastExpr = copyPos(std::make_unique<ConditionalExpression>(std::move(condition), fold(std::move(node.thenExpr)), fold(std::move(node.elseExpr))), node);
        }
    }

    void visit(GenericSelection& node) override;

    void visit(ArrayAccess& node) override {
        lastExpr = copyPos(std::make_unique<ArrayAccess>(fold(std::move(node.arrayExpr)), fold(std::move(node.indexExpr))), node);
    }

    void visit(MemberAccess& node) override {
        lastExpr = copyPos(std::make_unique<MemberAccess>(fold(std::move(node.structExpr)), node.memberName, node.isArrow), node);
    }

    void visit(AlignofExpression& node) override;
    void visit(SizeofExpression& node) override;

    void visit(VariableDeclaration& node) override {
        auto alignmentExpr = node.alignmentExpr ? fold(std::move(node.alignmentExpr)) : nullptr;
        if (alignmentExpr) {
            if (auto* lit = dynamic_cast<IntegerLiteral*>(alignmentExpr.get())) {
                node.alignment = lit->value;
            }
        }
        auto initializer = node.initializer ? fold(std::move(node.initializer)) : nullptr;
        if (node.isVolatile) {
            volatileVars.insert(node.name);
            knownConstants.erase(node.name);
        } else if (initializer) {
            if (auto* lit = dynamic_cast<IntegerLiteral*>(initializer.get())) {
                knownConstants[node.name] = lit->value;
            } else {
                knownConstants.erase(node.name);
            }
        } else {
            knownConstants.erase(node.name);
        }
        auto decl = copyPos(std::make_unique<VariableDeclaration>(node.type, node.name, node.pointerLevel), node);
        decl->isSigned = node.isSigned;
        decl->initializer = std::move(initializer);
        decl->alignmentExpr = std::move(alignmentExpr);
        decl->alignment = node.alignment;
        decl->isVolatile = node.isVolatile;
        decl->isGlobal = node.isGlobal;
        decl->arraySize = node.arraySize;
        lastStmt = std::move(decl);
    }

    void visit(ReturnStatement& node) override {
        lastStmt = copyPos(std::make_unique<ReturnStatement>(fold(std::move(node.expression))), node);
    }

    void visit(BreakStatement& node) override {
        lastStmt = copyPos(std::make_unique<BreakStatement>(), node);
    }

    void visit(ContinueStatement& node) override {
        lastStmt = copyPos(std::make_unique<ContinueStatement>(), node);
    }

    void visit(SwitchContinueStatement& node) override {
        lastStmt = copyPos(std::make_unique<SwitchContinueStatement>(node.target ? fold(std::move(node.target)) : nullptr), node);
    }

    void visit(GotoStatement& node) override {
        lastStmt = copyPos(std::make_unique<GotoStatement>(node.label), node);
    }

    void visit(LabelledStatement& node) override {
        lastStmt = copyPos(std::make_unique<LabelledStatement>(node.label, fold(std::move(node.statement))), node);
    }

    void visit(ExpressionStatement& node) override {
        lastStmt = copyPos(std::make_unique<ExpressionStatement>(fold(std::move(node.expression))), node);
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
            lastStmt = copyPos(std::make_unique<IfStatement>(std::move(condition), fold(std::move(node.thenBranch)), fold(std::move(node.elseBranch))), node);
        }
    }

    void visit(WhileStatement& node) override;
    void visit(DoWhileStatement& node) override;
    void visit(ForStatement& node) override;

    void visit(SwitchStatement& node) override {
        auto expression = fold(std::move(node.expression));
        auto body = fold(std::move(node.body));
        lastStmt = copyPos(std::make_unique<SwitchStatement>(std::move(expression), std::move(body)), node);
    }

    void visit(CaseStatement& node) override {
        auto value = fold(std::move(node.value));
        auto newNode = std::make_unique<CaseStatement>(std::move(value));
        newNode->label = node.label;
        lastStmt = copyPos(std::move(newNode), node);
    }

    void visit(DefaultStatement& node) override {
        auto newNode = std::make_unique<DefaultStatement>();
        newNode->label = node.label;
        lastStmt = copyPos(std::move(newNode), node);
    }

    void visit(AsmStatement& node) override {
        lastStmt = copyPos(std::make_unique<AsmStatement>(node.code), node);
    }

    void visit(StaticAssert& node) override;

    void visit(EnumDefinition& node) override {
        auto def = copyPos(std::make_unique<EnumDefinition>(node.name), node);
        def->enumerators = node.enumerators;
        lastStmt = std::move(def);
    }

    void visit(StructDefinition& node) override {
        auto def = copyPos(std::make_unique<StructDefinition>(node.name, node.isUnion), node);
        int currentOffset = 0;
        int maxAlignment = 1;

        auto sInfo = std::make_shared<CodeGenerator::StructInfo>();
        sInfo->name = node.name;

        for (auto& m : node.members) {
            auto alignmentExpr = m.alignmentExpr ? fold(std::move(m.alignmentExpr)) : nullptr;
            int alignment = 1;
            if (alignmentExpr) {
                if (auto* lit = dynamic_cast<IntegerLiteral*>(alignmentExpr.get())) {
                    alignment = lit->value;
                }
            }

            int mSize = CodeGenerator::getTypeSize(m.type, m.pointerLevel, m.arraySize, structs);
            int mAlign = alignment; // Simplification, should ideally check type alignment too
            if (mAlign > maxAlignment) maxAlignment = mAlign;

            if (!node.isUnion) {
                if (currentOffset % mAlign != 0) currentOffset += mAlign - (currentOffset % mAlign);
            }

            CodeGenerator::MemberInfo mi = {m.type, m.pointerLevel, m.isSigned, currentOffset, mAlign, m.arraySize};
            sInfo->members[m.name] = mi;

            if (!node.isUnion) currentOffset += mSize;
            else if (mSize > currentOffset) currentOffset = mSize;

            def->members.push_back({m.type, m.pointerLevel, m.isSigned, m.name, alignment, std::move(alignmentExpr), m.isAnonymous, m.arraySize});
        }

        if (currentOffset % maxAlignment != 0) currentOffset += maxAlignment - (currentOffset % maxAlignment);
        sInfo->totalSize = currentOffset;
        sInfo->alignment = maxAlignment;
        structs[node.name] = sInfo;

        lastStmt = std::move(def);
    }

    void visit(CompoundStatement& node) override {
        auto oldConstants = knownConstants;
        auto oldVolatile = volatileVars;
        // Create a new vector to hold the folded statements
        std::vector<std::unique_ptr<Statement>> newStatements;
        for (auto& stmt : node.statements) {
            auto folded = fold(std::move(stmt));
            if (folded) {
                newStatements.push_back(std::move(folded));
            }
        }
        node.statements = std::move(newStatements);
        knownConstants = std::move(oldConstants);
        volatileVars = std::move(oldVolatile);
        // We don't set lastStmt here, as CompoundStatement is modified in place
    }

    void visit(FunctionDeclaration& node) override {
        knownConstants.clear(); // Fresh state for new function
        // The body is a CompoundStatement, which is modified in-place by its visit method.
        // We just need to call accept on it directly, not fold it via the helper.
        node.body->accept(*this);
        // We don't set lastStmt here, as FunctionDeclaration is modified in place
    }

    void visit(TranslationUnit& node) override;

    std::unique_ptr<TranslationUnit> foldTranslationUnit(std::unique_ptr<TranslationUnit> unit);
    std::unique_ptr<ASTNode> fold(std::unique_ptr<ASTNode> node);

private:
    template<typename T, typename U>
    std::unique_ptr<T> copyPos(std::unique_ptr<T> newNode, const U& oldNode) {
        newNode->line = oldNode.line;
        newNode->column = oldNode.column;
        return newNode;
    }
};
