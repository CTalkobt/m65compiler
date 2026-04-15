#include "AST.hpp"

void IntegerLiteral::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void StringLiteral::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void VariableReference::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void Assignment::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void BinaryOperation::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void FunctionCall::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void VariableDeclaration::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void ReturnStatement::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void ExpressionStatement::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void IfStatement::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void WhileStatement::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void DoWhileStatement::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void ForStatement::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void AsmStatement::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void CompoundStatement::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void FunctionDeclaration::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void TranslationUnit::accept(ASTVisitor& visitor) { visitor.visit(*this); }
