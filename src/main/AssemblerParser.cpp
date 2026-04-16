#include "AssemblerParser.hpp"
#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <memory>
#include <cmath>

static uint8_t toPetscii(char c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    if (c >= 'A' && c <= 'Z') return c + 32;
    return (uint8_t)c;
}

static std::vector<uint8_t> encodeFloat(double val) {
    std::vector<uint8_t> result(5, 0);
    if (val == 0.0) return result;
    bool negative = false;
    if (val < 0) { negative = true; val = -val; }
    int exp;
    double mantissa = std::frexp(val, &exp);
    result[0] = (uint8_t)(exp + 128);
    uint32_t m = (uint32_t)(mantissa * 4294967296.0);
    result[1] = (m >> 24) & 0x7F;
    if (negative) result[1] |= 0x80;
    result[2] = (m >> 16) & 0xFF;
    result[3] = (m >> 8) & 0xFF;
    result[4] = m & 0xFF;
    return result;
}

struct ExprAST {
    virtual ~ExprAST() = default;
    virtual uint32_t getValue() const = 0;
    virtual bool isConstant() const = 0;
    virtual bool is16Bit() const = 0;
    virtual void emit(std::vector<uint8_t>& binary, AssemblerParser* parser, int width, const std::string& target) = 0;
};

struct ConstantNode : public ExprAST {
    uint32_t value;
    ConstantNode(uint32_t v) : value(v) {}
    uint32_t getValue() const override { return value; }
    bool isConstant() const override { return true; }
    bool is16Bit() const override { return value > 0xFF; }
    void emit(std::vector<uint8_t>& binary, AssemblerParser* parser, int width, const std::string& target) override {
        binary.push_back(0xA9); binary.push_back(value & 0xFF);
        if (width >= 16) { binary.push_back(0xA2); binary.push_back((value >> 8) & 0xFF); }
    }
};

struct RegisterNode : public ExprAST {
    std::string name;
    RegisterNode(const std::string& n) : name(n) {}
    uint32_t getValue() const override { return 0; }
    bool isConstant() const override { return false; }
    bool is16Bit() const override { return name.size() > 1; }
    void emit(std::vector<uint8_t>& binary, AssemblerParser* parser, int width, const std::string& target) override {
        if (name == ".X") binary.push_back(0x8A); else if (name == ".Y") binary.push_back(0x98); else if (name == ".Z") binary.push_back(0x6B);
    }
};

struct FlagNode : public ExprAST {
    char flag;
    FlagNode(char f) : flag(f) {}
    uint32_t getValue() const override { return 0; }
    bool isConstant() const override { return false; }
    bool is16Bit() const override { return false; }
    void emit(std::vector<uint8_t>& binary, AssemblerParser* parser, int width, const std::string& target) override {
        binary.push_back(0xA9); binary.push_back(0); 
    }
};

struct BinaryExpr : public ExprAST {
    std::string op;
    std::unique_ptr<ExprAST> left, right;
    BinaryExpr(std::string o, std::unique_ptr<ExprAST> l, std::unique_ptr<ExprAST> r)
        : op(o), left(std::move(l)), right(std::move(r)) {}
    uint32_t getValue() const override {
        uint32_t l = left ? left->getValue() : 0; uint32_t r = right ? right->getValue() : 0;
        if (op == "+") return l + r; if (op == "-") return l - r; if (op == "*") return l * r; if (op == "/") return r != 0 ? l / r : 0;
        return 0;
    }
    bool isConstant() const override { return (left ? left->isConstant() : true) && (right ? right->isConstant() : true); }
    bool is16Bit() const override { return getValue() > 0xFF || (left && left->is16Bit()) || (right && right->is16Bit()); }
    void emit(std::vector<uint8_t>& binary, AssemblerParser* parser, int width, const std::string& target) override {
        if (!left || !right) return;
        if (op == "*" || op == "/") {
            left->emit(binary, parser, width, ".A");
            binary.push_back(0x8D); binary.push_back(0x70); binary.push_back(0xD7);
            if (width >= 16) { binary.push_back(0x8E); binary.push_back(0x71); binary.push_back(0xD7); }
            right->emit(binary, parser, width, ".A");
            binary.push_back(0x8D); binary.push_back(0x74); binary.push_back(0xD7);
            if (width >= 16) { binary.push_back(0x8E); binary.push_back(0x75); binary.push_back(0xD7); }
            if (op == "/") {
                binary.push_back(0x24); binary.push_back(0x0F); binary.push_back(0xD7);
                binary.push_back(0x30); binary.push_back(0xFB);
                binary.push_back(0xAD); binary.push_back(0x6C); binary.push_back(0xD7);
                if (width >= 16) { binary.push_back(0xAE); binary.push_back(0x6D); binary.push_back(0xD7); }
            } else {
                binary.push_back(0xAD); binary.push_back(0x78); binary.push_back(0xD7);
                if (width >= 16) { binary.push_back(0xAE); binary.push_back(0x79); binary.push_back(0xD7); }
            }
        } else if (op == "+" || op == "-") {
            left->emit(binary, parser, width, ".A");
            if (right->isConstant()) {
                uint32_t val = right->getValue();
                if (op == "+") binary.push_back(0x18); else binary.push_back(0x38);
                binary.push_back(op == "+" ? 0x69 : 0xE9); binary.push_back(val & 0xFF);
                if (width >= 16) {
                    binary.push_back(0x48); binary.push_back(0x8A);
                    binary.push_back(op == "+" ? 0x69 : 0xE9); binary.push_back((val >> 8) & 0xFF);
                    binary.push_back(0xAA); binary.push_back(0x68);
                }
            }
        }
    }
};

AssemblerParser::AssemblerParser(const std::vector<AssemblerToken>& tokens) : tokens(tokens), pos(0), pc(0) {}
const AssemblerToken& AssemblerParser::peek() const { if (pos >= tokens.size()) return tokens.back(); return tokens[pos]; }
const AssemblerToken& AssemblerParser::advance() { if (pos < tokens.size()) pos++; return tokens[pos - 1]; }
bool AssemblerParser::match(AssemblerTokenType type) { if (peek().type == type) { advance(); return true; } return false; }
const AssemblerToken& AssemblerParser::expect(AssemblerTokenType type, const std::string& message) { if (peek().type == type) return advance(); throw std::runtime_error(message + " at " + std::to_string(peek().line) + ":" + std::to_string(peek().column)); }

static uint32_t parseNumericLiteral(const std::string& literal) {
    if (literal.empty()) return 0;
    try {
        if (literal[0] == '$') return std::stoul(literal.substr(1), nullptr, 16);
        if (literal[0] == '%') return std::stoul(literal.substr(1), nullptr, 2);
        return std::stoul(literal);
    } catch (...) { return 0; }
}

std::unique_ptr<ExprAST> parseExprAST(const std::vector<AssemblerToken>& tokens, int& idx, std::map<std::string, Symbol>& symbolTable) {
    auto parsePrimary = [&]() -> std::unique_ptr<ExprAST> {
        if (idx >= (int)tokens.size()) return nullptr;
        const auto& t = tokens[idx++];
        if (t.type == AssemblerTokenType::DECIMAL_LITERAL) return std::make_unique<ConstantNode>(std::stoul(t.value));
        if (t.type == AssemblerTokenType::HEX_LITERAL) return std::make_unique<ConstantNode>(parseNumericLiteral(t.value));
        if (t.type == AssemblerTokenType::BINARY_LITERAL) return std::make_unique<ConstantNode>(parseNumericLiteral(t.value));
        if (t.type == AssemblerTokenType::REGISTER) return std::make_unique<RegisterNode>(t.value);
        if (t.type == AssemblerTokenType::FLAG) return std::make_unique<FlagNode>(t.value[0]);
        if (t.type == AssemblerTokenType::IDENTIFIER) { if (symbolTable.count(t.value)) return std::make_unique<ConstantNode>(symbolTable[t.value].value); return std::make_unique<ConstantNode>(0); }
        return nullptr;
    };
    auto left = parsePrimary();
    while (idx < (int)tokens.size() && (tokens[idx].type == AssemblerTokenType::PLUS || tokens[idx].type == AssemblerTokenType::MINUS || tokens[idx].type == AssemblerTokenType::STAR || tokens[idx].type == AssemblerTokenType::SLASH)) {
        std::string op = tokens[idx++].value; auto right = parsePrimary(); left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
    }
    return left;
}

uint32_t AssemblerParser::evaluateExpressionAt(int index) {
    int idx = index; auto ast = parseExprAST(tokens, idx, symbolTable);
    if (!ast) return 0; return ast->getValue();
}

