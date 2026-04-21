#pragma once
#include <string>

enum class TokenType {
    // Keywords
    INT,
    CHAR,
    RETURN,
    VOID,
    IF,
    ELSE,
    WHILE,
    FOR,
    ASM,
    DO,
    STRUCT,
    VOLATILE,
    _Static_assert,
    _Alignas,
    _Alignof,
    BREAK,
    CONTINUE,
    SWITCH,
    CASE,
    DEFAULT,
    
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
    COLON,         // :
    COMMA,         // ,
    EQUALS,        // =
    PLUS,          // +
    MINUS,         // -
    STAR,          // *
    SLASH,         // /
    PERCENT,       // %
    LESS_THAN,     // <
    GREATER_THAN,  // >
    LESS_EQUAL,    // <=
    GREATER_EQUAL, // >=
    EQUALS_EQUALS, // ==
    NOT_EQUALS,    // !=
    BANG,          // !
    AMPERSAND,     // &
    PIPE,          // |
    CARET,         // ^
    TILDE,         // ~
    LSHIFT,        // <<
    RSHIFT,        // >>
    AND,           // &&
    OR,            // ||
    DOT,           // .
    ARROW,         // ->
    PLUS_PLUS,     // ++
    MINUS_MINUS,   // --
    
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
            case TokenType::CHAR: return "CHAR";
            case TokenType::RETURN: return "RETURN";
            case TokenType::VOID: return "VOID";
            case TokenType::IF: return "IF";
            case TokenType::ELSE: return "ELSE";
            case TokenType::WHILE: return "WHILE";
            case TokenType::FOR: return "FOR";
            case TokenType::ASM: return "ASM";
            case TokenType::DO: return "DO";
            case TokenType::STRUCT: return "STRUCT";
            case TokenType::VOLATILE: return "VOLATILE";
            case TokenType::_Static_assert: return "_Static_assert";
            case TokenType::_Alignas: return "_Alignas";
            case TokenType::_Alignof: return "_Alignof";
            case TokenType::BREAK: return "BREAK";
            case TokenType::CONTINUE: return "CONTINUE";
            case TokenType::SWITCH: return "SWITCH";
            case TokenType::CASE: return "CASE";
            case TokenType::DEFAULT: return "DEFAULT";
            case TokenType::IDENTIFIER: return "IDENTIFIER";
            case TokenType::INTEGER_LITERAL: return "INTEGER_LITERAL";
            case TokenType::STRING_LITERAL: return "STRING_LITERAL";
            case TokenType::OPEN_PAREN: return "OPEN_PAREN";
            case TokenType::CLOSE_PAREN: return "CLOSE_PAREN";
            case TokenType::OPEN_BRACE: return "OPEN_BRACE";
            case TokenType::CLOSE_BRACE: return "CLOSE_BRACE";
            case TokenType::SEMICOLON: return "SEMICOLON";
            case TokenType::COLON: return "COLON";
            case TokenType::COMMA: return "COMMA";
            case TokenType::EQUALS: return "EQUALS";
            case TokenType::PLUS: return "PLUS";
            case TokenType::MINUS: return "MINUS";
            case TokenType::STAR: return "STAR";
            case TokenType::SLASH: return "SLASH";
            case TokenType::PERCENT: return "PERCENT";
            case TokenType::LESS_THAN: return "LESS_THAN";
            case TokenType::GREATER_THAN: return "GREATER_THAN";
            case TokenType::LESS_EQUAL: return "LESS_EQUAL";
            case TokenType::GREATER_EQUAL: return "GREATER_EQUAL";
            case TokenType::EQUALS_EQUALS: return "EQUALS_EQUALS";
            case TokenType::NOT_EQUALS: return "NOT_EQUALS";
            case TokenType::BANG: return "BANG";
            case TokenType::AMPERSAND: return "AMPERSAND";
            case TokenType::PIPE: return "PIPE";
            case TokenType::CARET: return "CARET";
            case TokenType::TILDE: return "TILDE";
            case TokenType::LSHIFT: return "LSHIFT";
            case TokenType::RSHIFT: return "RSHIFT";
            case TokenType::AND: return "AND";
            case TokenType::OR: return "OR";
            case TokenType::DOT: return "DOT";
            case TokenType::ARROW: return "ARROW";
            case TokenType::PLUS_PLUS: return "PLUS_PLUS";
            case TokenType::MINUS_MINUS: return "MINUS_MINUS";
            case TokenType::END_OF_FILE: return "EOF";
            default: return "UNKNOWN";
        }
    }
};
