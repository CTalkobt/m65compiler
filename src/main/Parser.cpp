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
        if (peek().type == TokenType::ASM) {
            advance();
            expect(TokenType::OPEN_PAREN, "Expected '(' after asm");
            std::string code = expect(TokenType::STRING_LITERAL, "Expected string literal for asm code").value;
            expect(TokenType::CLOSE_PAREN, "Expected ')' after asm code");
            expect(TokenType::SEMICOLON, "Expected ';'");
            flushPending(*unit);
            unit->topLevelDecls.push_back(setPos(std::make_unique<AsmStatement>(code), tokens[pos-5]));
            continue;
        }
        if (peek().type == TokenType::_Static_assert) {
            auto decl = parseStaticAssert();
            flushPending(*unit);
            unit->topLevelDecls.push_back(std::unique_ptr<Statement>(std::move(decl)));
            continue;
        }

        if (peek().type == TokenType::STRUCT || peek().type == TokenType::UNION) {
            bool isUnion = peek().type == TokenType::UNION;
            // Check if it's a definition: struct/union name { ... };
            if (pos + 1 < tokens.size() && tokens[pos+1].type == TokenType::IDENTIFIER &&
                pos + 2 < tokens.size() && tokens[pos+2].type == TokenType::OPEN_BRACE) {
                advance(); // struct/union
                auto def = parseStructDefinition(isUnion);
                flushPending(*unit);
                unit->topLevelDecls.push_back(std::move(def));
                continue;
            }
        }

        // Look ahead to distinguish function from global variable
        size_t look = pos;
        bool isVol = false;
        bool isNR = false;

        if (tokens[look].type == TokenType::_Noreturn) {
            isNR = true;
            look++;
        }

        if (tokens[look].type == TokenType::_Alignas) {
            look++;
            if (look < tokens.size() && tokens[look].type == TokenType::OPEN_PAREN) {
                look++;
                int parens = 1;
                while (look < tokens.size() && parens > 0) {
                    if (tokens[look].type == TokenType::OPEN_PAREN) parens++;
                    else if (tokens[look].type == TokenType::CLOSE_PAREN) parens--;
                    look++;
                }
            }
        }
        
        bool isSig = false;

        while (tokens[look].type == TokenType::VOLATILE || tokens[look].type == TokenType::AUTO ||
               tokens[look].type == TokenType::SIGNED || tokens[look].type == TokenType::UNSIGNED) {
            if (tokens[look].type == TokenType::VOLATILE) isVol = true;
            else if (tokens[look].type == TokenType::SIGNED) isSig = true;
            look++;
        }

        if (look < tokens.size() && (tokens[look].type == TokenType::INT || 
                                     tokens[look].type == TokenType::CHAR || 
                                     tokens[look].type == TokenType::UNSIGNED ||
                                     tokens[look].type == TokenType::SIGNED ||
                                     tokens[look].type == TokenType::VOID ||
                                     tokens[look].type == TokenType::STRUCT ||
                                     tokens[look].type == TokenType::UNION)) {
            if (tokens[look].type == TokenType::UNSIGNED || tokens[look].type == TokenType::SIGNED) {
                look++;
                if (look < tokens.size() && (tokens[look].type == TokenType::INT || tokens[look].type == TokenType::CHAR)) {
                    look++;
                }
            } else if (tokens[look].type == TokenType::STRUCT || tokens[look].type == TokenType::UNION) {
                look++; // skip struct/union
                if (look < tokens.size() && tokens[look].type == TokenType::IDENTIFIER) {
                    look++; // skip struct/union name
                } else if (look < tokens.size() && tokens[look].type == TokenType::OPEN_BRACE) {
                    int braceLevel = 1;
                    look++;
                    while (look < tokens.size() && braceLevel > 0) {
                        if (tokens[look].type == TokenType::OPEN_BRACE) braceLevel++;
                        else if (tokens[look].type == TokenType::CLOSE_BRACE) braceLevel--;
                        look++;
                    }
                } else {
                    auto decl = parseFunctionDeclaration();
                    flushPending(*unit);
                    unit->topLevelDecls.push_back(std::move(decl));
                    continue;
                }
            } else {
                look++; // skip int/char/void
            }
            
            while (look < tokens.size() && tokens[look].type == TokenType::STAR) look++;
            
            if (look < tokens.size() && tokens[look].type == TokenType::IDENTIFIER) {
                look++;
                if (look < tokens.size() && tokens[look].type == TokenType::OPEN_PAREN) {
                    if (isNR) match(TokenType::_Noreturn);
                    while (match(TokenType::VOLATILE) || match(TokenType::AUTO) || match(TokenType::SIGNED) || match(TokenType::UNSIGNED));
                    auto decl = parseFunctionDeclaration();
                    decl->isNoreturn = isNR;
                    flushPending(*unit);
                    unit->topLevelDecls.push_back(std::move(decl));
                } else {
                    if (isNR) match(TokenType::_Noreturn);
                    while (match(TokenType::VOLATILE) || match(TokenType::AUTO) || match(TokenType::SIGNED) || match(TokenType::UNSIGNED));
                    auto decl = parseVariableDeclaration(isVol);
                    if (auto* vd = dynamic_cast<VariableDeclaration*>(decl.get())) {
                        vd->isGlobal = true;
                        vd->isSigned = isSig;
                    }
                    flushPending(*unit);
                    unit->topLevelDecls.push_back(std::move(decl));
                }
            } else {
                if (isNR) match(TokenType::_Noreturn);
                while (match(TokenType::VOLATILE) || match(TokenType::AUTO) || match(TokenType::SIGNED) || match(TokenType::UNSIGNED));
                auto decl = parseFunctionDeclaration();
                decl->isNoreturn = isNR;
                flushPending(*unit);
                unit->topLevelDecls.push_back(std::move(decl));
            }
        } else {
            if (isNR) match(TokenType::_Noreturn);
            while (match(TokenType::VOLATILE) || match(TokenType::AUTO) || match(TokenType::SIGNED) || match(TokenType::UNSIGNED));
            auto decl = parseFunctionDeclaration();
            decl->isNoreturn = isNR;
            flushPending(*unit);
            unit->topLevelDecls.push_back(std::unique_ptr<Statement>(std::move(decl)));
        }
    }
    return unit;
}