void AssemblerParser::pass1() {
    pc = 0; pos = 0; statements.clear();
    while (pos < tokens.size()) {
        while (match(AssemblerTokenType::NEWLINE));
        if (peek().type == AssemblerTokenType::END_OF_FILE) break;
        auto stmt = std::make_unique<Statement>();
        stmt->address = pc; stmt->line = peek().line;
        if (peek().type == AssemblerTokenType::IDENTIFIER && pos + 1 < tokens.size() && tokens[pos+1].type == AssemblerTokenType::COLON) {
            stmt->label = advance().value; advance(); symbolTable[stmt->label] = {pc, true, 2};
        }
        if (match(AssemblerTokenType::DIRECTIVE)) {
            stmt->type = Statement::DIRECTIVE; stmt->dir.name = tokens[pos-1].value;
            if (stmt->dir.name == "var") {
                std::string varName = expect(AssemblerTokenType::IDENTIFIER, "Expected var name").value;
                stmt->dir.varName = varName;
                if (match(AssemblerTokenType::EQUALS)) {
                    stmt->dir.varType = Directive::ASSIGN; stmt->dir.tokenIndex = (int)pos;
                    uint32_t val = evaluateExpressionAt((int)pos);
                    while (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) advance();
                    symbolTable[varName] = {val, false, 2, true, val};
                } else if (match(AssemblerTokenType::INCREMENT)) { stmt->dir.varType = Directive::INC; if (symbolTable.count(varName)) symbolTable[varName].value++; }
                else if (match(AssemblerTokenType::DECREMENT)) { stmt->dir.varType = Directive::DEC; if (symbolTable.count(varName)) symbolTable[varName].value--; }
                stmt->size = 0;
            } else if (stmt->dir.name == "cleanup") {
                stmt->dir.tokenIndex = (int)pos; uint32_t val = evaluateExpressionAt((int)pos);
                while (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) advance();
                if (currentProc) currentProc->totalParamSize += val; stmt->size = 0;
            } else if (stmt->dir.name == "basicUpstart") { stmt->type = Statement::BASIC_UPSTART; stmt->basicUpstartTokenIndex = (int)pos; while (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) advance(); stmt->size = 12; }
            else {
                while (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) {
                    if (peek().type == AssemblerTokenType::COMMA) { advance(); continue; }
                    std::string val = advance().value; if (!val.empty()) stmt->dir.arguments.push_back(val);
                }
                if (stmt->dir.name == "org") { stmt->address = pc; if (!stmt->dir.arguments.empty()) pc = parseNumericLiteral(stmt->dir.arguments[0]); stmt->size = 0; }
                else if (stmt->dir.name == "cpu") stmt->size = 0;
                else stmt->size = calculateDirectiveSize(stmt->dir);
            }
        } else if (peek().type == AssemblerTokenType::STAR && pos + 1 < tokens.size() && tokens[pos+1].type == AssemblerTokenType::EQUALS) {
            advance(); advance(); stmt->type = Statement::DIRECTIVE; stmt->dir.name = "org"; stmt->address = pc;
            while (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) { if (peek().type == AssemblerTokenType::COMMA) { advance(); continue; } stmt->dir.arguments.push_back(advance().value); }
            if (!stmt->dir.arguments.empty()) pc = parseNumericLiteral(stmt->dir.arguments[0]); stmt->size = 0;
        } else if (match(AssemblerTokenType::INSTRUCTION)) {
            stmt->type = Statement::INSTRUCTION; stmt->instr.mnemonic = tokens[pos-1].value;
            std::transform(stmt->instr.mnemonic.begin(), stmt->instr.mnemonic.end(), stmt->instr.mnemonic.begin(), ::toupper);
            if (stmt->instr.mnemonic == "PROC") {
                std::string procName = advance().value; stmt->label = procName; symbolTable[procName] = {pc, true, 2};
                ProcContext ctx; ctx.name = procName; ctx.totalParamSize = 0; std::vector<std::pair<std::string, int>> args;
                while (match(AssemblerTokenType::COMMA)) {
                    bool isByte = false; if (peek().type == AssemblerTokenType::IDENTIFIER && (peek().value == "B" || peek().value == "W")) { isByte = (advance().value == "B"); match(AssemblerTokenType::HASH); }
                    std::string argName = advance().value; int size = isByte ? 1 : 2; args.push_back({argName, size}); ctx.totalParamSize += size;
                }
                int currentOffset = 2;
                for (int i = (int)args.size() - 1; i >= 0; --i) {
                    ctx.localArgs[args[i].first] = currentOffset; ctx.localArgs["ARG" + std::to_string(i + 1)] = currentOffset;
                    symbolTable[args[i].first] = {(uint32_t)currentOffset, false, (int)args[i].second, true, (uint32_t)currentOffset};
                    symbolTable["ARG" + std::to_string(i + 1)] = {(uint32_t)currentOffset, false, (int)args[i].second, true, (uint32_t)currentOffset};
                    currentOffset += args[i].second;
                }
                auto res = procedures.emplace(pc, ctx); currentProc = &res.first->second; stmt->size = 0;
            } else if (stmt->instr.mnemonic == "ENDPROC") { if (currentProc) { stmt->instr.procParamSize = currentProc->totalParamSize; currentProc = nullptr; } stmt->size = 2; }
            else if (stmt->instr.mnemonic == "CALL") {
                stmt->instr.operand = advance().value;
                while (match(AssemblerTokenType::COMMA)) {
                    if (match(AssemblerTokenType::HASH)) stmt->instr.callArgs.push_back(std::string("#") + advance().value);
                    else if (peek().type == AssemblerTokenType::IDENTIFIER && (peek().value == "B" || peek().value == "W")) { std::string p = advance().value; match(AssemblerTokenType::HASH); stmt->instr.callArgs.push_back(p + "#" + advance().value); }
                    else stmt->instr.callArgs.push_back(advance().value);
                }
                stmt->size = calculateInstructionSize(stmt->instr);
            } else if (stmt->instr.mnemonic == "EXPR") {
                stmt->type = Statement::EXPR; const auto& target = advance(); stmt->exprTarget = (target.type == AssemblerTokenType::REGISTER ? "." : "") + target.value;
                expect(AssemblerTokenType::COMMA, "Expected ,"); stmt->exprTokenIndex = (int)pos;
                while (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) advance();
                stmt->size = calculateExprSize(stmt->exprTokenIndex);
            } else if (stmt->instr.mnemonic.substr(0, 3) == "MUL" || stmt->instr.mnemonic.substr(0, 3) == "DIV") {
                stmt->type = (stmt->instr.mnemonic.substr(0, 3) == "MUL") ? Statement::MUL : Statement::DIV;
                std::string m = stmt->instr.mnemonic; if (m.size() > 4 && m[3] == '.') stmt->mulWidth = std::stoi(m.substr(4)); else stmt->mulWidth = 8;
                const auto& dest = advance(); stmt->instr.operand = (dest.type == AssemblerTokenType::REGISTER ? "." : "") + dest.value;
                if (!match(AssemblerTokenType::COMMA)) throw std::runtime_error("Expected , after destination");
                stmt->exprTokenIndex = (int)pos; while (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) advance();
                int bytes = stmt->mulWidth / 8; if (bytes < 1) bytes = 1; stmt->size = bytes * 35;
            } else {
                if (match(AssemblerTokenType::HASH)) { stmt->instr.operand = advance().value; if (stmt->instr.mnemonic == "PHW") stmt->instr.mode = AddressingMode::IMMEDIATE16; else stmt->instr.mode = AddressingMode::IMMEDIATE; }
                else if (match(AssemblerTokenType::OPEN_PAREN)) {
                    stmt->instr.operand = advance().value;
                    if (match(AssemblerTokenType::COMMA)) {
                        std::string r = advance().value; if (r == "X" || r == "x") { expect(AssemblerTokenType::CLOSE_PAREN, "Expected )"); try { uint32_t val = parseNumericLiteral(stmt->instr.operand); if (val > 0xFF || stmt->instr.mnemonic == "JSR" || stmt->instr.mnemonic == "JMP") stmt->instr.mode = AddressingMode::ABSOLUTE_X_INDIRECT; else stmt->instr.mode = AddressingMode::BASE_PAGE_X_INDIRECT; } catch(...) { stmt->instr.mode = AddressingMode::ABSOLUTE_X_INDIRECT; } }
                        else if (r == "SP" || r == "sp") { expect(AssemblerTokenType::CLOSE_PAREN, "Expected )"); expect(AssemblerTokenType::COMMA, "Expected ,"); advance(); stmt->instr.mode = AddressingMode::STACK_RELATIVE; }
                    } else {
                        expect(AssemblerTokenType::CLOSE_PAREN, "Expected )");
                        if (match(AssemblerTokenType::COMMA)) { std::string r = advance().value; if (r == "Y" || r == "y") stmt->instr.mode = AddressingMode::BASE_PAGE_INDIRECT_Y; else if (r == "Z" || r == "z") stmt->instr.mode = AddressingMode::BASE_PAGE_INDIRECT_Z; }
                        else { try { uint32_t val = parseNumericLiteral(stmt->instr.operand); if (val > 0xFF || stmt->instr.mnemonic == "JSR" || stmt->instr.mnemonic == "JMP") stmt->instr.mode = AddressingMode::ABSOLUTE_INDIRECT; else stmt->instr.mode = AddressingMode::INDIRECT; } catch(...) { stmt->instr.mode = AddressingMode::ABSOLUTE_INDIRECT; } }
                    }
                } else if (match(AssemblerTokenType::OPEN_BRACKET)) { stmt->instr.operand = advance().value; expect(AssemblerTokenType::CLOSE_BRACKET, "Expected ]"); expect(AssemblerTokenType::COMMA, "Expected ,"); advance(); stmt->instr.mode = AddressingMode::FLAT_INDIRECT_Z; }
                else if (peek().type == AssemblerTokenType::REGISTER && (peek().value == "A" || peek().value == "a")) { advance(); stmt->instr.mode = AddressingMode::ACCUMULATOR; }
                else if (peek().type == AssemblerTokenType::IDENTIFIER || peek().type == AssemblerTokenType::HEX_LITERAL || peek().type == AssemblerTokenType::DECIMAL_LITERAL) {
                    stmt->instr.operand = advance().value;
                    if (match(AssemblerTokenType::COMMA)) {
                        const auto& r = peek(); if (r.type == AssemblerTokenType::IDENTIFIER && (r.value == "X" || r.value == "x")) { advance(); stmt->instr.mode = AddressingMode::ABSOLUTE_X; }
                        else if (r.type == AssemblerTokenType::IDENTIFIER && (r.value == "Y" || r.value == "y")) { advance(); stmt->instr.mode = AddressingMode::ABSOLUTE_Y; }
                        else if (r.type == AssemblerTokenType::IDENTIFIER && (r.value == "S" || r.value == "s")) { advance(); stmt->instr.mode = AddressingMode::STACK_RELATIVE; }
                        else { stmt->instr.bitBranchTarget = advance().value; stmt->instr.mode = AddressingMode::BASE_PAGE_RELATIVE; }
                    } else { if (stmt->instr.mnemonic[0] == 'B' && stmt->instr.mnemonic.size() == 3 && stmt->instr.mnemonic != "BIT" && stmt->instr.mnemonic != "BRK") stmt->instr.mode = AddressingMode::RELATIVE; else stmt->instr.mode = AddressingMode::ABSOLUTE; }
                } else stmt->instr.mode = AddressingMode::IMPLIED;
                if (stmt->instr.mode == AddressingMode::ABSOLUTE && !stmt->instr.operand.empty()) {
                    if (stmt->instr.operand == "A" || stmt->instr.operand == "a") stmt->instr.mode = AddressingMode::ACCUMULATOR;
                    else { try { uint32_t val = parseNumericLiteral(stmt->instr.operand); if (val <= 0xFF && stmt->instr.mnemonic != "JMP" && stmt->instr.mnemonic != "JSR") stmt->instr.mode = AddressingMode::BASE_PAGE; } catch(...) {} }
                }
                if (stmt->instr.mode == AddressingMode::ABSOLUTE_X && !stmt->instr.operand.empty()) { try { uint32_t val = parseNumericLiteral(stmt->instr.operand); if (val <= 0xFF) stmt->instr.mode = AddressingMode::BASE_PAGE_X; } catch(...) {} }
                if (stmt->instr.mode == AddressingMode::ABSOLUTE_Y && !stmt->instr.operand.empty()) { try { uint32_t val = parseNumericLiteral(stmt->instr.operand); if (val <= 0xFF) stmt->instr.mode = AddressingMode::BASE_PAGE_Y; } catch(...) {} }
                stmt->size = calculateInstructionSize(stmt->instr);
            }
        } else if (stmt->label.empty()) { advance(); continue; }
        pc += stmt->size; statements.push_back(std::move(stmt));
    }
}

