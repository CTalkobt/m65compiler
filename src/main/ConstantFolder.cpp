#include "ConstantFolder.hpp"
#include <algorithm>

ConstantFolder::ConstantFolder() {
}

std::unique_ptr<ASTNode> ConstantFolder::fold(std::unique_ptr<ASTNode> node) {
    if (!node) return nullptr;

    lastExpr = nullptr;
    lastStmt = nullptr;

    node->accept(*this);

    if (lastExpr) return std::move(lastExpr);
    if (lastStmt) return std::move(lastStmt);

    return std::move(node);
}

std::unique_ptr<TranslationUnit> ConstantFolder::foldTranslationUnit(std::unique_ptr<TranslationUnit> unit) {
    if (!unit) return nullptr;

    auto newUnit = std::make_unique<TranslationUnit>();
    newUnit->line = unit->line;
    newUnit->column = unit->column;

    auto oldConstants = knownConstants;

    for (auto& decl : unit->topLevelDecls) {
        auto foldedDecl = fold(std::move(decl));
        if (foldedDecl) {
            newUnit->topLevelDecls.push_back(std::move(foldedDecl));
        }
    }
    knownConstants = std::move(oldConstants);
    return newUnit;
}

void ConstantFolder::visit(WhileStatement& node) {
    knownConstants.clear(); // Disable constant propagation inside/across loops
    auto condition = fold(std::move(node.condition));
    auto* lit = dynamic_cast<IntegerLiteral*>(condition.get());
    if (lit && lit->value == 0) {
        lastStmt = nullptr;
    } else {
        lastStmt = copyPos(std::make_unique<WhileStatement>(std::move(condition), fold(std::move(node.body))), node);
    }
}

void ConstantFolder::visit(DoWhileStatement& node) {
    knownConstants.clear();
    lastStmt = copyPos(std::make_unique<DoWhileStatement>(fold(std::move(node.body)), fold(std::move(node.condition))), node);
}

void ConstantFolder::visit(ForStatement& node) {
    auto initializer = fold(std::move(node.initializer));
    knownConstants.clear(); // Clear before condition/body/increment
    auto condition = fold(std::move(node.condition));
    auto increment = fold(std::move(node.increment));
    auto body = fold(std::move(node.body));
    
    auto* lit = dynamic_cast<IntegerLiteral*>(condition.get());
    if (lit && lit->value == 0) {
        lastStmt = std::move(initializer);
    } else {
        lastStmt = copyPos(std::make_unique<ForStatement>(std::move(initializer), std::move(condition), std::move(increment), std::move(body)), node);
    }
}

void ConstantFolder::visit(TranslationUnit& node) {
    for (auto& decl : node.topLevelDecls) {
        decl->accept(*this);
    }
}

void ConstantFolder::visit(AlignofExpression& node) {
    int alignment = 1;
    if (node.pointerLevel > 0 || node.typeName == "int") alignment = 2;
    else if (node.typeName == "char") alignment = 1;
    else if (node.typeName.substr(0, 7) == "struct ") {
        // Struct alignment would need full struct info, but we don't have it here easily
        // Default to 1 for now, or 2 if we assume they might contain ints.
        // Actually, let's keep it as 1 for now as a safe default for char-only structs.
        alignment = 1; 
    }
    lastExpr = copyPos(std::make_unique<IntegerLiteral>(alignment), node);
}

void ConstantFolder::visit(StaticAssert& node) {
    auto condition = fold(std::move(node.condition));
    auto* lit = dynamic_cast<IntegerLiteral*>(condition.get());
    if (lit) {
        if (lit->value == 0) {
            throw std::runtime_error("Static assertion failed at " + std::to_string(node.line) + ":" + std::to_string(node.column) + ": " + node.message);
        }
    } else {
        // If it's not a literal after folding, it might not be a constant expression
        // For C11 _Static_assert, it must be a constant expression.
        // We could either warn or error here. Standards say it must be a constant expression.
        throw std::runtime_error("Static assertion condition is not a constant expression at " + std::to_string(node.line) + ":" + std::to_string(node.column));
    }
    // Static assert doesn't produce any code, so we don't set lastStmt to it.
    // However, the folder expects a statement if it's folding one.
    // If we return nullptr, it might be removed from the AST, which is fine for StaticAssert.
    lastStmt = nullptr;
}

void ConstantFolder::visit(GenericSelection& node) {
    auto control = fold(std::move(node.control));
    // ConstantFolder doesn't have full type info, so we usually can't resolve _Generic here
    // unless the controlling expression was already folded to a literal with a clear type.
    // But even then, cc45 literals don't have explicit types.
    // So we just fold all branches and keep the _Generic node.
    auto gs = copyPos(std::make_unique<GenericSelection>(std::move(control)), node);
    for (auto& assoc : node.associations) {
        GenericAssociation newAssoc;
        newAssoc.typeName = assoc.typeName;
        newAssoc.pointerLevel = assoc.pointerLevel;
        newAssoc.isDefault = assoc.isDefault;
        newAssoc.result = fold(std::move(assoc.result));
        gs->associations.push_back(std::move(newAssoc));
    }
    lastExpr = std::move(gs);
}

void ConstantFolder::visit(SizeofExpression& node) {
    if (node.isType) {
        int size = CodeGenerator::getTypeSize(node.typeName, node.pointerLevel, -1, structs);
        lastExpr = copyPos(std::make_unique<IntegerLiteral>(size), node);
    } else {
        // We need to resolve the type of the expression to get its size.
        // ConstantFolder doesn't have a full type-checking pass yet, 
        // so we might need to defer this to CodeGenerator or implement minimal lookup.
        // For now, if it's a VariableReference we might know it.
        if (auto* ref = dynamic_cast<VariableReference*>(node.expression.get())) {
             // We'd need a map of variable types in ConstantFolder too.
             // Given the current architecture, deferring 'sizeof expr' to CodeGenerator is safer
             // unless we add variableType tracking to ConstantFolder.
             lastExpr = copyPos(std::make_unique<SizeofExpression>(fold(std::move(node.expression))), node);
        } else {
             lastExpr = copyPos(std::make_unique<SizeofExpression>(fold(std::move(node.expression))), node);
        }
    }
}
