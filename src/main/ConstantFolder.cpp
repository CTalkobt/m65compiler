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

void ConstantFolder::visit(TranslationUnit& node) {
    for (auto& decl : node.topLevelDecls) {
        decl->accept(*this);
    }
}