int AssemblerParser::calculateDirectiveSize(const Directive& dir) {
    if (dir.name == "byte") return (int)dir.arguments.size();
    if (dir.name == "word") return (int)dir.arguments.size() * 2;
    if (dir.name == "dword" || dir.name == "long") return (int)dir.arguments.size() * 4;
    if (dir.name == "float") return (int)dir.arguments.size() * 5;
    if (dir.name == "text" || dir.name == "ascii") return dir.arguments.empty() ? 0 : (int)dir.arguments[0].length();
    return 0;
}

int AssemblerParser::calculateExprSize(int tokenIndex) {
    int idx = tokenIndex; auto ast = parseExprAST(tokens, idx, symbolTable); if (!ast) return 0;
    return ast->isConstant() ? 5 : 35;
}

int AssemblerParser::calculateInstructionSize(const Instruction& instr) {
    if (instr.mnemonic == "PROC") return 0; if (instr.mnemonic == "ENDPROC") return 2;
    if (instr.mnemonic == "CALL") return 3 + (int)instr.callArgs.size() * 3;
    int size = 0; bool isQuad = (instr.mnemonic.size() > 1 && instr.mnemonic.back() == 'Q' && instr.mnemonic != "LDQ" && instr.mnemonic != "STQ" && instr.mnemonic != "BEQ" && instr.mnemonic != "BNE" && instr.mnemonic != "BRA" && instr.mnemonic != "BSR");
    if (isQuad) size += 2; if (instr.mode == AddressingMode::FLAT_INDIRECT_Z) size += 1;
    if (instr.mnemonic == "PHW") size += 3; else if (instr.mnemonic == "RTN") size += 2; else if (instr.mnemonic == "BSR") size += 3; else if (instr.mnemonic == "BRA" || (instr.mnemonic[0] == 'B' && instr.mnemonic.size() == 3 && instr.mnemonic != "BIT" && instr.mnemonic != "BRK")) size += 3;
    else {
        switch (instr.mode) {
            case AddressingMode::IMPLIED: case AddressingMode::ACCUMULATOR: size += 1; break;
            case AddressingMode::IMMEDIATE: case AddressingMode::STACK_RELATIVE: case AddressingMode::RELATIVE: case AddressingMode::BASE_PAGE: case AddressingMode::BASE_PAGE_X: case AddressingMode::BASE_PAGE_Y: case AddressingMode::BASE_PAGE_X_INDIRECT: case AddressingMode::BASE_PAGE_INDIRECT_Y: case AddressingMode::BASE_PAGE_INDIRECT_Z: case AddressingMode::FLAT_INDIRECT_Z: size += 2; break;
            case AddressingMode::IMMEDIATE16: case AddressingMode::RELATIVE16: case AddressingMode::BASE_PAGE_RELATIVE: default: size += 3; break;
        }
    }
    return size;
}

