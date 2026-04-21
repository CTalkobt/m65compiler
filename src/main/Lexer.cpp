#include "Lexer.hpp"
#include <cctype>
#include <map>
#include <cstdint>

Lexer::Lexer(const std::string& source) : source(source), pos(0), line(1), column(1) {}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (true) {
        skipWhitespace();
        Token token = nextToken();
        tokens.push_back(token);
        if (token.type == TokenType::END_OF_FILE) {
            break;
        }
    }
    return tokens;
}

char Lexer::peek() const {
    if (pos >= source.length()) return '\0';
    return source[pos];
}

char Lexer::get() {
    if (pos >= source.length()) return '\0';
    char c = source[pos++];
    if (c == '\n') {
        line++;
        column = 1;
    } else {
        column++;
    }
    return c;
}

void Lexer::skipWhitespace() {
    while (true) {
        if (std::isspace(peek())) {
            get();
        } else if (peek() == '/' && pos + 1 < source.length() && source[pos + 1] == '/') {
            // Single-line comment
            while (peek() != '\n' && peek() != '\0') {
                get();
            }
        } else if (peek() == '/' && pos + 1 < source.length() && source[pos + 1] == '*') {
            // Multi-line comment
            get(); // skip /
            get(); // skip *
            while (peek() != '\0' && !(peek() == '*' && pos + 1 < source.length() && source[pos + 1] == '/')) {
                get();
            }
            if (peek() == '*') {
                get(); // skip *
                get(); // skip /
            }
        } else {
            break;
        }
    }
}

Token Lexer::nextToken() {
    char c = peek();
    if (c == '\0') {
        return {TokenType::END_OF_FILE, "", line, column};
    }

    if (std::isalpha(c) || c == '_') {
        return lexIdentifierOrKeyword();
    }

    if (std::isdigit(c)) {
        return lexNumber();
    }

    if (c == '"') {
        return lexString();
    }

    int startLine = line;
    int startCol = column;
    get();

    switch (c) {
        case '(': return {TokenType::OPEN_PAREN, "(", startLine, startCol};
        case ')': return {TokenType::CLOSE_PAREN, ")", startLine, startCol};
        case '{': return {TokenType::OPEN_BRACE, "{", startLine, startCol};
        case '}': return {TokenType::CLOSE_BRACE, "}", startLine, startCol};
        case ';': return {TokenType::SEMICOLON, ";", startLine, startCol};
        case ':': return {TokenType::COLON, ":", startLine, startCol};
        case ',': return {TokenType::COMMA, ",", startLine, startCol};
        case '.': return {TokenType::DOT, ".", startLine, startCol};
        case '=': 
            if (peek() == '=') { get(); return {TokenType::EQUALS_EQUALS, "==", startLine, startCol}; }
            return {TokenType::EQUALS, "=", startLine, startCol};
        case '+': 
            if (peek() == '+') { get(); return {TokenType::PLUS_PLUS, "++", startLine, startCol}; }
            return {TokenType::PLUS, "+", startLine, startCol};
        case '-': 
            if (peek() == '>') { get(); return {TokenType::ARROW, "->", startLine, startCol}; }
            if (peek() == '-') { get(); return {TokenType::MINUS_MINUS, "--", startLine, startCol}; }
            return {TokenType::MINUS, "-", startLine, startCol};
        case '*': return {TokenType::STAR, "*", startLine, startCol};
        case '/': return {TokenType::SLASH, "/", startLine, startCol};
        case '%': return {TokenType::PERCENT, "%", startLine, startCol};
        case '<':
            if (peek() == '=') { get(); return {TokenType::LESS_EQUAL, "<=", startLine, startCol}; }
            if (peek() == '<') { get(); return {TokenType::LSHIFT, "<<", startLine, startCol}; }
            return {TokenType::LESS_THAN, "<", startLine, startCol};
        case '>':
            if (peek() == '=') { get(); return {TokenType::GREATER_EQUAL, ">=", startLine, startCol}; }
            if (peek() == '>') { get(); return {TokenType::RSHIFT, ">>", startLine, startCol}; }
            return {TokenType::GREATER_THAN, ">", startLine, startCol};
        case '!':
            if (peek() == '=') { get(); return {TokenType::NOT_EQUALS, "!=", startLine, startCol}; }
            return {TokenType::BANG, "!", startLine, startCol};
        case '&':
            if (peek() == '&') { get(); return {TokenType::AND, "&&", startLine, startCol}; }
            return {TokenType::AMPERSAND, "&", startLine, startCol};
        case '|':
            if (peek() == '|') { get(); return {TokenType::OR, "||", startLine, startCol}; }
            return {TokenType::PIPE, "|", startLine, startCol};
        case '^': return {TokenType::CARET, "^", startLine, startCol};
        case '~': return {TokenType::TILDE, "~", startLine, startCol};
        default: return {TokenType::UNKNOWN, std::string(1, c), startLine, startCol};
    }
}

Token Lexer::lexIdentifierOrKeyword() {
    int startLine = line;
    int startCol = column;
    std::string value;
    while (std::isalnum(peek()) || peek() == '_') {
        value += get();
    }

    static const std::map<std::string, TokenType> keywords = {
        {"int", TokenType::INT},
        {"char", TokenType::CHAR},
        {"return", TokenType::RETURN},
        {"void", TokenType::VOID},
        {"if", TokenType::IF},
        {"else", TokenType::ELSE},
        {"while", TokenType::WHILE},
        {"for", TokenType::FOR},
        {"do", TokenType::DO},
        {"asm", TokenType::ASM},
        {"__asm__", TokenType::ASM},
        {"struct", TokenType::STRUCT},
        {"volatile", TokenType::VOLATILE},
        {"_Static_assert", TokenType::_Static_assert},
        {"break", TokenType::BREAK},
        {"continue", TokenType::CONTINUE},
        {"switch", TokenType::SWITCH},
        {"case", TokenType::CASE},
        {"default", TokenType::DEFAULT}
        };

    auto it = keywords.find(value);
    if (it != keywords.end()) {
        return {it->second, value, startLine, startCol};
    }

    return {TokenType::IDENTIFIER, value, startLine, startCol};
}

Token Lexer::lexNumber() {
    int startLine = line;
    int startCol = column;
    std::string value;
    if (peek() == '0') {
        value += get();
        if (peek() == 'x' || peek() == 'X') {
            value += get();
            while (std::isxdigit(peek())) value += get();
            // TODO: Add checks for integer overflow during parsing.
            uint32_t val = std::stoul(value.substr(2), nullptr, 16);
            return {TokenType::INTEGER_LITERAL, std::to_string(val), startLine, startCol};
        }
    }
    while (std::isdigit(peek())) {
        value += get();
    }
    return {TokenType::INTEGER_LITERAL, value, startLine, startCol};
}

Token Lexer::lexString() {
    int startLine = line;
    int startCol = column;
    get(); // skip opening quote
    std::string value;
    while (peek() != '"' && peek() != '\0') {
        if (peek() == '\\') {
            get();
            char next = peek();
            switch (next) {
                case 'n': value += '\n'; get(); break;
                case 'r': value += '\r'; get(); break;
                case 't': value += '\t'; get(); break;
                default: value += get(); break;
            }
        } else {
            value += get();
        }
    }
    if (peek() == '"') get(); // skip closing quote
    return {TokenType::STRING_LITERAL, value, startLine, startCol};
}
