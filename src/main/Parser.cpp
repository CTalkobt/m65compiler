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
    auto unit = setPos(std::make_unique<TranslationUnit>(), tokens[0]);
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
    const Token& startToken = peek();
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
    auto func = setPos(std::make_unique<FunctionDeclaration>(name, returnType), startToken);
    func->parameters = std::move(params);
    func->body = std::move(body);
    return func;
}

std::unique_ptr<CompoundStatement> Parser::parseCompoundStatement() {
    const Token& startToken = expect(TokenType::OPEN_BRACE, "Expected '{'");
    auto compound = setPos(std::make_unique<CompoundStatement>(), startToken);
    while (peek().type != TokenType::CLOSE_BRACE && peek().type != TokenType::END_OF_FILE) {
        compound->statements.push_back(parseStatement());
    }
    expect(TokenType::CLOSE_BRACE, "Expected '}'");
    return compound;
}

std::unique_ptr<Statement> Parser::parseStatement() {
    if (match(TokenType::STRUCT)) {
        const Token& startToken = tokens[pos-1];
        if (peek().type == TokenType::IDENTIFIER && pos + 1 < tokens.size() && tokens[pos+1].type == TokenType::OPEN_BRACE) {
            return parseStructDefinition();
        }
        // struct name var;
        std::string structName = expect(TokenType::IDENTIFIER, "Expected struct name").value;
        int ptrLevel = 0;
        while (match(TokenType::STAR)) ptrLevel++;
        std::string varName = expect(TokenType::IDENTIFIER, "Expected variable name").value;
        auto decl = setPos(std::make_unique<VariableDeclaration>("struct " + structName, varName, ptrLevel), startToken);
        if (match(TokenType::EQUALS)) {
            decl->initializer = parseExpression();
        }
        expect(TokenType::SEMICOLON, "Expected ';'");
        return decl;
    }

    if (peek().type == TokenType::INT || peek().type == TokenType::CHAR) {
        const Token& startToken = peek();
        std::string type = (advance().type == TokenType::INT) ? "int" : "char";
        int ptrLevel = 0;
        while (match(TokenType::STAR)) ptrLevel++;
        std::string name = expect(TokenType::IDENTIFIER, "Expected variable name").value;
        auto decl = setPos(std::make_unique<VariableDeclaration>(type, name, ptrLevel), startToken);
        if (match(TokenType::EQUALS)) {
            decl->initializer = parseExpression();
        }
        expect(TokenType::SEMICOLON, "Expected ';'");
        return decl;
    }

    if (match(TokenType::RETURN)) {
        const Token& startToken = tokens[pos-1];
        std::unique_ptr<Expression> expr = nullptr;
        if (peek().type != TokenType::SEMICOLON) {
            expr = parseExpression();
        }
        expect(TokenType::SEMICOLON, "Expected ';'");
        return setPos(std::make_unique<ReturnStatement>(std::move(expr)), startToken);
    }

    if (match(TokenType::IF)) {
        const Token& startToken = tokens[pos-1];
        expect(TokenType::OPEN_PAREN, "Expected '(' after 'if'");
        auto condition = parseExpression();
        expect(TokenType::CLOSE_PAREN, "Expected ')' after if condition");
        auto thenBranch = parseStatement();
        std::unique_ptr<Statement> elseBranch = nullptr;
        if (match(TokenType::ELSE)) {
            elseBranch = parseStatement();
        }
        return setPos(std::make_unique<IfStatement>(std::move(condition), std::move(thenBranch), std::move(elseBranch)), startToken);
    }

    if (match(TokenType::WHILE)) {
        const Token& startToken = tokens[pos-1];
        expect(TokenType::OPEN_PAREN, "Expected '(' after 'while'");
        auto condition = parseExpression();
        expect(TokenType::CLOSE_PAREN, "Expected ')' after while condition");
        auto body = parseStatement();
        return setPos(std::make_unique<WhileStatement>(std::move(condition), std::move(body)), startToken);
    }

    if (match(TokenType::DO)) {
        const Token& startToken = tokens[pos-1];
        auto body = parseStatement();
        expect(TokenType::WHILE, "Expected 'while' after 'do' body");
        expect(TokenType::OPEN_PAREN, "Expected '(' after 'while'");
        auto condition = parseExpression();
        expect(TokenType::CLOSE_PAREN, "Expected ')' after while condition");
        expect(TokenType::SEMICOLON, "Expected ';' after do-while");
        return setPos(std::make_unique<DoWhileStatement>(std::move(body), std::move(condition)), startToken);
    }

    if (match(TokenType::FOR)) {
        const Token& startToken = tokens[pos-1];
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
        return setPos(std::make_unique<ForStatement>(std::move(initializer), std::move(condition), std::move(increment), std::move(body)), startToken);
    }

    if (match(TokenType::ASM)) {
        const Token& startToken = tokens[pos-1];
        expect(TokenType::OPEN_PAREN, "Expected '(' after asm");
        std::string code = expect(TokenType::STRING_LITERAL, "Expected string literal for asm code").value;
        expect(TokenType::CLOSE_PAREN, "Expected ')' after asm code");
        expect(TokenType::SEMICOLON, "Expected ';'");
        return setPos(std::make_unique<AsmStatement>(code), startToken);
    }

    if (peek().type == TokenType::OPEN_BRACE) {
        return parseCompoundStatement();
    }
    
    const Token& startToken = peek();
    auto expr = parseExpression();
    expect(TokenType::SEMICOLON, "Expected ';'");
    return setPos(std::make_unique<ExpressionStatement>(std::move(expr)), startToken);
}

