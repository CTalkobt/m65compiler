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

    template<typename T>
    std::unique_ptr<T> setPos(std::unique_ptr<T> node, const Token& token) {
        node->line = token.line;
        node->column = token.column;
        return node;
    }

    std::unique_ptr<FunctionDeclaration> parseFunctionDeclaration();
    std::unique_ptr<CompoundStatement> parseCompoundStatement();
    std::unique_ptr<Statement> parseStatement();
    std::unique_ptr<Statement> parseVariableDeclaration(bool isVolatile);
    std::unique_ptr<StaticAssert> parseStaticAssert();
    std::unique_ptr<StructDefinition> parseStructDefinition(bool isUnion = false);
    std::unique_ptr<Expression> parseExpression();
    std::unique_ptr<Expression> parseConditional();
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

    int anonymousAggregateCount = 0;
    std::vector<std::unique_ptr<StructDefinition>> pendingDefinitions;
    void flushPending(TranslationUnit& unit);
};