std::unique_ptr<FunctionDeclaration> Parser::parseFunctionDeclaration() {
    const Token& startToken = peek();
    bool isNR = match(TokenType::_Noreturn);
    std::string returnType;
    bool isSigned = false;
    if (match(TokenType::SIGNED)) {
        isSigned = true;
        if (match(TokenType::INT)) returnType = "int";
        else if (match(TokenType::CHAR)) returnType = "char";
        else returnType = "int";
    }
    else if (match(TokenType::UNSIGNED)) {
        if (match(TokenType::INT)) returnType = "int";
        else if (match(TokenType::CHAR)) returnType = "char";
        else returnType = "int"; // bare 'unsigned' is 'unsigned int'
    }
    else if (match(TokenType::INT)) returnType = "int";
    else if (match(TokenType::CHAR)) returnType = "char";
    else if (match(TokenType::VOID)) returnType = "void";
    else if (match(TokenType::STRUCT) || match(TokenType::UNION)) {
        bool isU = tokens[pos-1].type == TokenType::UNION;
        returnType = (isU ? "union " : "struct ") + expect(TokenType::IDENTIFIER, "Expected struct/union name").value;
    }
    else {
        std::string foundStr = peek().value.empty() ? peek().typeToString() : peek().value;
        throw std::runtime_error("Syntax Error at " + std::to_string(peek().line) + ":" + std::to_string(peek().column) + ": Expected return type (int, char, void, struct, union) for function declaration. Found '" + foundStr + "' instead.");
    }

    match(TokenType::STAR); // return pointer (optional)

    std::string name = expect(TokenType::IDENTIFIER, "Expected function name").value;
    
    expect(TokenType::OPEN_PAREN, "Expected '('");
    std::vector<Parameter> params;
    if (peek().type != TokenType::CLOSE_PAREN) {
        do {
            bool pIsVolatile = false;
            if (match(TokenType::VOLATILE)) {
                pIsVolatile = true;
            }
            std::string pType;
            bool pIsSigned = false;
            if (match(TokenType::SIGNED)) {
                pIsSigned = true;
                if (match(TokenType::INT)) pType = "int";
                else if (match(TokenType::CHAR)) pType = "char";
                else pType = "int";
            }
            else if (match(TokenType::UNSIGNED)) {
                if (match(TokenType::INT)) pType = "int";
                else if (match(TokenType::CHAR)) pType = "char";
                else pType = "int";
            }
            else if (match(TokenType::INT)) pType = "int";
            else if (match(TokenType::CHAR)) pType = "char";
            else if (match(TokenType::STRUCT) || match(TokenType::UNION)) {
                bool isU = tokens[pos-1].type == TokenType::UNION;
                pType = (isU ? "union " : "struct ") + expect(TokenType::IDENTIFIER, "Expected struct/union name").value;
            }
            else {
                std::string foundStr = peek().value.empty() ? peek().typeToString() : peek().value;
                throw std::runtime_error("Syntax Error at " + std::to_string(peek().line) + ":" + std::to_string(peek().column) + ": Expected parameter type (int, char, struct, union). Found '" + foundStr + "' instead.");
            }

            int pPtrLevel = 0;
            while (match(TokenType::STAR)) pPtrLevel++;

            std::string pName = expect(TokenType::IDENTIFIER, "Expected parameter name").value;
            params.push_back({pType, pPtrLevel, pIsSigned, pName, pIsVolatile});
        } while (match(TokenType::COMMA));
    }
    expect(TokenType::CLOSE_PAREN, "Expected ')'");
    
    auto body = parseCompoundStatement();
    auto func = setPos(std::make_unique<FunctionDeclaration>(name, returnType), startToken);
    func->isSigned = isSigned;
    func->parameters = std::move(params);
    func->body = std::move(body);
    func->isNoreturn = isNR;
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
    if (peek().type == TokenType::IDENTIFIER && pos + 1 < tokens.size() && tokens[pos+1].type == TokenType::COLON) {
        std::string label = advance().value;
        advance(); // :
        return setPos(std::make_unique<LabelledStatement>(label, parseStatement()), tokens[pos-2]);
    }

    if (match(TokenType::GOTO)) {
        const Token& startToken = tokens[pos-1];
        std::string label = expect(TokenType::IDENTIFIER, "Expected label name after 'goto'").value;
        expect(TokenType::SEMICOLON, "Expected ';' after goto label");
        return setPos(std::make_unique<GotoStatement>(label), startToken);
    }

    bool isVolatile = false;
    bool isAuto = false;
    while (true) {
        if (match(TokenType::VOLATILE)) {
            isVolatile = true;
        } else if (match(TokenType::AUTO)) {
            isAuto = true;
        } else if (peek().type == TokenType::SIGNED || peek().type == TokenType::UNSIGNED) {
            break;
        } else {
            break;
        }
    }

    if (peek().type == TokenType::STRUCT || peek().type == TokenType::UNION) {
        bool isUnion = peek().type == TokenType::UNION;
        if (pos + 1 < tokens.size() && tokens[pos+1].type == TokenType::IDENTIFIER && pos + 2 < tokens.size() && tokens[pos+2].type == TokenType::OPEN_BRACE) {
            advance(); // struct/union
            return parseStructDefinition(isUnion);
        }
        return parseVariableDeclaration(isVolatile);
    }

    if (peek().type == TokenType::INT || peek().type == TokenType::CHAR || peek().type == TokenType::UNSIGNED || peek().type == TokenType::SIGNED) {
        return parseVariableDeclaration(isVolatile);
    }

    if (match(TokenType::RETURN)) {
        const Token& startToken = tokens[pos-1];
        std::unique_ptr<Expression> expr = nullptr;
        if (!match(TokenType::SEMICOLON)) {
            expr = parseExpression();
            expect(TokenType::SEMICOLON, "Expected ';' after return expression");
        }
        return setPos(std::make_unique<ReturnStatement>(std::move(expr)), startToken);
    }

    if (match(TokenType::BREAK)) {
        const Token& startToken = tokens[pos-1];
        expect(TokenType::SEMICOLON, "Expected ';' after break");
        return setPos(std::make_unique<BreakStatement>(), startToken);
    }

    if (match(TokenType::CONTINUE)) {
        const Token& startToken = tokens[pos-1];
        if (peek().type == TokenType::SEMICOLON) {
            advance();
            return setPos(std::make_unique<ContinueStatement>(), startToken);
        }
        // Handle continue <value> or continue default
        if (match(TokenType::DEFAULT)) {
            expect(TokenType::SEMICOLON, "Expected ';' after continue default");
            return setPos(std::make_unique<SwitchContinueStatement>(nullptr), startToken);
        }
        auto target = parseExpression();
        expect(TokenType::SEMICOLON, "Expected ';' after continue expression");
        return setPos(std::make_unique<SwitchContinueStatement>(std::move(target)), startToken);
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
            // Check if it's a declaration
            bool isDecl = false;
            if (peek().type == TokenType::INT || peek().type == TokenType::CHAR || 
                peek().type == TokenType::UNSIGNED || peek().type == TokenType::SIGNED ||
                peek().type == TokenType::STRUCT || peek().type == TokenType::UNION ||
                peek().type == TokenType::VOLATILE || peek().type == TokenType::AUTO) {
                isDecl = true;
            }

            if (isDecl) {
                bool isVolatile = false;
                while (match(TokenType::VOLATILE)) isVolatile = true;
                match(TokenType::AUTO); // consume auto if present
                initializer = parseVariableDeclaration(isVolatile);
                // parseVariableDeclaration expects and consumes a semicolon
            } else {
                auto expr = parseExpression();
                expect(TokenType::SEMICOLON, "Expected ';' after for initializer expression");
                initializer = setPos(std::make_unique<ExpressionStatement>(std::move(expr)), startToken);
            }
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

    if (match(TokenType::SWITCH)) {
        const Token& startToken = tokens[pos-1];
        expect(TokenType::OPEN_PAREN, "Expected '(' after 'switch'");
        auto expression = parseExpression();
        expect(TokenType::CLOSE_PAREN, "Expected ')' after switch expression");
        auto body = parseStatement();
        return setPos(std::make_unique<SwitchStatement>(std::move(expression), std::move(body)), startToken);
    }

    if (match(TokenType::CASE)) {
        const Token& startToken = tokens[pos-1];
        auto value = parseExpression();
        expect(TokenType::COLON, "Expected ':' after case value");
        return setPos(std::make_unique<CaseStatement>(std::move(value)), startToken);
    }

    if (match(TokenType::DEFAULT)) {
        const Token& startToken = tokens[pos-1];
        expect(TokenType::COLON, "Expected ':' after default");
        return setPos(std::make_unique<DefaultStatement>(), startToken);
    }

    if (match(TokenType::ASM)) {
        const Token& startToken = tokens[pos-1];
        expect(TokenType::OPEN_PAREN, "Expected '(' after asm");
        std::string code = expect(TokenType::STRING_LITERAL, "Expected string literal for asm code").value;
        expect(TokenType::CLOSE_PAREN, "Expected ')' after asm code");
        expect(TokenType::SEMICOLON, "Expected ';'");
        return setPos(std::make_unique<AsmStatement>(code), startToken);
    }

    if (peek().type == TokenType::_Static_assert) {
        return parseStaticAssert();
    }

    if (peek().type == TokenType::OPEN_BRACE) {
        return parseCompoundStatement();
    }
    
    const Token& startToken = peek();
    auto expr = parseExpression();
    expect(TokenType::SEMICOLON, "Expected ';'");
    return setPos(std::make_unique<ExpressionStatement>(std::move(expr)), startToken);
}

std::unique_ptr<Statement> Parser::parseVariableDeclaration(bool isVolatile) {
    std::unique_ptr<Expression> alignmentExpr = nullptr;
    if (match(TokenType::_Alignas)) {
        expect(TokenType::OPEN_PAREN, "Expected '(' after '_Alignas'");
        if (peek().type == TokenType::INT || peek().type == TokenType::CHAR || peek().type == TokenType::STRUCT || peek().type == TokenType::VOID) {
            std::string aType;
            if (match(TokenType::INT)) aType = "int";
            else if (match(TokenType::CHAR)) aType = "char";
            else if (match(TokenType::VOID)) aType = "void";
            else if (match(TokenType::STRUCT)) aType = "struct " + expect(TokenType::IDENTIFIER, "Expected struct name").value;
            int aPtr = 0;
            while (match(TokenType::STAR)) aPtr++;
            alignmentExpr = std::make_unique<AlignofExpression>(aType, aPtr);
        } else {
            alignmentExpr = parseExpression();
        }
        expect(TokenType::CLOSE_PAREN, "Expected ')' after alignment expression");
    }

    const Token& typeToken = peek();
    std::string type;
    bool isSigned = false;
    if (match(TokenType::SIGNED)) {
        isSigned = true;
        if (match(TokenType::INT)) type = "int";
        else if (match(TokenType::CHAR)) type = "char";
        else type = "int";
    }
    else if (match(TokenType::UNSIGNED)) {
        if (match(TokenType::INT)) type = "int";
        else if (match(TokenType::CHAR)) type = "char";
        else type = "int";
    }
    else if (match(TokenType::INT)) type = "int";
    else if (match(TokenType::CHAR)) type = "char";
    else if (match(TokenType::STRUCT) || match(TokenType::UNION)) {
        bool isU = tokens[pos-1].type == TokenType::UNION;
        type = (isU ? "union " : "struct ") + expect(TokenType::IDENTIFIER, "Expected struct/union name").value;
    } else {
        throw std::runtime_error("Syntax Error at " + std::to_string(peek().line) + ":" + std::to_string(peek().column) + ": Expected type for variable declaration. Found '" + peek().typeToString() + "' instead.");
    }

    int ptrLevel = 0;
    while (match(TokenType::STAR)) ptrLevel++;

    std::string name = expect(TokenType::IDENTIFIER, "Expected variable name").value;
    int arraySize = -1;
    if (match(TokenType::OPEN_SQUARE)) {
        const Token& sizeToken = expect(TokenType::INTEGER_LITERAL, "Expected integer literal for array size");
        arraySize = std::stoi(sizeToken.value);
        expect(TokenType::CLOSE_SQUARE, "Expected ']' after array size");
    }
    auto decl = setPos(std::make_unique<VariableDeclaration>(type, name, ptrLevel), typeToken);
    decl->isSigned = isSigned;
    decl->isVolatile = isVolatile;
    decl->alignmentExpr = std::move(alignmentExpr);
    decl->arraySize = arraySize;

    if (match(TokenType::EQUALS)) {
        decl->initializer = parseExpression();
    }
    expect(TokenType::SEMICOLON, "Expected ';'");
    return decl;
}

std::unique_ptr<StaticAssert> Parser::parseStaticAssert() {
    const Token& startToken = expect(TokenType::_Static_assert, "Expected '_Static_assert'");
    expect(TokenType::OPEN_PAREN, "Expected '(' after '_Static_assert'");
    auto condition = parseExpression();
    expect(TokenType::COMMA, "Expected ',' after condition in '_Static_assert'");
    std::string message = expect(TokenType::STRING_LITERAL, "Expected string literal for '_Static_assert' message").value;
    expect(TokenType::CLOSE_PAREN, "Expected ')' after '_Static_assert'");
    expect(TokenType::SEMICOLON, "Expected ';'");
    return setPos(std::make_unique<StaticAssert>(std::move(condition), message), startToken);
}

std::unique_ptr<StructDefinition> Parser::parseStructDefinition(bool isUnion) {
    const Token& startToken = tokens[pos-1]; // 'struct' or 'union'
    std::string name;
    if (peek().type == TokenType::IDENTIFIER) {
        name = advance().value;
    } else {
        name = (isUnion ? "<anon_union_" : "<anon_struct_") + std::to_string(anonymousAggregateCount++) + ">";
    }

    expect(TokenType::OPEN_BRACE, "Expected '{'");
    auto def = setPos(std::make_unique<StructDefinition>(name, isUnion), startToken);
    while (peek().type != TokenType::CLOSE_BRACE && peek().type != TokenType::END_OF_FILE) {
        std::unique_ptr<Expression> mAlignmentExpr = nullptr;
        if (match(TokenType::_Alignas)) {
            expect(TokenType::OPEN_PAREN, "Expected '(' after '_Alignas'");
            if (peek().type == TokenType::INT || peek().type == TokenType::CHAR || peek().type == TokenType::STRUCT || peek().type == TokenType::UNION || peek().type == TokenType::VOID) {
                std::string aType;
                if (match(TokenType::INT)) aType = "int";
                else if (match(TokenType::CHAR)) aType = "char";
                else if (match(TokenType::VOID)) aType = "void";
                else if (match(TokenType::STRUCT) || match(TokenType::UNION)) {
                    bool isU = tokens[pos-1].type == TokenType::UNION;
                    aType = (isU ? "union " : "struct ") + expect(TokenType::IDENTIFIER, "Expected struct/union name").value;
                }
                int aPtr = 0;
                while (match(TokenType::STAR)) aPtr++;
                mAlignmentExpr = std::make_unique<AlignofExpression>(aType, aPtr);
            } else {
                mAlignmentExpr = parseExpression();
            }
            expect(TokenType::CLOSE_PAREN, "Expected ')' after alignment expression");
        }

        if (peek().type == TokenType::STRUCT || peek().type == TokenType::UNION) {
            bool isNestedUnion = peek().type == TokenType::UNION;
            if (pos + 1 < tokens.size() && tokens[pos+1].type == TokenType::OPEN_BRACE) {
                // Anonymous nested struct/union
                advance(); // struct/union
                auto nestedDef = parseStructDefinition(isNestedUnion);
                std::string nestedTypeName = (isNestedUnion ? "union " : "struct ") + nestedDef->name;
                def->members.push_back({nestedTypeName, 0, false, "", 0, nullptr, true, -1});
                pendingDefinitions.push_back(std::move(nestedDef));
                continue; 
            }
        }

        std::string type;
        bool mIsSigned = false;
        if (match(TokenType::SIGNED)) {
            mIsSigned = true;
            if (match(TokenType::INT)) type = "int";
            else if (match(TokenType::CHAR)) type = "char";
            else type = "int";
        }
        else if (match(TokenType::UNSIGNED)) {
            if (match(TokenType::INT)) type = "int";
            else if (match(TokenType::CHAR)) type = "char";
            else type = "int";
        }
        else if (match(TokenType::INT)) type = "int";
        else if (match(TokenType::CHAR)) type = "char";
        else if (match(TokenType::STRUCT) || match(TokenType::UNION)) {
            bool isU = tokens[pos-1].type == TokenType::UNION;
            if (peek().type == TokenType::OPEN_BRACE) {
                // Inline definition: struct { ... } var;
                auto nestedDef = parseStructDefinition(isU);
                type = (isU ? "union " : "struct ") + nestedDef->name;
                pendingDefinitions.push_back(std::move(nestedDef));
            } else {
                type = (isU ? "union " : "struct ") + expect(TokenType::IDENTIFIER, "Expected struct/union name").value;
            }
        } else {
            throw std::runtime_error("Expected member type");
        }
        int ptrLevel = 0;
        while (match(TokenType::STAR)) ptrLevel++;
        std::string memberName = expect(TokenType::IDENTIFIER, "Expected member name").value;
        int arraySize = -1;
        if (match(TokenType::OPEN_SQUARE)) {
            const Token& sizeToken = expect(TokenType::INTEGER_LITERAL, "Expected integer literal for array size");
            arraySize = std::stoi(sizeToken.value);
            expect(TokenType::CLOSE_SQUARE, "Expected ']' after array size");
        }
        def->members.push_back({type, ptrLevel, mIsSigned, memberName, 0, std::move(mAlignmentExpr), false, arraySize});
        expect(TokenType::SEMICOLON, "Expected ';'");
    }
    expect(TokenType::CLOSE_BRACE, "Expected '}'");
    expect(TokenType::SEMICOLON, "Expected ';'");
    return def;
}

std::unique_ptr<Expression> Parser::parseExpression() {
    auto expr = parseConditional();
    if (match(TokenType::EQUALS) || match(TokenType::PLUS_EQUALS) || match(TokenType::MINUS_EQUALS) ||
        match(TokenType::STAR_EQUALS) || match(TokenType::SLASH_EQUALS) || match(TokenType::PERCENT_EQUALS) ||
        match(TokenType::AMPERSAND_EQUALS) || match(TokenType::PIPE_EQUALS) || match(TokenType::CARET_EQUALS) ||
        match(TokenType::LSHIFT_EQUALS) || match(TokenType::RSHIFT_EQUALS)) {
        const Token& opToken = tokens[pos-1];
        std::string op = opToken.value;
        return setPos(std::make_unique<Assignment>(std::move(expr), parseExpression(), op), opToken);
    }
    return expr;
}

std::unique_ptr<Expression> Parser::parseConditional() {
    auto expr = parseLogicalOr();
    if (match(TokenType::QUESTION_MARK)) {
        const Token& opToken = tokens[pos-1];
        auto thenExpr = parseExpression();
        expect(TokenType::COLON, "Expected ':' in conditional expression");
        auto elseExpr = parseConditional();
        return setPos(std::make_unique<ConditionalExpression>(std::move(expr), std::move(thenExpr), std::move(elseExpr)), opToken);
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
    while (match(TokenType::STAR) || match(TokenType::SLASH) || match(TokenType::PERCENT)) {
        const Token& opToken = tokens[pos-1];
        std::string op = opToken.value;
        auto right = parseUnary();
        left = setPos(std::make_unique<BinaryOperation>(op, std::move(left), std::move(right)), opToken);
    }
    return left;
}

std::unique_ptr<Expression> Parser::parseUnary() {
    if (match(TokenType::SIZEOF)) {
        const Token& startToken = tokens[pos-1];
        if (match(TokenType::OPEN_PAREN)) {
            // Check if it's a type or an expression
            if (peek().type == TokenType::INT || peek().type == TokenType::CHAR || 
                peek().type == TokenType::STRUCT || peek().type == TokenType::UNION ||
                peek().type == TokenType::SIGNED || peek().type == TokenType::UNSIGNED) {
                
                std::string type;
                if (match(TokenType::SIGNED) || match(TokenType::UNSIGNED)) {
                    if (match(TokenType::INT)) type = "int";
                    else if (match(TokenType::CHAR)) type = "char";
                    else type = "int";
                }
                else if (match(TokenType::INT)) type = "int";
                else if (match(TokenType::CHAR)) type = "char";
                else if (match(TokenType::STRUCT) || match(TokenType::UNION)) {
                    bool isU = tokens[pos-1].type == TokenType::UNION;
                    type = (isU ? "union " : "struct ") + expect(TokenType::IDENTIFIER, "Expected struct/union name").value;
                }
                
                int ptrLevel = 0;
                while (match(TokenType::STAR)) ptrLevel++;
                expect(TokenType::CLOSE_PAREN, "Expected ')' after type in sizeof");
                return setPos(std::make_unique<SizeofExpression>(type, ptrLevel), startToken);
            } else {
                auto expr = parseExpression();
                expect(TokenType::CLOSE_PAREN, "Expected ')' after expression in sizeof");
                return setPos(std::make_unique<SizeofExpression>(std::move(expr)), startToken);
            }
        } else {
            return setPos(std::make_unique<SizeofExpression>(parseUnary()), startToken);
        }
    }

    if (match(TokenType::BANG) || match(TokenType::TILDE) || match(TokenType::MINUS) || match(TokenType::STAR) || match(TokenType::AMPERSAND) ||
        match(TokenType::PLUS_PLUS) || match(TokenType::MINUS_MINUS)) {
        const Token& opToken = tokens[pos-1];
        std::string op = opToken.value;
        return setPos(std::make_unique<UnaryOperation>(op, parseUnary()), opToken);
    }
    return parsePrimary();
}

std::unique_ptr<Expression> Parser::parsePrimary() {
    std::unique_ptr<Expression> expr;
    if (match(TokenType::_GENERIC)) {
        return parseGenericSelection();
    }
    if (match(TokenType::_Alignof)) {
        expect(TokenType::OPEN_PAREN, "Expected '(' after '_Alignof'");
        std::string typeName;
        if (match(TokenType::INT)) typeName = "int";
        else if (match(TokenType::CHAR)) typeName = "char";
        else if (match(TokenType::VOID)) typeName = "void";
        else if (match(TokenType::STRUCT)) typeName = "struct " + expect(TokenType::IDENTIFIER, "Expected struct name").value;
        else throw std::runtime_error("Expected type name in _Alignof");
        
        int aPtrLevel = 0;
        while (match(TokenType::STAR)) aPtrLevel++;
        
        expect(TokenType::CLOSE_PAREN, "Expected ')' after _Alignof type");
        return setPos(std::make_unique<AlignofExpression>(typeName, aPtrLevel), tokens[pos-1]);
    }

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

    while (match(TokenType::DOT) || match(TokenType::ARROW) || match(TokenType::PLUS_PLUS) || match(TokenType::MINUS_MINUS) || match(TokenType::OPEN_SQUARE)) {
        const Token& opToken = tokens[pos-1];
        if (opToken.type == TokenType::PLUS_PLUS || opToken.type == TokenType::MINUS_MINUS) {
            expr = setPos(std::make_unique<UnaryOperation>(opToken.value + "_POST", std::move(expr)), opToken);
        } else if (opToken.type == TokenType::OPEN_SQUARE) {
            auto indexExpr = parseExpression();
            expect(TokenType::CLOSE_SQUARE, "Expected ']' after array index");
            expr = setPos(std::make_unique<ArrayAccess>(std::move(expr), std::move(indexExpr)), opToken);
        } else {
            bool isArrow = (opToken.type == TokenType::ARROW);
            std::string memberName = expect(TokenType::IDENTIFIER, "Expected member name").value;
            expr = setPos(std::make_unique<MemberAccess>(std::move(expr), memberName, isArrow), opToken);
        }
    }
    return expr;
}

void Parser::flushPending(TranslationUnit& unit) {
    for (auto& def : pendingDefinitions) {
        if (def) unit.topLevelDecls.push_back(std::move(def));
    }
    pendingDefinitions.clear();
}

std::unique_ptr<Expression> Parser::parseGenericSelection() {
    const Token& startToken = tokens[pos-1];
    expect(TokenType::OPEN_PAREN, "Expected '(' after '_Generic'");
    auto control = parseExpression();
    expect(TokenType::COMMA, "Expected ',' after controlling expression in _Generic");

    auto node = setPos(std::make_unique<GenericSelection>(std::move(control)), startToken);

    do {
        GenericAssociation assoc;
        if (match(TokenType::DEFAULT)) {
            assoc.isDefault = true;
        } else {
            if (match(TokenType::INT)) assoc.typeName = "int";
            else if (match(TokenType::CHAR)) assoc.typeName = "char";
            else if (match(TokenType::VOID)) assoc.typeName = "void";
            else if (match(TokenType::STRUCT) || match(TokenType::UNION)) {
                assoc.typeName = (tokens[pos-1].type == TokenType::STRUCT ? "struct " : "union ") + expect(TokenType::IDENTIFIER, "Expected aggregate name").value;
            } else {
                throw std::runtime_error("Expected type name in _Generic association");
            }
            while (match(TokenType::STAR)) assoc.pointerLevel++;
        }

        expect(TokenType::COLON, "Expected ':' in _Generic association");
        assoc.result = parseExpression();
        node->associations.push_back(std::move(assoc));

    } while (match(TokenType::COMMA));

    expect(TokenType::CLOSE_PAREN, "Expected ')' at end of _Generic");
    return node;
}
