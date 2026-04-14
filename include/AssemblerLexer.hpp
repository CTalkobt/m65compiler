#pragma once
#include <vector>
#include <string>
#include "AssemblerToken.hpp"

class AssemblerLexer {
public:
    AssemblerLexer(const std::string& source);
    std::vector<AssemblerToken> tokenize();

private:
    std::string source;
    size_t pos;
    int line;
    int column;

    char peek() const;
    char get();
    void skipWhitespaceAndComments();
    AssemblerToken nextToken();
    
    AssemblerToken lexIdentifierOrInstruction();
    AssemblerToken lexNumber();
    AssemblerToken lexString();
    AssemblerToken lexChar();
};
