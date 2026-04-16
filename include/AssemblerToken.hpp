#pragma once
#include <string>

enum class AssemblerTokenType {
    IDENTIFIER, INSTRUCTION, DIRECTIVE, REGISTER, FLAG,
    HEX_LITERAL, DECIMAL_LITERAL, BINARY_LITERAL, STRING_LITERAL, CHAR_LITERAL,
    COLON, COMMA, HASH, OPEN_PAREN, CLOSE_PAREN, OPEN_BRACKET, CLOSE_BRACKET,
    EQUALS, PLUS, MINUS, STAR, SLASH, INCREMENT, DECREMENT,
    AMPERSAND, PIPE, CARET, TILDE, BANG, LSHIFT, RSHIFT, AND, OR,
    EQUALS_EQUALS, NOT_EQUALS, LESS_THAN, GREATER_THAN, LESS_EQUAL, GREATER_EQUAL,
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
            case AssemblerTokenType::REGISTER: return "REGISTER";
            case AssemblerTokenType::FLAG: return "FLAG";
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
            case AssemblerTokenType::AMPERSAND: return "AMPERSAND";
            case AssemblerTokenType::PIPE: return "PIPE";
            case AssemblerTokenType::CARET: return "CARET";
            case AssemblerTokenType::TILDE: return "TILDE";
            case AssemblerTokenType::BANG: return "BANG";
            case AssemblerTokenType::LSHIFT: return "LSHIFT";
            case AssemblerTokenType::RSHIFT: return "RSHIFT";
            case AssemblerTokenType::AND: return "AND";
            case AssemblerTokenType::OR: return "OR";
            case AssemblerTokenType::EQUALS_EQUALS: return "EQUALS_EQUALS";
            case AssemblerTokenType::NOT_EQUALS: return "NOT_EQUALS";
            case AssemblerTokenType::LESS_THAN: return "LESS_THAN";
            case AssemblerTokenType::GREATER_THAN: return "GREATER_THAN";
            case AssemblerTokenType::LESS_EQUAL: return "LESS_EQUAL";
            case AssemblerTokenType::GREATER_EQUAL: return "GREATER_EQUAL";
            case AssemblerTokenType::NEWLINE: return "NEWLINE";
            case AssemblerTokenType::END_OF_FILE: return "EOF";
            default: return "UNKNOWN";
        }
    }
};
