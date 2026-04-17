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
    std::string foundStr = peek().value.empty() ? peek().typeToString() : peek().value;
    throw std::runtime_error("Syntax Error at " + std::to_string(peek().line) + ":" + std::to_string(peek().column) + ": " + message + ". Found '" + foundStr + "' (" + peek().typeToString() + ") instead.");
}

std::unique_ptr<TranslationUnit> Parser::parse() {
    auto unit = std::make_unique<TranslationUnit>();
    while (peek().type != TokenType::END_OF_FILE) {
        if (peek().type == TokenType::STRUCT) {
            // Check if it's a definition: struct name { ... };
            if (pos + 1 < tokens.size() && tokens[pos+1].type == TokenType::IDENTIFIER &&
                pos + 2 < tokens.size() && tokens[pos+2].type == TokenType::OPEN_BRACE) {
                advance(); // struct
                unit->topLevelDecls.push_back(parseStructDefinition());
            } else {
                unit->topLevelDecls.push_back(parseFunctionDeclaration());
            }
        } else {
            unit->topLevelDecls.push_back(parseFunctionDeclaration());
        }
    }
    return unit;
}

std::unique_ptr<FunctionDeclaration> Parser::parseFunctionDeclaration() {
    std::string returnType;
    if (match(TokenType::INT)) returnType = "int";
    else if (match(TokenType::CHAR)) returnType = "char";
    else if (match(TokenType::VOID)) returnType = "void";
    else if (match(TokenType::STRUCT)) returnType = "struct " + expect(TokenType::IDENTIFIER, "Expected struct name").value;
    else {
        std::string foundStr = peek().value.empty() ? peek().typeToString() : peek().value;
        throw std::runtime_error("Syntax Error at " + std::to_string(peek().line) + ":" + std::to_string(peek().column) + ": Expected return type (int, char, void, struct) for function declaration. Found '" + foundStr + "' instead.");
    }

    match(TokenType::STAR); // return pointer (optional)

    std::string name = expect(TokenType::IDENTIFIER, "Expected function name").value;
    
    expect(TokenType::OPEN_PAREN, "Expected '('");
    std::vector<Parameter> params;
    if (peek().type != TokenType::CLOSE_PAREN) {
        do {
            std::string pType;
            if (match(TokenType::INT)) pType = "int";
            else if (match(TokenType::CHAR)) pType = "char";
            else if (match(TokenType::STRUCT)) pType = "struct " + expect(TokenType::IDENTIFIER, "Expected struct name").value;
            else {
                std::string foundStr = peek().value.empty() ? peek().typeToString() : peek().value;
                throw std::runtime_error("Syntax Error at " + std::to_string(peek().line) + ":" + std::to_string(peek().column) + ": Expected parameter type (int, char, struct). Found '" + foundStr + "' instead.");
            }

            int pPtrLevel = 0;
            while (match(TokenType::STAR)) pPtrLevel++;

            std::string pName = expect(TokenType::IDENTIFIER, "Expected parameter name").value;
            params.push_back({pType, pPtrLevel, pName});
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
    if (match(TokenType::STRUCT)) {
        if (peek().type == TokenType::IDENTIFIER && pos + 1 < tokens.size() && tokens[pos+1].type == TokenType::OPEN_BRACE) {
            return parseStructDefinition();
        }
        // struct name var;
        std::string structName = expect(TokenType::IDENTIFIER, "Expected struct name").value;
        int ptrLevel = 0;
        while (match(TokenType::STAR)) ptrLevel++;
        std::string varName = expect(TokenType::IDENTIFIER, "Expected variable name").value;
        auto decl = std::make_unique<VariableDeclaration>("struct " + structName, varName, ptrLevel);
        if (match(TokenType::EQUALS)) {
            decl->initializer = parseExpression();
        }
        expect(TokenType::SEMICOLON, "Expected ';'");
        return decl;
    }

    if (peek().type == TokenType::INT || peek().type == TokenType::CHAR) {
        std::string type = (advance().type == TokenType::INT) ? "int" : "char";
        int ptrLevel = 0;
        while (match(TokenType::STAR)) ptrLevel++;
        std::string name = expect(TokenType::IDENTIFIER, "Expected variable name").value;
        auto decl = std::make_unique<VariableDeclaration>(type, name, ptrLevel);
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

    if (match(TokenType::DO)) {
        auto body = parseStatement();
        expect(TokenType::WHILE, "Expected 'while' after 'do' body");
        expect(TokenType::OPEN_PAREN, "Expected '(' after 'while'");
        auto condition = parseExpression();
        expect(TokenType::CLOSE_PAREN, "Expected ')' after while condition");
        expect(TokenType::SEMICOLON, "Expected ';' after do-while");
        return std::make_unique<DoWhileStatement>(std::move(body), std::move(condition));
    }

    if (match(TokenType::FOR)) {
        expect(TokenType::OPEN_PAREN, "Expected '(' after 'for'");
        std::unique_ptr<Statement> initializer = nullptr;
        if (!match(TokenType::SEMICOLON)) {
            initializer = parseStatement();
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

std::unique_ptr<StructDefinition> Parser::parseStructDefinition() {
    std::string name = expect(TokenType::IDENTIFIER, "Expected struct name").value;
    expect(TokenType::OPEN_BRACE, "Expected '{'");
    auto def = std::make_unique<StructDefinition>(name);
    while (peek().type != TokenType::CLOSE_BRACE && peek().type != TokenType::END_OF_FILE) {
        std::string type;
        if (match(TokenType::INT)) type = "int";
        else if (match(TokenType::CHAR)) type = "char";
        else if (match(TokenType::STRUCT)) {
            type = "struct " + expect(TokenType::IDENTIFIER, "Expected struct name").value;
        } else {
            throw std::runtime_error("Expected member type");
        }
        int ptrLevel = 0;
        while (match(TokenType::STAR)) ptrLevel++;
        std::string memberName = expect(TokenType::IDENTIFIER, "Expected member name").value;
        def->members.push_back({type, ptrLevel, memberName});
        expect(TokenType::SEMICOLON, "Expected ';'");
    }
    expect(TokenType::CLOSE_BRACE, "Expected '}'");
    expect(TokenType::SEMICOLON, "Expected ';'");
    return def;
}

std::unique_ptr<Expression> Parser::parseExpression() {
    auto expr = parseLogicalOr();
    if (match(TokenType::EQUALS)) {
        return std::make_unique<Assignment>(std::move(expr), parseExpression());
    }
    return expr;
}

std::unique_ptr<Expression> Parser::parseLogicalOr() {
    auto left = parseLogicalAnd();
    while (match(TokenType::OR)) {
        std::string op = tokens[pos-1].value;
        auto right = parseLogicalAnd();
        left = std::make_unique<BinaryOperation>(op, std::move(left), std::move(right));
    }
    return left;
}

std::unique_ptr<Expression> Parser::parseLogicalAnd() {
    auto left = parseBitwiseOr();
    while (match(TokenType::AND)) {
        std::string op = tokens[pos-1].value;
        auto right = parseBitwiseOr();
        left = std::make_unique<BinaryOperation>(op, std::move(left), std::move(right));
    }
    return left;
}

std::unique_ptr<Expression> Parser::parseBitwiseOr() {
    auto left = parseBitwiseXor();
    while (match(TokenType::PIPE)) {
        std::string op = tokens[pos-1].value;
        auto right = parseBitwiseXor();
        left = std::make_unique<BinaryOperation>(op, std::move(left), std::move(right));
    }
    return left;
}

std::unique_ptr<Expression> Parser::parseBitwiseXor() {
    auto left = parseBitwiseAnd();
    while (match(TokenType::CARET)) {
        std::string op = tokens[pos-1].value;
        auto right = parseBitwiseAnd();
        left = std::make_unique<BinaryOperation>(op, std::move(left), std::move(right));
    }
    return left;
}

std::unique_ptr<Expression> Parser::parseBitwiseAnd() {
    auto left = parseEquality();
    while (match(TokenType::AMPERSAND)) {
        std::string op = tokens[pos-1].value;
        auto right = parseEquality();
        left = std::make_unique<BinaryOperation>(op, std::move(left), std::move(right));
    }
    return left;
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
    auto left = parseShift();
    while (match(TokenType::LESS_THAN) || match(TokenType::GREATER_THAN) ||
           match(TokenType::LESS_EQUAL) || match(TokenType::GREATER_EQUAL)) {
        std::string op = tokens[pos-1].value;
        auto right = parseShift();
        left = std::make_unique<BinaryOperation>(op, std::move(left), std::move(right));
    }
    return left;
}

std::unique_ptr<Expression> Parser::parseShift() {
    auto left = parseAdditive();
    while (match(TokenType::LSHIFT) || match(TokenType::RSHIFT)) {
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
    auto left = parseUnary();
    while (match(TokenType::STAR) || match(TokenType::SLASH)) {
        std::string op = tokens[pos-1].value;
        auto right = parseUnary();
        left = std::make_unique<BinaryOperation>(op, std::move(left), std::move(right));
    }
    return left;
}

std::unique_ptr<Expression> Parser::parseUnary() {
    if (match(TokenType::BANG) || match(TokenType::TILDE) || match(TokenType::MINUS) || match(TokenType::STAR) || match(TokenType::AMPERSAND)) {
        std::string op = tokens[pos-1].value;
        return std::make_unique<UnaryOperation>(op, parseUnary());
    }
    return parsePrimary();
}

std::unique_ptr<Expression> Parser::parsePrimary() {
    std::unique_ptr<Expression> expr;
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
            expr = std::move(call);
        } else {
            expr = std::make_unique<VariableReference>(name);
        }
    } else if (peek().type == TokenType::INTEGER_LITERAL) {
        expr = std::make_unique<IntegerLiteral>(std::stoi(advance().value));
    } else if (peek().type == TokenType::STRING_LITERAL) {
        expr = std::make_unique<StringLiteral>(advance().value);
    } else if (match(TokenType::OPEN_PAREN)) {
        expr = parseExpression();
        expect(TokenType::CLOSE_PAREN, "Expected ')'");
    } else {
        std::string foundStr = peek().value.empty() ? peek().typeToString() : peek().value;
        throw std::runtime_error("Syntax Error at " + std::to_string(peek().line) + ":" + std::to_string(peek().column) + ": Expected expression. Found '" + foundStr + "' (" + peek().typeToString() + ") instead.");
    }

    while (match(TokenType::DOT) || match(TokenType::ARROW)) {
        bool isArrow = (tokens[pos-1].type == TokenType::ARROW);
        std::string memberName = expect(TokenType::IDENTIFIER, "Expected member name").value;
        expr = std::make_unique<MemberAccess>(std::move(expr), memberName, isArrow);
    }
    return expr;
}