uint8_t AssemblerParser::getOpcode(const std::string& m, AddressingMode mode) {
    std::string baseM = m; if (m.size() > 1 && m.back() == 'Q' && m != "LDQ" && m != "STQ" && m.substr(0, 3) != "BEQ" && m.substr(0, 3) != "BNE" && m.substr(0, 3) != "BRA" && m.substr(0, 3) != "BSR") baseM = m.substr(0, m.size() - 1);
    static const std::map<std::pair<std::string, AddressingMode>, uint8_t> opcodes = {
        { {"ADC", AddressingMode::BASE_PAGE_INDIRECT_Y}, 0x71 }, { {"ADC", AddressingMode::BASE_PAGE_INDIRECT_Z}, 0x72 }, { {"ADC", AddressingMode::STACK_RELATIVE}, 0x72 }, { {"ADC", AddressingMode::BASE_PAGE_X_INDIRECT}, 0x61 }, { {"ADC", AddressingMode::ABSOLUTE}, 0x6D }, { {"ADC", AddressingMode::ABSOLUTE_X}, 0x7D }, { {"ADC", AddressingMode::ABSOLUTE_Y}, 0x79 }, { {"ADC", AddressingMode::BASE_PAGE}, 0x65 }, { {"ADC", AddressingMode::BASE_PAGE_X}, 0x75 }, { {"ADC", AddressingMode::IMMEDIATE}, 0x69 },
        { {"AND", AddressingMode::BASE_PAGE_INDIRECT_Y}, 0x31 }, { {"AND", AddressingMode::BASE_PAGE_INDIRECT_Z}, 0x32 }, { {"AND", AddressingMode::STACK_RELATIVE}, 0x32 }, { {"AND", AddressingMode::BASE_PAGE_X_INDIRECT}, 0x21 }, { {"AND", AddressingMode::ABSOLUTE}, 0x2D }, { {"AND", AddressingMode::ABSOLUTE_X}, 0x3D }, { {"AND", AddressingMode::ABSOLUTE_Y}, 0x39 }, { {"AND", AddressingMode::BASE_PAGE}, 0x25 }, { {"AND", AddressingMode::BASE_PAGE_X}, 0x35 }, { {"AND", AddressingMode::IMMEDIATE}, 0x29 },
        { {"ASL", AddressingMode::ABSOLUTE}, 0x0E }, { {"ASL", AddressingMode::ABSOLUTE_X}, 0x1E }, { {"ASL", AddressingMode::ACCUMULATOR}, 0x0A }, { {"ASL", AddressingMode::BASE_PAGE}, 0x06 }, { {"ASL", AddressingMode::BASE_PAGE_X}, 0x16 },
        { {"ASR", AddressingMode::ACCUMULATOR}, 0x43 }, { {"ASR", AddressingMode::BASE_PAGE}, 0x44 }, { {"ASR", AddressingMode::BASE_PAGE_X}, 0x54 }, { {"ASW", AddressingMode::ABSOLUTE}, 0xCB },
        { {"BBR0", AddressingMode::BASE_PAGE_RELATIVE}, 0x0F }, { {"BBR1", AddressingMode::BASE_PAGE_RELATIVE}, 0x1F }, { {"BBR2", AddressingMode::BASE_PAGE_RELATIVE}, 0x2F }, { {"BBR3", AddressingMode::BASE_PAGE_RELATIVE}, 0x3F }, { {"BBR4", AddressingMode::BASE_PAGE_RELATIVE}, 0x4F }, { {"BBR5", AddressingMode::BASE_PAGE_RELATIVE}, 0x5F }, { {"BBR6", AddressingMode::BASE_PAGE_RELATIVE}, 0x6F }, { {"BBR7", AddressingMode::BASE_PAGE_RELATIVE}, 0x7F },
        { {"BBS0", AddressingMode::BASE_PAGE_RELATIVE}, 0x8F }, { {"BBS1", AddressingMode::BASE_PAGE_RELATIVE}, 0x9F }, { {"BBS2", AddressingMode::BASE_PAGE_RELATIVE}, 0xAF }, { {"BBS3", AddressingMode::BASE_PAGE_RELATIVE}, 0xBF }, { {"BBS4", AddressingMode::BASE_PAGE_RELATIVE}, 0xCF }, { {"BBS5", AddressingMode::BASE_PAGE_RELATIVE}, 0xDF }, { {"BBS6", AddressingMode::BASE_PAGE_RELATIVE}, 0xEF }, { {"BBS7", AddressingMode::BASE_PAGE_RELATIVE}, 0xFF },
        { {"BCC", AddressingMode::RELATIVE}, 0x90 }, { {"BCC", AddressingMode::RELATIVE16}, 0x93 }, { {"BCS", AddressingMode::RELATIVE}, 0xB0 }, { {"BCS", AddressingMode::RELATIVE16}, 0xB3 }, { {"BEQ", AddressingMode::RELATIVE}, 0xF0 }, { {"BEQ", AddressingMode::RELATIVE16}, 0xF3 },
        { {"BIT", AddressingMode::ABSOLUTE}, 0x2C }, { {"BIT", AddressingMode::ABSOLUTE_X}, 0x3C }, { {"BIT", AddressingMode::BASE_PAGE}, 0x24 }, { {"BIT", AddressingMode::BASE_PAGE_X}, 0x34 }, { {"BIT", AddressingMode::IMMEDIATE}, 0x89 },
        { {"BMI", AddressingMode::RELATIVE}, 0x30 }, { {"BMI", AddressingMode::RELATIVE16}, 0x33 }, { {"BNE", AddressingMode::RELATIVE}, 0xD0 }, { {"BNE", AddressingMode::RELATIVE16}, 0xD3 }, { {"BPL", AddressingMode::RELATIVE}, 0x10 }, { {"BPL", AddressingMode::RELATIVE16}, 0x13 }, { {"BRA", AddressingMode::RELATIVE}, 0x80 }, { {"BRA", AddressingMode::RELATIVE16}, 0x83 },
        { {"BRK", AddressingMode::IMPLIED}, 0x00 }, { {"BSR", AddressingMode::RELATIVE16}, 0x63 }, { {"BVC", AddressingMode::RELATIVE}, 0x50 }, { {"BVC", AddressingMode::RELATIVE16}, 0x53 }, { {"BVS", AddressingMode::RELATIVE}, 0x70 }, { {"BVS", AddressingMode::RELATIVE16}, 0x73 },
        { {"CLC", AddressingMode::IMPLIED}, 0x18 }, { {"CLD", AddressingMode::IMPLIED}, 0xD8 }, { {"CLE", AddressingMode::IMPLIED}, 0x02 }, { {"CLI", AddressingMode::IMPLIED}, 0x58 }, { {"CLV", AddressingMode::IMPLIED}, 0xB8 },
        { {"CMP", AddressingMode::BASE_PAGE_INDIRECT_Y}, 0xD1 }, { {"CMP", AddressingMode::BASE_PAGE_INDIRECT_Z}, 0xD2 }, { {"CMP", AddressingMode::STACK_RELATIVE}, 0xD2 }, { {"CMP", AddressingMode::BASE_PAGE_X_INDIRECT}, 0xC1 }, { {"CMP", AddressingMode::ABSOLUTE}, 0xCD }, { {"CMP", AddressingMode::ABSOLUTE_X}, 0xDD }, { {"CMP", AddressingMode::ABSOLUTE_Y}, 0xD9 }, { {"CMP", AddressingMode::BASE_PAGE}, 0xC5 }, { {"CMP", AddressingMode::BASE_PAGE_X}, 0xD5 }, { {"CMP", AddressingMode::IMMEDIATE}, 0xC9 },
        { {"CPX", AddressingMode::ABSOLUTE}, 0xEC }, { {"CPX", AddressingMode::BASE_PAGE}, 0xE4 }, { {"CPX", AddressingMode::IMMEDIATE}, 0xE0 }, { {"CPY", AddressingMode::ABSOLUTE}, 0xCC }, { {"CPY", AddressingMode::BASE_PAGE}, 0xC4 }, { {"CPY", AddressingMode::IMMEDIATE}, 0xC0 }, { {"CPZ", AddressingMode::ABSOLUTE}, 0xDC }, { {"CPZ", AddressingMode::BASE_PAGE}, 0xD4 }, { {"CPZ", AddressingMode::IMMEDIATE}, 0xC2 },
        { {"DEC", AddressingMode::ABSOLUTE}, 0xCE }, { {"DEC", AddressingMode::ABSOLUTE_X}, 0xDE }, { {"DEC", AddressingMode::ACCUMULATOR}, 0x3A }, { {"DEC", AddressingMode::BASE_PAGE}, 0xC6 }, { {"DEC", AddressingMode::BASE_PAGE_X}, 0xD6 }, { {"DEW", AddressingMode::BASE_PAGE}, 0xC3 }, { {"DEX", AddressingMode::IMPLIED}, 0xCA }, { {"DEY", AddressingMode::IMPLIED}, 0x88 }, { {"DEZ", AddressingMode::IMPLIED}, 0x3B }, { {"EOM", AddressingMode::IMPLIED}, 0xEA },
        { {"EOR", AddressingMode::BASE_PAGE_INDIRECT_Y}, 0x51 }, { {"EOR", AddressingMode::BASE_PAGE_INDIRECT_Z}, 0x52 }, { {"EOR", AddressingMode::STACK_RELATIVE}, 0x52 }, { {"EOR", AddressingMode::BASE_PAGE_X_INDIRECT}, 0x41 }, { {"EOR", AddressingMode::ABSOLUTE}, 0x4D }, { {"EOR", AddressingMode::ABSOLUTE_X}, 0x5D }, { {"EOR", AddressingMode::ABSOLUTE_Y}, 0x59 }, { {"EOR", AddressingMode::BASE_PAGE}, 0x45 }, { {"EOR", AddressingMode::BASE_PAGE_X}, 0x55 }, { {"EOR", AddressingMode::IMMEDIATE}, 0x49 },
        { {"INC", AddressingMode::ABSOLUTE}, 0xEE }, { {"INC", AddressingMode::ABSOLUTE_X}, 0xFE }, { {"INC", AddressingMode::ACCUMULATOR}, 0x1A }, { {"INC", AddressingMode::BASE_PAGE}, 0xE6 }, { {"INC", AddressingMode::BASE_PAGE_X}, 0xF6 }, { {"INW", AddressingMode::BASE_PAGE}, 0xE3 }, { {"INX", AddressingMode::IMPLIED}, 0xE8 }, { {"INY", AddressingMode::IMPLIED}, 0xC8 }, { {"INZ", AddressingMode::IMPLIED}, 0x1B },
        { {"JMP", AddressingMode::ABSOLUTE_INDIRECT}, 0x6C }, { {"JMP", AddressingMode::ABSOLUTE_X_INDIRECT}, 0x7C }, { {"JMP", AddressingMode::ABSOLUTE}, 0x4C }, { {"JSR", AddressingMode::ABSOLUTE_INDIRECT}, 0x22 }, { {"JSR", AddressingMode::ABSOLUTE_X_INDIRECT}, 0x23 }, { {"JSR", AddressingMode::ABSOLUTE}, 0x20 },
        { {"LDA", AddressingMode::BASE_PAGE_INDIRECT_Y}, 0xB1 }, { {"LDA", AddressingMode::BASE_PAGE_INDIRECT_Z}, 0xB2 }, { {"LDA", AddressingMode::STACK_RELATIVE}, 0xE2 }, { {"LDA", AddressingMode::BASE_PAGE_X_INDIRECT}, 0xA1 }, { {"LDA", AddressingMode::ABSOLUTE}, 0xAD }, { {"LDA", AddressingMode::ABSOLUTE_X}, 0xBD }, { {"LDA", AddressingMode::ABSOLUTE_Y}, 0xB9 }, { {"LDA", AddressingMode::BASE_PAGE}, 0xA5 }, { {"LDA", AddressingMode::BASE_PAGE_X}, 0xB5 }, { {"LDA", AddressingMode::IMMEDIATE}, 0xA9 },
        { {"LDX", AddressingMode::ABSOLUTE}, 0xAE }, { {"LDX", AddressingMode::ABSOLUTE_Y}, 0xBE }, { {"LDX", AddressingMode::BASE_PAGE}, 0xA6 }, { {"LDX", AddressingMode::BASE_PAGE_Y}, 0xB6 }, { {"LDX", AddressingMode::IMMEDIATE}, 0xA2 },
        { {"LDY", AddressingMode::ABSOLUTE}, 0xAC }, { {"LDY", AddressingMode::ABSOLUTE_X}, 0xBC }, { {"LDY", AddressingMode::BASE_PAGE}, 0xA4 }, { {"LDY", AddressingMode::BASE_PAGE_X}, 0xB4 }, { {"LDY", AddressingMode::IMMEDIATE}, 0xA0 },
        { {"LDZ", AddressingMode::ABSOLUTE}, 0xAB }, { {"LDZ", AddressingMode::ABSOLUTE_X}, 0xBB }, { {"LDZ", AddressingMode::IMMEDIATE}, 0xA3 }, { {"LDQ", AddressingMode::BASE_PAGE}, 0xA5 }, { {"LDQ", AddressingMode::ABSOLUTE}, 0xAD }, { {"LDQ", AddressingMode::BASE_PAGE_INDIRECT_Z}, 0xB2 }, { {"LDQ", AddressingMode::FLAT_INDIRECT_Z}, 0xB2 },
        { {"LSR", AddressingMode::ABSOLUTE}, 0x4E }, { {"LSR", AddressingMode::ABSOLUTE_X}, 0x5E }, { {"LSR", AddressingMode::ACCUMULATOR}, 0x4A }, { {"LSR", AddressingMode::BASE_PAGE}, 0x46 }, { {"LSR", AddressingMode::BASE_PAGE_X}, 0x56 }, { {"MAP", AddressingMode::IMPLIED}, 0x5C }, { {"NEG", AddressingMode::ACCUMULATOR}, 0x42 },
        { {"ORA", AddressingMode::BASE_PAGE_INDIRECT_Y}, 0x11 }, { {"ORA", AddressingMode::BASE_PAGE_INDIRECT_Z}, 0x12 }, { {"ORA", AddressingMode::STACK_RELATIVE}, 0x12 }, { {"ORA", AddressingMode::BASE_PAGE_X_INDIRECT}, 0x01 }, { {"ORA", AddressingMode::ABSOLUTE}, 0x0D }, { {"ORA", AddressingMode::ABSOLUTE_X}, 0x1D }, { {"ORA", AddressingMode::ABSOLUTE_Y}, 0x19 }, { {"ORA", AddressingMode::BASE_PAGE}, 0x05 }, { {"ORA", AddressingMode::BASE_PAGE_X}, 0x15 }, { {"ORA", AddressingMode::IMMEDIATE}, 0x09 },
        { {"PHA", AddressingMode::IMPLIED}, 0x48 }, { {"PHP", AddressingMode::IMPLIED}, 0x08 }, { {"PHW", AddressingMode::ABSOLUTE}, 0xFC }, { {"PHW", AddressingMode::IMMEDIATE16}, 0xF4 }, { {"PHX", AddressingMode::IMPLIED}, 0xDA }, { {"PHY", AddressingMode::IMPLIED}, 0x5A }, { {"PHZ", AddressingMode::IMPLIED}, 0xDB }, { {"PLA", AddressingMode::IMPLIED}, 0x68 }, { {"PLP", AddressingMode::IMPLIED}, 0x28 }, { {"PLX", AddressingMode::IMPLIED}, 0xFA }, { {"PLY", AddressingMode::IMPLIED}, 0x7A }, { {"PLZ", AddressingMode::IMPLIED}, 0xFB },
        { {"RMB0", AddressingMode::BASE_PAGE}, 0x07 }, { {"RMB1", AddressingMode::BASE_PAGE}, 0x17 }, { {"RMB2", AddressingMode::BASE_PAGE}, 0x27 }, { {"RMB3", AddressingMode::BASE_PAGE}, 0x37 }, { {"RMB4", AddressingMode::BASE_PAGE}, 0x47 }, { {"RMB5", AddressingMode::BASE_PAGE}, 0x57 }, { {"RMB6", AddressingMode::BASE_PAGE}, 0x67 }, { {"RMB7", AddressingMode::BASE_PAGE}, 0x77 },
        { {"ROL", AddressingMode::ABSOLUTE}, 0x2E }, { {"ROL", AddressingMode::ABSOLUTE_X}, 0x3E }, { {"ROL", AddressingMode::ACCUMULATOR}, 0x2A }, { {"ROL", AddressingMode::BASE_PAGE}, 0x26 }, { {"ROL", AddressingMode::BASE_PAGE_X}, 0x36 }, { {"ROR", AddressingMode::ABSOLUTE}, 0x6E }, { {"ROR", AddressingMode::ABSOLUTE_X}, 0x7E }, { {"ROR", AddressingMode::ACCUMULATOR}, 0x6A }, { {"ROR", AddressingMode::BASE_PAGE}, 0x66 }, { {"ROR", AddressingMode::BASE_PAGE_X}, 0x76 }, { {"ROW", AddressingMode::ABSOLUTE}, 0xEB },
        { {"RTI", AddressingMode::IMPLIED}, 0x40 }, { {"RTS", AddressingMode::IMMEDIATE}, 0x62 }, { {"RTS", AddressingMode::IMPLIED}, 0x60 },
        { {"SBC", AddressingMode::BASE_PAGE_INDIRECT_Y}, 0xF1 }, { {"SBC", AddressingMode::BASE_PAGE_INDIRECT_Z}, 0xF2 }, { {"SBC", AddressingMode::STACK_RELATIVE}, 0xF2 }, { {"SBC", AddressingMode::BASE_PAGE_X_INDIRECT}, 0xE1 }, { {"SBC", AddressingMode::ABSOLUTE}, 0xED }, { {"SBC", AddressingMode::ABSOLUTE_X}, 0xFD }, { {"SBC", AddressingMode::ABSOLUTE_Y}, 0xF9 }, { {"SBC", AddressingMode::BASE_PAGE}, 0xE5 }, { {"SBC", AddressingMode::BASE_PAGE_X}, 0xF5 }, { {"SBC", AddressingMode::IMMEDIATE}, 0xE9 },
        { {"SEC", AddressingMode::IMPLIED}, 0x38 }, { {"SED", AddressingMode::IMPLIED}, 0xD8 }, { {"SEE", AddressingMode::IMPLIED}, 0x03 }, { {"SEI", AddressingMode::IMPLIED}, 0x78 }, { {"SMB0", AddressingMode::BASE_PAGE}, 0x87 }, { {"SMB1", AddressingMode::BASE_PAGE}, 0x97 }, { {"SMB2", AddressingMode::BASE_PAGE}, 0xA7 }, { {"SMB3", AddressingMode::BASE_PAGE}, 0xB7 }, { {"SMB4", AddressingMode::BASE_PAGE}, 0xC7 }, { {"SMB5", AddressingMode::BASE_PAGE}, 0xD7 }, { {"SMB6", AddressingMode::BASE_PAGE}, 0xE7 }, { {"SMB7", AddressingMode::BASE_PAGE}, 0xF7 },
        { {"STA", AddressingMode::BASE_PAGE_INDIRECT_Y}, 0x91 }, { {"STA", AddressingMode::BASE_PAGE_INDIRECT_Z}, 0x92 }, { {"STA", AddressingMode::STACK_RELATIVE}, 0x82 }, { {"STA", AddressingMode::BASE_PAGE_X_INDIRECT}, 0x81 }, { {"STA", AddressingMode::ABSOLUTE}, 0x8D }, { {"STA", AddressingMode::ABSOLUTE_X}, 0x9D }, { {"STA", AddressingMode::ABSOLUTE_Y}, 0x99 }, { {"STA", AddressingMode::BASE_PAGE}, 0x85 }, { {"STA", AddressingMode::BASE_PAGE_X}, 0x95 },
        { {"STX", AddressingMode::ABSOLUTE}, 0x8E }, { {"STX", AddressingMode::ABSOLUTE_Y}, 0x9B }, { {"STX", AddressingMode::BASE_PAGE}, 0x86 }, { {"STX", AddressingMode::BASE_PAGE_Y}, 0x96 },
        { {"STY", AddressingMode::ABSOLUTE}, 0x8C }, { {"STY", AddressingMode::ABSOLUTE_X}, 0x8B }, { {"STY", AddressingMode::BASE_PAGE}, 0x84 }, { {"STY", AddressingMode::BASE_PAGE_X}, 0x94 },
        { {"STZ", AddressingMode::ABSOLUTE}, 0x9C }, { {"STZ", AddressingMode::ABSOLUTE_X}, 0x9E }, { {"STZ", AddressingMode::BASE_PAGE}, 0x64 }, { {"STZ", AddressingMode::BASE_PAGE_X}, 0x74 }, { {"STQ", AddressingMode::BASE_PAGE}, 0x85 }, { {"STQ", AddressingMode::ABSOLUTE}, 0x8D }, { {"STQ", AddressingMode::BASE_PAGE_INDIRECT_Z}, 0x92 }, { {"STQ", AddressingMode::FLAT_INDIRECT_Z}, 0x92 },
        { {"TAB", AddressingMode::IMPLIED}, 0x5B }, { {"TAX", AddressingMode::IMPLIED}, 0xAA }, { {"TAY", AddressingMode::IMPLIED}, 0xA8 }, { {"TAZ", AddressingMode::IMPLIED}, 0x4B }, { {"TBA", AddressingMode::IMPLIED}, 0x7B }, { {"TRB", AddressingMode::ABSOLUTE}, 0x1C }, { {"TRB", AddressingMode::BASE_PAGE}, 0x14 }, { {"TSB", AddressingMode::ABSOLUTE}, 0x0C }, { {"TSB", AddressingMode::BASE_PAGE}, 0x04 }, { {"TSX", AddressingMode::IMPLIED}, 0xBA }, { {"TSY", AddressingMode::IMPLIED}, 0x0B }, { {"TXA", AddressingMode::IMPLIED}, 0x8A }, { {"TXS", AddressingMode::IMPLIED}, 0x9A }, { {"TYA", AddressingMode::IMPLIED}, 0x98 }, { {"TYS", AddressingMode::IMPLIED}, 0x2B }, { {"TZA", AddressingMode::IMPLIED}, 0x6B },
    };
    auto it = opcodes.find({baseM, mode}); if (it != opcodes.end()) return it->second; return 0;
}

