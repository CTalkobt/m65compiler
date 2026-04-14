#pragma once
#include <string>

enum class AssemblerTokenType {
    IDENTIFIER, INSTRUCTION, DIRECTIVE,
    HEX_LITERAL, DECIMAL_LITERAL, BINARY_LITERAL, STRING_LITERAL, CHAR_LITERAL,
    COLON, COMMA, HASH, OPEN_PAREN, CLOSE_PAREN, OPEN_BRACKET, CLOSE_BRACKET,
    EQUALS, PLUS, MINUS, STAR, SLASH, INCREMENT, DECREMENT,
    NEWLINE, END_OF_FILE, UNKNOWN
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
            case AssemblerTokenType::OPEN_BRACKET: return "OPEN_BRACKET";
            case AssemblerTokenType::CLOSE_BRACKET: return "CLOSE_BRACKET";
            case AssemblerTokenType::EQUALS: return "EQUALS";
            case AssemblerTokenType::PLUS: return "PLUS";
            case AssemblerTokenType::MINUS: return "MINUS";
            case AssemblerTokenType::STAR: return "STAR";
            case AssemblerTokenType::SLASH: return "SLASH";
            case AssemblerTokenType::INCREMENT: return "INCREMENT";
            case AssemblerTokenType::DECREMENT: return "DECREMENT";
            case AssemblerTokenType::NEWLINE: return "NEWLINE";
            case AssemblerTokenType::END_OF_FILE: return "EOF";
            default: return "UNKNOWN";
        }
    }
};
