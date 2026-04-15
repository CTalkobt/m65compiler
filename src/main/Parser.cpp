#include "Parser.hpp"
#include <stdexcept>
#include <iostream>

Parser::Parser(const std::vector<Token>& tokens) : tokens(tokens), pos(0) {}

const Token& Parser::peek() const {
    if (pos >= tokens.size()) return tokens.back();
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
    std::string returnType;
    if (match(TokenType::INT)) returnType = "int";
    else if (match(TokenType::CHAR)) returnType = "char";
    else if (match(TokenType::VOID)) returnType = "void";
    else throw std::runtime_error("Error at " + std::to_string(peek().line) + ":" + std::to_string(peek().column) + ": Expected return type");

    match(TokenType::STAR); // return pointer (optional)

    std::string name = expect(TokenType::IDENTIFIER, "Expected function name").value;
    
    expect(TokenType::OPEN_PAREN, "Expected '('");
    std::vector<Parameter> params;
    if (peek().type != TokenType::CLOSE_PAREN) {
        do {
            std::string pType;
            if (match(TokenType::INT)) pType = "int";
            else if (match(TokenType::CHAR)) pType = "char";
            else throw std::runtime_error("Error at " + std::to_string(peek().line) + ":" + std::to_string(peek().column) + ": Expected parameter type");

            bool pIsPtr = match(TokenType::STAR);

            std::string pName = expect(TokenType::IDENTIFIER, "Expected parameter name").value;
            params.push_back({pType, pIsPtr, pName});
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
    if (peek().type == TokenType::INT || peek().type == TokenType::CHAR) {
        std::string type = (advance().type == TokenType::INT) ? "int" : "char";
        bool isPtr = match(TokenType::STAR);
        std::string name = expect(TokenType::IDENTIFIER, "Expected variable name").value;
        auto decl = std::make_unique<VariableDeclaration>(type, name, isPtr);
        if (match(TokenType::EQUALS)) {
            decl->initializer = parseExpression();
        }
        expect(TokenType::SEMICOLON, "Expected ';'");
        return decl;
    }

    if (match(TokenType::RETURN)) {
        std::unique_ptr<Expression> expr = nullptr;
        if (peek().type != TokenType::SEMICOLON) {
            expr = parseExpression();
        }
        expect(TokenType::SEMICOLON, "Expected ';'");
        return std::make_unique<ReturnStatement>(std::move(expr));
    }

    if (match(TokenType::IF)) {
        expect(TokenType::OPEN_PAREN, "Expected '(' after 'if'");
        auto condition = parseExpression();
        expect(TokenType::CLOSE_PAREN, "Expected ')' after if condition");
        auto thenBranch = parseStatement();
        std::unique_ptr<Statement> elseBranch = nullptr;
        if (match(TokenType::ELSE)) {
            elseBranch = parseStatement();
        }
        return std::make_unique<IfStatement>(std::move(condition), std::move(thenBranch), std::move(elseBranch));
    }

    if (match(TokenType::WHILE)) {
        expect(TokenType::OPEN_PAREN, "Expected '(' after 'while'");
        auto condition = parseExpression();
        expect(TokenType::CLOSE_PAREN, "Expected ')' after while condition");
        auto body = parseStatement();
        return std::make_unique<WhileStatement>(std::move(condition), std::move(body));
    }

    if (match(TokenType::FOR)) {
        expect(TokenType::OPEN_PAREN, "Expected '(' after 'for'");
        std::unique_ptr<Statement> initializer = nullptr;
        if (!match(TokenType::SEMICOLON)) {
            initializer = parseStatement(); // parseStatement handles its own semicolon for decls/exprs
        }
        std::unique_ptr<Expression> condition = nullptr;
        if (!match(TokenType::SEMICOLON)) {
            condition = parseExpression();
            expect(TokenType::SEMICOLON, "Expected ';' after for condition");
        }
        std::unique_ptr<Expression> increment = nullptr;
        if (!match(TokenType::CLOSE_PAREN)) {
            increment = parseExpression();
            expect(TokenType::CLOSE_PAREN, "Expected ')' after for increment");
        }
        auto body = parseStatement();
        return std::make_unique<ForStatement>(std::move(initializer), std::move(condition), std::move(increment), std::move(body));
    }

    if (match(TokenType::ASM)) {
        expect(TokenType::OPEN_PAREN, "Expected '(' after asm");
        std::string code = expect(TokenType::STRING_LITERAL, "Expected string literal for asm code").value;
        expect(TokenType::CLOSE_PAREN, "Expected ')' after asm code");
        expect(TokenType::SEMICOLON, "Expected ';'");
        return std::make_unique<AsmStatement>(code);
    }

    if (peek().type == TokenType::OPEN_BRACE) {
        return parseCompoundStatement();
    }
    
    auto expr = parseExpression();
    expect(TokenType::SEMICOLON, "Expected ';'");
    return std::make_unique<ExpressionStatement>(std::move(expr));
}

std::unique_ptr<Expression> Parser::parseExpression() {
    // Assignment has lowest precedence
    if (peek().type == TokenType::IDENTIFIER && pos + 1 < tokens.size() && tokens[pos+1].type == TokenType::EQUALS) {
        std::string name = advance().value;
        advance(); // consume =
        return std::make_unique<Assignment>(name, parseExpression());
    }
    return parseEquality();
}

std::unique_ptr<Expression> Parser::parseEquality() {
    auto left = parseRelational();
    while (match(TokenType::EQUALS_EQUALS) || match(TokenType::NOT_EQUALS)) {
        std::string op = tokens[pos-1].value;
        auto right = parseRelational();
        left = std::make_unique<BinaryOperation>(op, std::move(left), std::move(right));
    }
    return left;
}

std::unique_ptr<Expression> Parser::parseRelational() {
    auto left = parseAdditive();
    while (match(TokenType::LESS_THAN) || match(TokenType::GREATER_THAN) ||
           match(TokenType::LESS_EQUAL) || match(TokenType::GREATER_EQUAL)) {
        std::string op = tokens[pos-1].value;
        auto right = parseAdditive();
        left = std::make_unique<BinaryOperation>(op, std::move(left), std::move(right));
    }
    return left;
}

std::unique_ptr<Expression> Parser::parseAdditive() {
    auto left = parseMultiplicative();
    while (match(TokenType::PLUS) || match(TokenType::MINUS)) {
        std::string op = tokens[pos-1].value;
        auto right = parseMultiplicative();
        left = std::make_unique<BinaryOperation>(op, std::move(left), std::move(right));
    }
    return left;
}

std::unique_ptr<Expression> Parser::parseMultiplicative() {
    auto left = parsePrimary();
    while (match(TokenType::STAR) || match(TokenType::SLASH)) {
        std::string op = tokens[pos-1].value;
        auto right = parsePrimary();
        left = std::make_unique<BinaryOperation>(op, std::move(left), std::move(right));
    }
    return left;
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

    if (match(TokenType::OPEN_PAREN)) {
        auto expr = parseExpression();
        expect(TokenType::CLOSE_PAREN, "Expected ')'");
        return expr;
    }

    throw std::runtime_error("Expected expression at " + std::to_string(peek().line) + ":" + std::to_string(peek().column));
}