void AssemblerParser::emitExpressionCode(std::vector<uint8_t>& binary, const std::string& target, int tokenIndex) {
    int idx = tokenIndex; auto ast = parseExprAST(tokens, idx, symbolTable); if (!ast) return;
    int width = 8; if (target == ".AX" || target == ".AY" || target == ".AZ" || target == ".XY") width = 16; else if (target == ".AXY") width = 24; else if (target == ".Q" || target == ".AXYZ") width = 32; else if (target[0] != '.') width = 16;
    if (ast->isConstant()) { uint32_t val = ast->getValue(); binary.push_back(0xA9); binary.push_back(val & 0xFF); if (width >= 16) { binary.push_back(0xA2); binary.push_back((val >> 8) & 0xFF); } }
    else ast->emit(binary, this, width, target);
    if (target == ".X") binary.push_back(0xAA); else if (target == ".Y") binary.push_back(0xA8); else if (target == ".Z") binary.push_back(0x5B);
    else if (target[0] != '.') { uint32_t addr = symbolTable.count(target) ? symbolTable[target].value : parseNumericLiteral(target); binary.push_back(0x8D); binary.push_back(addr & 0xFF); binary.push_back((addr >> 8) & 0xFF); if (width >= 16) { binary.push_back(0x8E); binary.push_back((addr + 1) & 0xFF); binary.push_back(((addr + 1) >> 8) & 0xFF); } }
}

