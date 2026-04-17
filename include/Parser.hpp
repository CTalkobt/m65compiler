#pragma once
#include <vector>
#include <memory>
#include "Token.hpp"
#include "AST.hpp"

class Parser {
public:
    Parser(const std::vector<Token>& tokens);
    std::unique_ptr<TranslationUnit> parse();

private:
    std::vector<Token> tokens;
    size_t pos;

    const Token& peek() const;
    const Token& advance();
    bool match(TokenType type);
    const Token& expect(TokenType type, const std::string& message);

    std::unique_ptr<FunctionDeclaration> parseFunctionDeclaration();
    std::unique_ptr<CompoundStatement> parseCompoundStatement();
    std::unique_ptr<Statement> parseStatement();
    std::unique_ptr<StructDefinition> parseStructDefinition();
    std::unique_ptr<Expression> parseExpression();
    std::unique_ptr<Expression> parseLogicalOr();
    std::unique_ptr<Expression> parseLogicalAnd();
    std::unique_ptr<Expression> parseBitwiseOr();
    std::unique_ptr<Expression> parseBitwiseXor();
    std::unique_ptr<Expression> parseBitwiseAnd();
    std::unique_ptr<Expression> parseEquality();
    std::unique_ptr<Expression> parseRelational();
    std::unique_ptr<Expression> parseShift();
    std::unique_ptr<Expression> parseAdditive();
    std::unique_ptr<Expression> parseMultiplicative();
    std::unique_ptr<Expression> parseUnary();
    std::unique_ptr<Expression> parsePrimary();
};