std::unique_ptr<StructDefinition> Parser::parseStructDefinition() {
    const Token& startToken = tokens[pos-1]; // 'struct'
    std::string name = expect(TokenType::IDENTIFIER, "Expected struct name").value;
    expect(TokenType::OPEN_BRACE, "Expected '{'");
    auto def = setPos(std::make_unique<StructDefinition>(name), startToken);
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
        const Token& opToken = tokens[pos-1];
        return setPos(std::make_unique<Assignment>(std::move(expr), parseExpression()), opToken);
    }
    return expr;
}

std::unique_ptr<Expression> Parser::parseLogicalOr() {
    auto left = parseLogicalAnd();
    while (match(TokenType::OR)) {
        const Token& opToken = tokens[pos-1];
        std::string op = opToken.value;
        auto right = parseLogicalAnd();
        left = setPos(std::make_unique<BinaryOperation>(op, std::move(left), std::move(right)), opToken);
    }
    return left;
}

std::unique_ptr<Expression> Parser::parseLogicalAnd() {
    auto left = parseBitwiseOr();
    while (match(TokenType::AND)) {
        const Token& opToken = tokens[pos-1];
        std::string op = opToken.value;
        auto right = parseBitwiseOr();
        left = setPos(std::make_unique<BinaryOperation>(op, std::move(left), std::move(right)), opToken);
    }
    return left;
}

std::unique_ptr<Expression> Parser::parseBitwiseOr() {
    auto left = parseBitwiseXor();
    while (match(TokenType::PIPE)) {
        const Token& opToken = tokens[pos-1];
        std::string op = opToken.value;
        auto right = parseBitwiseXor();
        left = setPos(std::make_unique<BinaryOperation>(op, std::move(left), std::move(right)), opToken);
    }
    return left;
}

std::unique_ptr<Expression> Parser::parseBitwiseXor() {
    auto left = parseBitwiseAnd();
    while (match(TokenType::CARET)) {
        const Token& opToken = tokens[pos-1];
        std::string op = opToken.value;
        auto right = parseBitwiseAnd();
        left = setPos(std::make_unique<BinaryOperation>(op, std::move(left), std::move(right)), opToken);
    }
    return left;
}

std::unique_ptr<Expression> Parser::parseBitwiseAnd() {
    auto left = parseEquality();
    while (match(TokenType::AMPERSAND)) {
        const Token& opToken = tokens[pos-1];
        std::string op = opToken.value;
        auto right = parseEquality();
        left = setPos(std::make_unique<BinaryOperation>(op, std::move(left), std::move(right)), opToken);
    }
    return left;
}

std::unique_ptr<Expression> Parser::parseEquality() {
    auto left = parseRelational();
    while (match(TokenType::EQUALS_EQUALS) || match(TokenType::NOT_EQUALS)) {
        const Token& opToken = tokens[pos-1];
        std::string op = opToken.value;
        auto right = parseRelational();
        left = setPos(std::make_unique<BinaryOperation>(op, std::move(left), std::move(right)), opToken);
    }
    return left;
}

