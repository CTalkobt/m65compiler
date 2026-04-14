#include "AssemblerLexer.hpp"
#include <cctype>
#include <set>
#include <algorithm>

AssemblerLexer::AssemblerLexer(const std::string& source) : source(source), pos(0), line(1), column(1) {}

std::vector<AssemblerToken> AssemblerLexer::tokenize() {
    std::vector<AssemblerToken> tokens;
    while (true) {
        skipWhitespaceAndComments();
        AssemblerToken token = nextToken();
        tokens.push_back(token);
        if (token.type == AssemblerTokenType::END_OF_FILE) {
            break;
        }
    }
    return tokens;
}

char AssemblerLexer::peek() const {
    if (pos >= source.length()) return '\0';
    return source[pos];
}

char AssemblerLexer::get() {
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

void AssemblerLexer::skipWhitespaceAndComments() {
    while (true) {
        if (peek() == ' ' || peek() == '\t' || peek() == '\r') {
            get();
        } else if (peek() == ';') {
            // Comment
            while (peek() != '\n' && peek() != '\0') {
                get();
            }
        } else {
            break;
        }
    }
}

AssemblerToken AssemblerLexer::nextToken() {
    char c = peek();
    if (c == '\0') {
        return {AssemblerTokenType::END_OF_FILE, "", line, column};
    }

    if (c == '\n') {
        int startLine = line;
        int startCol = column;
        get();
        return {AssemblerTokenType::NEWLINE, "\\n", startLine, startCol};
    }

    if (std::isalpha(c) || c == '_') {
        return lexIdentifierOrInstruction();
    }

    if (c == '.') {
        int startLine = line;
        int startCol = column;
        get();
        std::string directive;
        while (std::isalnum(peek()) || peek() == '_') {
            directive += get();
        }
        return {AssemblerTokenType::DIRECTIVE, directive, startLine, startCol};
    }

    if (std::isdigit(c) || c == '$' || c == '%') {
        return lexNumber();
    }

    if (c == '"') {
        return lexString();
    }

    if (c == '\'') {
        return lexChar();
    }

    int startLine = line;
    int startCol = column;
    get();

    switch (c) {
        case ':': return {AssemblerTokenType::COLON, ":", startLine, startCol};
        case ',': return {AssemblerTokenType::COMMA, ",", startLine, startCol};
        case '#': return {AssemblerTokenType::HASH, "#", startLine, startCol};
        case '=': return {AssemblerTokenType::EQUALS, "=", startLine, startCol};
        case '+':
            if (pos < source.length() && source[pos] == '+') { get(); return {AssemblerTokenType::INCREMENT, "++", startLine, startCol}; }
            return {AssemblerTokenType::PLUS, "+", startLine, startCol};
        case '-':
            if (pos < source.length() && source[pos] == '-') { get(); return {AssemblerTokenType::DECREMENT, "--", startLine, startCol}; }
            return {AssemblerTokenType::MINUS, "-", startLine, startCol};
        case '*': return {AssemblerTokenType::STAR, "*", startLine, startCol};
        case '/': return {AssemblerTokenType::SLASH, "/", startLine, startCol};
        case '(': return {AssemblerTokenType::OPEN_PAREN, "(", startLine, startCol};
        case ')': return {AssemblerTokenType::CLOSE_PAREN, ")", startLine, startCol};
        case '[': return {AssemblerTokenType::OPEN_BRACKET, "[", startLine, startCol};
        case ']': return {AssemblerTokenType::CLOSE_BRACKET, "]", startLine, startCol};
        default: return {AssemblerTokenType::UNKNOWN, std::string(1, c), startLine, startCol};
    }
}

AssemblerToken AssemblerLexer::lexIdentifierOrInstruction() {
    int startLine = line;
    int startCol = column;
    std::string value;
    while (std::isalnum(peek()) || peek() == '_') {
        value += get();
    }

    static const std::set<std::string> instructions = {
        "LDA", "LDX", "LDY", "LDZ", "STA", "STX", "STY", "STZ", "STQ", "LDQ",
        "JSR", "RTS", "RTN", "PHW", "PHZ", "PLZ", "PHX", "PLX", "PHY", "PLY",
        "BEQ", "BNE", "BRA", "BCC", "BCS", "BPL", "BMI", "BVC", "BVS", "BSR",
        "INX", "INY", "INZ", "DEX", "DEY", "DEZ", "INW", "DEW", "ASW", "ROW",
        "CLC", "SEC", "CLI", "SEI", "CLD", "SED", "CLE", "SEE",
        "CALL", "ENDCALL", "PROC", "ENDPROC", "NEG", "ASR"
    };

    std::string upperValue = value;
    std::transform(upperValue.begin(), upperValue.end(), upperValue.begin(), ::toupper);

    if (instructions.count(upperValue)) {
        return {AssemblerTokenType::INSTRUCTION, upperValue, startLine, startCol};
    }

    return {AssemblerTokenType::IDENTIFIER, value, startLine, startCol};
}

AssemblerToken AssemblerLexer::lexNumber() {
    int startLine = line;
    int startCol = column;
    std::string value;
    char prefix = peek();
    
    if (prefix == '$') {
        get();
        while (std::isxdigit(peek())) value += get();
        return {AssemblerTokenType::HEX_LITERAL, value, startLine, startCol};
    }
    
    if (prefix == '%') {
        get();
        while (peek() == '0' || peek() == '1') value += get();
        return {AssemblerTokenType::BINARY_LITERAL, value, startLine, startCol};
    }

    while (std::isdigit(peek())) value += get();
    return {AssemblerTokenType::DECIMAL_LITERAL, value, startLine, startCol};
}

AssemblerToken AssemblerLexer::lexString() {
    int startLine = line;
    int startCol = column;
    get(); // skip "
    std::string value;
    while (peek() != '"' && peek() != '\0') {
        value += get();
    }
    if (peek() == '"') get();
    return {AssemblerTokenType::STRING_LITERAL, value, startLine, startCol};
}

AssemblerToken AssemblerLexer::lexChar() {
    int startLine = line;
    int startCol = column;
    get(); // skip '
    std::string value;
    if (peek() != '\0' && peek() != '\'') {
        value += get();
    }
    if (peek() == '\'') get();
    return {AssemblerTokenType::CHAR_LITERAL, value, startLine, startCol};
}