void AssemblerParser::emitMulCode(std::vector<uint8_t>& binary, int width, const std::string& dest, int tokenIndex) {
    int idx = tokenIndex; auto srcAst = parseExprAST(tokens, idx, symbolTable); if (!srcAst) return;
    int bytes = width / 8; if (bytes < 1) bytes = 1; if (bytes > 4) bytes = 4;
    auto storeToMath = [&](uint8_t regAddr, int byteIdx, const std::string& source) {
        if (source == ".A") { binary.push_back(0x8D); binary.push_back(regAddr + byteIdx); binary.push_back(0xD7); }
        else if (source == ".X") { binary.push_back(0x8E); binary.push_back(regAddr + byteIdx); binary.push_back(0xD7); }
        else if (source == ".Y") { binary.push_back(0x8C); binary.push_back(regAddr + byteIdx); binary.push_back(0xD7); }
        else if (source == ".Z") { binary.push_back(0x9C); binary.push_back(regAddr + byteIdx); binary.push_back(0xD7); }
        else { uint32_t addr = symbolTable.count(source) ? symbolTable[source].value : parseNumericLiteral(source); binary.push_back(0xAD); binary.push_back((addr + byteIdx) & 0xFF); binary.push_back(((addr + byteIdx) >> 8) & 0xFF); binary.push_back(0x8D); binary.push_back(regAddr + byteIdx); binary.push_back(0xD7); }
    };
    if (dest == ".A" || dest == ".AX" || dest == ".AXY" || dest == ".AXYZ" || dest == ".Q") { if (bytes >= 1) storeToMath(0x70, 0, ".A"); if (bytes >= 2) storeToMath(0x70, 1, ".X"); if (bytes >= 3) storeToMath(0x70, 2, ".Y"); if (bytes >= 4) storeToMath(0x70, 3, ".Z"); } else for (int i = 0; i < bytes; ++i) storeToMath(0x70, i, dest);
    if (srcAst->isConstant()) { uint32_t val = srcAst->getValue(); for (int i = 0; i < bytes; ++i) { binary.push_back(0xA9); binary.push_back((val >> (i * 8)) & 0xFF); binary.push_back(0x8D); binary.push_back(0x74 + i); binary.push_back(0xD7); } }
    else { int tempIdx = tokenIndex; std::string srcName = tokens[tempIdx].value; if (tokens[tempIdx].type == AssemblerTokenType::REGISTER) srcName = "." + srcName; if (srcName == ".A" || srcName == ".AX" || srcName == ".AXY" || srcName == ".AXYZ" || srcName == ".Q") { if (bytes >= 1) storeToMath(0x74, 0, ".A"); if (bytes >= 2) storeToMath(0x74, 1, ".X"); if (bytes >= 3) storeToMath(0x74, 2, ".Y"); if (bytes >= 4) storeToMath(0x74, 3, ".Z"); } else for (int i = 0; i < bytes; ++i) storeToMath(0x74, i, srcName); }
    for (int i = 0; i < bytes; ++i) { binary.push_back(0xAD); binary.push_back(0x78 + i); binary.push_back(0xD7); if (dest == ".A" || dest == ".AX" || dest == ".AXY" || dest == ".AXYZ" || dest == ".Q") { if (i == 1) binary.push_back(0xAA); else if (i == 2) binary.push_back(0xA8); else if (i == 3) binary.push_back(0x5B); } else { uint32_t addr = symbolTable.count(dest) ? symbolTable[dest].value : parseNumericLiteral(dest); binary.push_back(0x8D); binary.push_back((addr + i) & 0xFF); binary.push_back(((addr + i) >> 8) & 0xFF); } }
}

