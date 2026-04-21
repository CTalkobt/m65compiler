#pragma once
#include <vector>
#include <string>
#include "Token.hpp"

class Lexer {
public:
    Lexer(const std::string& source);
    std::vector<Token> tokenize();

private:
    std::string source;
    size_t pos;
    int line;
    int column;

    char peek() const;
    char get();
    void skipWhitespace();
    Token nextToken();
    
    Token lexIdentifierOrKeyword();
    Token lexNumber();
    Token lexString();
    Token lexChar();
};
