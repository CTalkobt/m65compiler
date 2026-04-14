#pragma once
#include <string>

enum class AssemblerTokenType {
    // Basic
    IDENTIFIER,
    INSTRUCTION,
    DIRECTIVE,
    
    // Literals
    HEX_LITERAL,     // $1234
    DECIMAL_LITERAL, // 123
    BINARY_LITERAL,  // %1010
    STRING_LITERAL,  // "Hello"
    CHAR_LITERAL,    // 'A'
    
    // Symbols
    COLON,      // :
    COMMA,      // ,
    HASH,       // #
    OPEN_PAREN, // (
    CLOSE_PAREN,// )
    EQUALS,     // =
    PLUS,       // +
    MINUS,      // -
    STAR,       // *
    SLASH,      // /
    INCREMENT,  // ++
    DECREMENT,  // --
    
    // Special
    NEWLINE,
    END_OF_FILE,
    UNKNOWN
};

struct AssemblerToken {
    AssemblerTokenType type;
    std::string value;
    int line;
    int column;

    std::string typeToString() const {
        switch (type) {
            case AssemblerTokenType::IDENTIFIER: return "IDENTIFIER";
            case AssemblerTokenType::INSTRUCTION: return "INSTRUCTION";
            case AssemblerTokenType::DIRECTIVE: return "DIRECTIVE";
            case AssemblerTokenType::HEX_LITERAL: return "HEX_LITERAL";
            case AssemblerTokenType::DECIMAL_LITERAL: return "DECIMAL_LITERAL";
            case AssemblerTokenType::BINARY_LITERAL: return "BINARY_LITERAL";
            case AssemblerTokenType::STRING_LITERAL: return "STRING_LITERAL";
            case AssemblerTokenType::CHAR_LITERAL: return "CHAR_LITERAL";
            case AssemblerTokenType::COLON: return "COLON";
            case AssemblerTokenType::COMMA: return "COMMA";
            case AssemblerTokenType::HASH: return "HASH";
            case AssemblerTokenType::OPEN_PAREN: return "OPEN_PAREN";
            case AssemblerTokenType::CLOSE_PAREN: return "CLOSE_PAREN";
            case AssemblerTokenType::NEWLINE: return "NEWLINE";
            case AssemblerTokenType::END_OF_FILE: return "EOF";
            default: return "UNKNOWN";
        }
    }
};
