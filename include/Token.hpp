#pragma once
#include <string>

enum class TokenType {
    // Keywords
    INT,
    RETURN,
    VOID,
    
    // Literals
    IDENTIFIER,
    INTEGER_LITERAL,
    STRING_LITERAL,
    
    // Punctuators
    OPEN_PAREN,    // (
    CLOSE_PAREN,   // )
    OPEN_BRACE,    // {
    CLOSE_BRACE,   // }
    SEMICOLON,     // ;
    COMMA,         // ,
    EQUALS,        // =
    PLUS,          // +
    MINUS,         // -
    STAR,          // *
    SLASH,         // /
    
    // Special
    END_OF_FILE,
    UNKNOWN
};

struct Token {
    TokenType type;
    std::string value;
    int line;
    int column;

    std::string typeToString() const {
        switch (type) {
            case TokenType::INT: return "INT";
            case TokenType::RETURN: return "RETURN";
            case TokenType::VOID: return "VOID";
            case TokenType::IDENTIFIER: return "IDENTIFIER";
            case TokenType::INTEGER_LITERAL: return "INTEGER_LITERAL";
            case TokenType::STRING_LITERAL: return "STRING_LITERAL";
            case TokenType::OPEN_PAREN: return "OPEN_PAREN";
            case TokenType::CLOSE_PAREN: return "CLOSE_PAREN";
            case TokenType::OPEN_BRACE: return "OPEN_BRACE";
            case TokenType::CLOSE_BRACE: return "CLOSE_BRACE";
            case TokenType::SEMICOLON: return "SEMICOLON";
            case TokenType::COMMA: return "COMMA";
            case TokenType::END_OF_FILE: return "EOF";
            default: return "UNKNOWN";
        }
    }
};