void AssemblerParser::emitDivCode(std::vector<uint8_t>& binary, int width, const std::string& dest, int tokenIndex) {
    int idx = tokenIndex; auto srcAst = parseExprAST(tokens, idx, symbolTable); if (!srcAst) return;
    int bytes = width / 8; if (bytes < 1) bytes = 1; if (bytes > 4) bytes = 4;
    auto storeToMath = [&](uint8_t regAddr, int byteIdx, const std::string& source) {
        if (source == ".A") { binary.push_back(0x8D); binary.push_back(regAddr + byteIdx); binary.push_back(0xD7); }
        else if (source == ".X") { binary.push_back(0x8E); binary.push_back(regAddr + byteIdx); binary.push_back(0xD7); }
        else if (source == ".Y") { binary.push_back(0x8C); binary.push_back(regAddr + byteIdx); binary.push_back(0xD7); }
        else if (source == ".Z") { binary.push_back(0x9C); binary.push_back(regAddr + byteIdx); binary.push_back(0xD7); }
        else { uint32_t addr = symbolTable.count(source) ? symbolTable[source].value : parseNumericLiteral(source); binary.push_back(0xAD); binary.push_back((addr + byteIdx) & 0xFF); binary.push_back(((addr + byteIdx) >> 8) & 0xFF); binary.push_back(0x8D); binary.push_back(regAddr + byteIdx); binary.push_back(0xD7); }
    };
    if (dest == ".A" || dest == ".AX" || dest == ".AXY" || dest == ".AXYZ" || dest == ".Q") { if (bytes >= 1) storeToMath(0x70, 0, ".A"); if (bytes >= 2) storeToMath(0x70, 1, ".X"); if (bytes >= 3) storeToMath(0x70, 2, ".Y"); if (bytes >= 4) storeToMath(0x70, 3, ".Z"); } else for (int i = 0; i < bytes; ++i) storeToMath(0x70, i, dest);
    if (srcAst->isConstant()) { uint32_t val = srcAst->getValue(); for (int i = 0; i < bytes; ++i) { binary.push_back(0xA9); binary.push_back((val >> (i * 8)) & 0xFF); binary.push_back(0x8D); binary.push_back(0x74 + i); binary.push_back(0xD7); } }
    else { int tempIdx = tokenIndex; std::string srcName = tokens[tempIdx].value; if (tokens[tempIdx].type == AssemblerTokenType::REGISTER) srcName = "." + srcName; if (srcName == ".A" || srcName == ".AX" || srcName == ".AXY" || srcName == ".AXYZ" || srcName == ".Q") { if (bytes >= 1) storeToMath(0x74, 0, ".A"); if (bytes >= 2) storeToMath(0x74, 1, ".X"); if (bytes >= 3) storeToMath(0x74, 2, ".Y"); if (bytes >= 4) storeToMath(0x74, 3, ".Z"); } else for (int i = 0; i < bytes; ++i) storeToMath(0x74, i, srcName); }
    binary.push_back(0x24); binary.push_back(0x0F); binary.push_back(0xD7); binary.push_back(0x30); binary.push_back(0xFB);
    for (int i = 0; i < bytes; ++i) { binary.push_back(0xAD); binary.push_back(0x6C + i); binary.push_back(0xD7); if (dest == ".A" || dest == ".AX" || dest == ".AXY" || dest == ".AXYZ" || dest == ".Q") { if (i == 1) binary.push_back(0xAA); else if (i == 2) binary.push_back(0xA8); else if (i == 3) binary.push_back(0x5B); } else { uint32_t addr = symbolTable.count(dest) ? symbolTable[dest].value : parseNumericLiteral(dest); binary.push_back(0x8D); binary.push_back((addr + i) & 0xFF); binary.push_back(((addr + i) >> 8) & 0xFF); } }
}

