#include "Parser.hpp"
#include <stdexcept>
#include <iostream>

Parser::Parser(const std::vector<Token>& tokens) : tokens(tokens), pos(0) {}

const Token& Parser::peek() const {
    return tokens[pos];
}

const Token& Parser::advance() {
    if (pos < tokens.size()) pos++;
    return tokens[pos - 1];
}

bool Parser::match(TokenType type) {
    if (peek().type == type) {
        advance();
        return true;
    }
    return false;
}

const Token& Parser::expect(TokenType type, const std::string& message) {
    if (peek().type == type) {
        return advance();
    }
    throw std::runtime_error("Error at " + std::to_string(peek().line) + ":" + std::to_string(peek().column) + ": " + message);
}

std::unique_ptr<TranslationUnit> Parser::parse() {
    auto unit = std::make_unique<TranslationUnit>();
    while (peek().type != TokenType::END_OF_FILE) {
        unit->functions.push_back(parseFunctionDeclaration());
    }
    return unit;
}

std::unique_ptr<FunctionDeclaration> Parser::parseFunctionDeclaration() {
    std::string returnType = expect(TokenType::INT, "Expected return type (int)").value;
    std::string name = expect(TokenType::IDENTIFIER, "Expected function name").value;
    
    expect(TokenType::OPEN_PAREN, "Expected '('");
    std::vector<Parameter> params;
    if (peek().type != TokenType::CLOSE_PAREN) {
        do {
            std::string pType = expect(TokenType::INT, "Expected parameter type").value;
            std::string pName = expect(TokenType::IDENTIFIER, "Expected parameter name").value;
            params.push_back({pType, pName});
        } while (match(TokenType::COMMA));
    }
    expect(TokenType::CLOSE_PAREN, "Expected ')'");
    
    auto body = parseCompoundStatement();
    auto func = std::make_unique<FunctionDeclaration>(name, returnType);
    func->parameters = std::move(params);
    func->body = std::move(body);
    return func;
}

std::unique_ptr<CompoundStatement> Parser::parseCompoundStatement() {
    expect(TokenType::OPEN_BRACE, "Expected '{'");
    auto compound = std::make_unique<CompoundStatement>();
    while (peek().type != TokenType::CLOSE_BRACE && peek().type != TokenType::END_OF_FILE) {
        compound->statements.push_back(parseStatement());
    }
    expect(TokenType::CLOSE_BRACE, "Expected '}'");
    return compound;
}

std::unique_ptr<Statement> Parser::parseStatement() {
    if (match(TokenType::RETURN)) {
        auto expr = parseExpression();
        expect(TokenType::SEMICOLON, "Expected ';'");
        return std::make_unique<ReturnStatement>(std::move(expr));
    }
    
    auto expr = parseExpression();
    expect(TokenType::SEMICOLON, "Expected ';'");
    return std::make_unique<ExpressionStatement>(std::move(expr));
}

std::unique_ptr<Expression> Parser::parseExpression() {
    return parsePrimary();
}

std::unique_ptr<Expression> Parser::parsePrimary() {
    if (peek().type == TokenType::IDENTIFIER) {
        std::string name = advance().value;
        if (match(TokenType::OPEN_PAREN)) {
            auto call = std::make_unique<FunctionCall>(name);
            if (peek().type != TokenType::CLOSE_PAREN) {
                do {
                    call->arguments.push_back(parseExpression());
                } while (match(TokenType::COMMA));
            }
            expect(TokenType::CLOSE_PAREN, "Expected ')'");
            return call;
        }
        return std::make_unique<VariableReference>(name);
    }

    if (peek().type == TokenType::INTEGER_LITERAL) {
        return std::make_unique<IntegerLiteral>(std::stoi(advance().value));
    }

    if (peek().type == TokenType::STRING_LITERAL) {
        return std::make_unique<StringLiteral>(advance().value);
    }

    throw std::runtime_error("Expected expression at " + std::to_string(peek().line) + ":" + std::to_string(peek().column));
}