std::unique_ptr<Expression> Parser::parseRelational() {
    auto left = parseShift();
    while (match(TokenType::LESS_THAN) || match(TokenType::GREATER_THAN) ||
           match(TokenType::LESS_EQUAL) || match(TokenType::GREATER_EQUAL)) {
        const Token& opToken = tokens[pos-1];
        std::string op = opToken.value;
        auto right = parseShift();
        left = setPos(std::make_unique<BinaryOperation>(op, std::move(left), std::move(right)), opToken);
    }
    return left;
}

std::unique_ptr<Expression> Parser::parseShift() {
    auto left = parseAdditive();
    while (match(TokenType::LSHIFT) || match(TokenType::RSHIFT)) {
        const Token& opToken = tokens[pos-1];
        std::string op = opToken.value;
        auto right = parseAdditive();
        left = setPos(std::make_unique<BinaryOperation>(op, std::move(left), std::move(right)), opToken);
    }
    return left;
}

std::unique_ptr<Expression> Parser::parseAdditive() {
    auto left = parseMultiplicative();
    while (match(TokenType::PLUS) || match(TokenType::MINUS)) {
        const Token& opToken = tokens[pos-1];
        std::string op = opToken.value;
        auto right = parseMultiplicative();
        left = setPos(std::make_unique<BinaryOperation>(op, std::move(left), std::move(right)), opToken);
    }
    return left;
}

std::unique_ptr<Expression> Parser::parseMultiplicative() {
    auto left = parseUnary();
    while (match(TokenType::STAR) || match(TokenType::SLASH)) {
        const Token& opToken = tokens[pos-1];
        std::string op = opToken.value;
        auto right = parseUnary();
        left = setPos(std::make_unique<BinaryOperation>(op, std::move(left), std::move(right)), opToken);
    }
    return left;
}

std::unique_ptr<Expression> Parser::parseUnary() {
    if (match(TokenType::BANG) || match(TokenType::TILDE) || match(TokenType::MINUS) || match(TokenType::STAR) || match(TokenType::AMPERSAND)) {
        const Token& opToken = tokens[pos-1];
        std::string op = opToken.value;
        return setPos(std::make_unique<UnaryOperation>(op, parseUnary()), opToken);
    }
    return parsePrimary();
}

std::unique_ptr<Expression> Parser::parsePrimary() {
    std::unique_ptr<Expression> expr;
    if (peek().type == TokenType::IDENTIFIER) {
        const Token& nameToken = advance();
        std::string name = nameToken.value;
        if (match(TokenType::OPEN_PAREN)) {
            auto call = setPos(std::make_unique<FunctionCall>(name), nameToken);
            if (peek().type != TokenType::CLOSE_PAREN) {
                do {
                    call->arguments.push_back(parseExpression());
                } while (match(TokenType::COMMA));
            }
            expect(TokenType::CLOSE_PAREN, "Expected ')'");
            expr = std::move(call);
        } else {
            expr = setPos(std::make_unique<VariableReference>(name), nameToken);
        }
    } else if (peek().type == TokenType::INTEGER_LITERAL) {
        const Token& litToken = advance();
        expr = setPos(std::make_unique<IntegerLiteral>(std::stoi(litToken.value)), litToken);
    } else if (peek().type == TokenType::STRING_LITERAL) {
        const Token& litToken = advance();
        expr = setPos(std::make_unique<StringLiteral>(litToken.value), litToken);
    } else if (match(TokenType::OPEN_PAREN)) {
        expr = parseExpression();
        expect(TokenType::CLOSE_PAREN, "Expected ')'");
    } else {
        std::string foundStr = peek().value.empty() ? peek().typeToString() : peek().value;
        throw std::runtime_error("Syntax Error at " + std::to_string(peek().line) + ":" + std::to_string(peek().column) + ": Expected expression. Found '" + foundStr + "' (" + peek().typeToString() + ") instead.");
    }

    while (match(TokenType::DOT) || match(TokenType::ARROW)) {
        const Token& opToken = tokens[pos-1];
        bool isArrow = (opToken.type == TokenType::ARROW);
        std::string memberName = expect(TokenType::IDENTIFIER, "Expected member name").value;
        expr = setPos(std::make_unique<MemberAccess>(std::move(expr), memberName, isArrow), opToken);
    }
    return expr;
}