std::vector<uint8_t> AssemblerParser::pass2() {
    std::vector<uint8_t> binary; ProcContext* currentPass2Proc = nullptr; bool isDeadCode = false;
    for (auto& [name, symbol] : symbolTable) if (symbol.isVariable) symbol.value = symbol.initialValue;
    for (auto& stmt : statements) {
        if (!stmt->label.empty()) { isDeadCode = false; symbolTable[stmt->label].value = stmt->address; }
        if (procedures.count(stmt->address)) { if (stmt->type == Statement::INSTRUCTION && stmt->instr.mnemonic == "PROC") { currentPass2Proc = &procedures[stmt->address]; isDeadCode = false; } }
        if (stmt->type == Statement::EXPR) { if (!isDeadCode) emitExpressionCode(binary, stmt->exprTarget, stmt->exprTokenIndex); continue; }
        if (stmt->type == Statement::MUL) { if (!isDeadCode) emitMulCode(binary, stmt->mulWidth, stmt->instr.operand, stmt->exprTokenIndex); continue; }
        if (stmt->type == Statement::DIV) { if (!isDeadCode) emitDivCode(binary, stmt->mulWidth, stmt->instr.operand, stmt->exprTokenIndex); continue; }
        if (stmt->type == Statement::BASIC_UPSTART) {
            if (!isDeadCode) {
                uint32_t addr = 0; const auto& t = tokens[stmt->basicUpstartTokenIndex]; if (t.type == AssemblerTokenType::IDENTIFIER) { if (symbolTable.count(t.value)) addr = symbolTable[t.value].value; } else addr = parseNumericLiteral(t.value);
                std::string addrStr = std::to_string(addr); while (addrStr.length() < 4) addrStr = " " + addrStr; if (addrStr.length() > 4) addrStr = addrStr.substr(addrStr.length() - 4);
                uint16_t nextLine = (uint16_t)(stmt->address + 12 - 2); binary.push_back(nextLine & 0xFF); binary.push_back((nextLine >> 8) & 0xFF); binary.push_back(0x0A); binary.push_back(0x00); binary.push_back(0x9E); for (char c : addrStr) binary.push_back(c); binary.push_back(0x00); binary.push_back(0x00); binary.push_back(0x00);
            }
            continue;
        }
        if (stmt->type == Statement::INSTRUCTION) {
            if (stmt->instr.mnemonic == "PROC") continue;
            else if (stmt->instr.mnemonic == "ENDPROC") { if (!isDeadCode) { if (stmt->instr.procParamSize == 0) binary.push_back(0x60); else { binary.push_back(0x62); binary.push_back((uint8_t)stmt->instr.procParamSize); } } currentPass2Proc = nullptr; isDeadCode = false; }
            else if (stmt->instr.mnemonic == "CALL") {
                if (!isDeadCode) {
                    for (const auto& arg : stmt->instr.callArgs) {
                        bool isByte = (arg.size() > 2 && arg.substr(0, 2) == "B#"); std::string v = isByte ? arg.substr(2) : (arg.substr(0, 2) == "W#" ? arg.substr(2) : (arg[0] == '#' ? arg.substr(1) : arg));
                        uint32_t val; if (currentPass2Proc && currentPass2Proc->localArgs.count(v)) val = currentPass2Proc->localArgs[v]; else if (symbolTable.count(v)) val = symbolTable[v].value; else val = parseNumericLiteral(v);
                        if (!isByte && arg[0] != '#' && arg.substr(0,2) != "W#" && symbolTable.count(v)) isByte = (symbolTable[v].size == 1);
                        if (isByte) { binary.push_back(0xA9); binary.push_back(val & 0xFF); binary.push_back(0x48); } else { binary.push_back(0xF2); binary.push_back(val & 0xFF); binary.push_back((val >> 8) & 0xFF); }
                    }
                    binary.push_back(0x20); uint32_t a = symbolTable[stmt->instr.operand].value; binary.push_back(a & 0xFF); binary.push_back((a >> 8) & 0xFF);
                }
            } else {
                if (!isDeadCode) {
                    bool isQuad = (stmt->instr.mnemonic.size() > 1 && stmt->instr.mnemonic.back() == 'Q' && stmt->instr.mnemonic != "LDQ" && stmt->instr.mnemonic != "STQ" && stmt->instr.mnemonic != "BEQ" && stmt->instr.mnemonic != "BNE" && stmt->instr.mnemonic != "BRA" && stmt->instr.mnemonic != "BSR");
                    if (isQuad) { binary.push_back(0x42); binary.push_back(0x42); }
                    if (stmt->instr.mode == AddressingMode::FLAT_INDIRECT_Z) binary.push_back(0xEA);
                    bool isBranch = (stmt->instr.mnemonic == "BEQ" || stmt->instr.mnemonic == "BNE" || stmt->instr.mnemonic == "BRA" || stmt->instr.mnemonic == "BCC" || stmt->instr.mnemonic == "BCS" || stmt->instr.mnemonic == "BPL" || stmt->instr.mnemonic == "BMI" || stmt->instr.mnemonic == "BVC" || stmt->instr.mnemonic == "BVS" || stmt->instr.mnemonic == "BSR");
                    if (!isBranch) { uint8_t op = getOpcode(stmt->instr.mnemonic, stmt->instr.mode); if (op) binary.push_back(op); }
                    if (stmt->instr.mode == AddressingMode::IMMEDIATE || stmt->instr.mode == AddressingMode::STACK_RELATIVE || stmt->instr.mode == AddressingMode::BASE_PAGE_INDIRECT_Z || stmt->instr.mode == AddressingMode::FLAT_INDIRECT_Z || stmt->instr.mode == AddressingMode::BASE_PAGE_INDIRECT_SP_Y || stmt->instr.mode == AddressingMode::BASE_PAGE_X_INDIRECT || stmt->instr.mode == AddressingMode::BASE_PAGE_INDIRECT_Y || stmt->instr.mode == AddressingMode::BASE_PAGE || stmt->instr.mode == AddressingMode::BASE_PAGE_X || stmt->instr.mode == AddressingMode::BASE_PAGE_Y) {
                        uint32_t v; if (currentPass2Proc && currentPass2Proc->localArgs.count(stmt->instr.operand)) v = currentPass2Proc->localArgs[stmt->instr.operand]; else if (symbolTable.count(stmt->instr.operand)) v = symbolTable[stmt->instr.operand].value; else v = parseNumericLiteral(stmt->instr.operand);
                        binary.push_back((uint8_t)v);
                    } else if (stmt->instr.mode == AddressingMode::ABSOLUTE || stmt->instr.mode == AddressingMode::ABSOLUTE_X || stmt->instr.mode == AddressingMode::ABSOLUTE_Y || stmt->instr.mode == AddressingMode::ABSOLUTE_INDIRECT || stmt->instr.mode == AddressingMode::ABSOLUTE_X_INDIRECT || stmt->instr.mode == AddressingMode::IMMEDIATE16) {
                        uint32_t a = symbolTable.count(stmt->instr.operand) ? symbolTable[stmt->instr.operand].value : parseNumericLiteral(stmt->instr.operand);
                        binary.push_back(a & 0xFF); binary.push_back((a >> 8) & 0xFF);
                    } else if (stmt->instr.mnemonic == "BEQ" || stmt->instr.mnemonic == "BNE" || stmt->instr.mnemonic == "BRA" || stmt->instr.mnemonic == "BCC" || stmt->instr.mnemonic == "BCS" || stmt->instr.mnemonic == "BPL" || stmt->instr.mnemonic == "BMI" || stmt->instr.mnemonic == "BVC" || stmt->instr.mnemonic == "BVS") {
                        uint32_t t = symbolTable[stmt->instr.operand].value; int32_t offset2 = (int32_t)t - (int32_t)(stmt->address + 2); int32_t offset3 = (int32_t)t - (int32_t)(stmt->address + 3);
                        if (offset2 >= -128 && offset2 <= 127) { binary.push_back(getOpcode(stmt->instr.mnemonic, AddressingMode::RELATIVE)); binary.push_back((uint8_t)(int8_t)offset2); }
                        else { binary.push_back(getOpcode(stmt->instr.mnemonic, AddressingMode::RELATIVE16)); binary.push_back(offset3 & 0xFF); binary.push_back((offset3 >> 8) & 0xFF); }
                    } else if (stmt->instr.mnemonic == "BSR") {
                        uint32_t t = symbolTable[stmt->instr.operand].value; int32_t offset = (int32_t)t - (int32_t)(stmt->address + 3);
                        binary.push_back(getOpcode("BSR", AddressingMode::RELATIVE16)); binary.push_back(offset & 0xFF); binary.push_back((offset >> 8) & 0xFF);
                    } else if (stmt->instr.mode == AddressingMode::BASE_PAGE_RELATIVE) {
                        uint32_t v = symbolTable.count(stmt->instr.operand) ? symbolTable[stmt->instr.operand].value : parseNumericLiteral(stmt->instr.operand); binary.push_back((uint8_t)v);
                        uint32_t t = symbolTable[stmt->instr.bitBranchTarget].value; int32_t offset = (int32_t)t - (int32_t)(stmt->address + 3); binary.push_back((uint8_t)offset);
                    } else if (stmt->instr.mnemonic == "RTN") {
                        uint32_t v; if (currentPass2Proc && currentPass2Proc->localArgs.count(stmt->instr.operand)) v = currentPass2Proc->localArgs[stmt->instr.operand]; else if (symbolTable.count(stmt->instr.operand)) v = symbolTable[stmt->instr.operand].value; else v = parseNumericLiteral(stmt->instr.operand);
                        if (v == 0) binary.push_back(0x60); else { binary.push_back(0x62); binary.push_back((uint8_t)v); }
                    }
                }
                if (stmt->instr.mnemonic == "RTS" || stmt->instr.mnemonic == "RTN" || stmt->instr.mnemonic == "RTI") isDeadCode = true;
            }
        } else if (stmt->type == Statement::DIRECTIVE) {
            if (!isDeadCode || stmt->dir.name == "org") {
                if (stmt->dir.name == "var") {
                    if (stmt->dir.varType == Directive::ASSIGN) symbolTable[stmt->dir.varName].value = evaluateExpressionAt(stmt->dir.tokenIndex);
                    else if (stmt->dir.varType == Directive::INC) symbolTable[stmt->dir.varName].value++;
                    else if (stmt->dir.varType == Directive::DEC) symbolTable[stmt->dir.varName].value--;
                } else if (stmt->dir.name == "cleanup") { if (currentPass2Proc) currentPass2Proc->totalParamSize += evaluateExpressionAt(stmt->dir.tokenIndex); }
                else if (stmt->dir.name == "byte") for (const auto& a : stmt->dir.arguments) { if (a.empty()) continue; binary.push_back((uint8_t)parseNumericLiteral(a)); }
                else if (stmt->dir.name == "word") for (const auto& a : stmt->dir.arguments) { if (a.empty()) continue; uint32_t v = parseNumericLiteral(a); binary.push_back(v & 0xFF); binary.push_back((v >> 8) & 0xFF); }
                else if (stmt->dir.name == "dword" || stmt->dir.name == "long") for (const auto& a : stmt->dir.arguments) { if (a.empty()) continue; uint32_t v = parseNumericLiteral(a); binary.push_back(v & 0xFF); binary.push_back((v >> 8) & 0xFF); binary.push_back((v >> 16) & 0xFF); binary.push_back((v >> 24) & 0xFF); }
                else if (stmt->dir.name == "float") {
                    for (const auto& a : stmt->dir.arguments) {
                        if (a.empty()) continue; double v; bool neg = false; std::string cleanA = a; if (cleanA[0] == '-') { neg = true; cleanA = cleanA.substr(1); }
                        if (!cleanA.empty() && (cleanA[0] == '$' || cleanA[0] == '%')) v = (double)parseNumericLiteral(cleanA); else { try { v = std::stod(cleanA); } catch(...) { v = 0.0; } }
                        if (neg) v = -v; std::vector<uint8_t> encoded = encodeFloat(v); for (uint8_t b : encoded) binary.push_back(b);
                    }
                } else if (stmt->dir.name == "text") { if (!stmt->dir.arguments.empty()) for (char c : stmt->dir.arguments[0]) binary.push_back(toPetscii(c)); }
                else if (stmt->dir.name == "ascii") { if (!stmt->dir.arguments.empty()) for (char c : stmt->dir.arguments[0]) binary.push_back((uint8_t)c); }
            }
        }
    }
    return binary;
}
