#include "AssemblerParser.hpp"
#include "M65Emitter.hpp"
#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <memory>
#include <cmath>

// --- Support Functions ---

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

// --- AST Nodes ---

struct ExprAST {
    virtual ~ExprAST() = default;
    virtual uint32_t getValue(AssemblerParser* parser) const = 0;
    virtual bool isConstant(AssemblerParser* parser) const = 0;
    virtual bool is16Bit(AssemblerParser* parser) const = 0;
    virtual void emit(std::vector<uint8_t>& binary, AssemblerParser* parser, int width, const std::string& target) = 0;
};

struct ConstantNode : public ExprAST {
    uint32_t value;
    ConstantNode(uint32_t v) : value(v) {}
    uint32_t getValue(AssemblerParser* parser) const override { return value; }
    bool isConstant(AssemblerParser* parser) const override { return true; }
    bool is16Bit(AssemblerParser* parser) const override { return value > 0xFF; }
    void emit(std::vector<uint8_t>& binary, AssemblerParser* parser, int width, const std::string& target) override {
        if (!parser) return;
        M65Emitter e(binary, parser->getZPStart());
        e.lda_imm(value & 0xFF);
        if (width >= 16) e.ldx_imm((value >> 8) & 0xFF);
    }
};

struct RegisterNode : public ExprAST {
    std::string name;
    RegisterNode(const std::string& n) : name(n) {}
    uint32_t getValue(AssemblerParser* parser) const override { return 0; }
    bool isConstant(AssemblerParser* parser) const override { return false; }
    bool is16Bit(AssemblerParser* parser) const override { return name.size() > 1; }
    void emit(std::vector<uint8_t>& binary, AssemblerParser* parser, int width, const std::string& target) override {
        if (!parser) return;
        M65Emitter e(binary, parser->getZPStart());
        if (width <= 8) {
            if (name == ".X") e.txa();
            else if (name == ".Y") e.tya();
            else if (name == ".Z") e.tza();
            else if (name == ".SP") { e.tsx(); e.txa(); }
        } else {
            if (name == ".AY") { e.phy(); e.plx(); }
            else if (name == ".AZ") { e.phz(); e.plx(); }
            else if (name == ".XY") { e.txa(); e.phy(); e.plx(); }
            else if (name == ".YZ") { e.tya(); e.phz(); e.plx(); }
            else if (name == ".A") { e.ldx_imm(0); }
            else if (name == ".X") { e.txa(); e.ldx_imm(0); }
            else if (name == ".Y") { e.tya(); e.ldx_imm(0); }
            else if (name == ".Z") { e.tza(); e.ldx_imm(0); }
        }
    }
};

struct FlagNode : public ExprAST {
    char flag;
    FlagNode(char f) : flag(f) {}
    uint32_t getValue(AssemblerParser* parser) const override { return 0; }
    bool isConstant(AssemblerParser* parser) const override { return false; }
    bool is16Bit(AssemblerParser* parser) const override { return false; }
    void emit(std::vector<uint8_t>& binary, AssemblerParser* parser, int width, const std::string& target) override {
        if (!parser) return;
        M65Emitter e(binary, parser->getZPStart());
        if (flag == 'C') {
            e.lda_imm(0); e.adc_imm(0);
        } else {
            int8_t branchOp = 0;
            switch (flag) {
                case 'Z': branchOp = 0xD0; break;
                case 'V': branchOp = 0x50; break;
                case 'N': branchOp = 0x10; break;
            }
            if (branchOp != 0) {
                e.lda_imm(0); e.bne(0x02); e.lda_imm(1);
            } else {
                uint8_t mask = 0;
                if (flag == 'I') mask = 0x04; else if (flag == 'D') mask = 0x08; else if (flag == 'B') mask = 0x10;
                if (mask != 0) {
                    e.pha(); e.pla(); e.and_imm(mask); e.beq(0x02); e.lda_imm(1);
                } else { e.lda_imm(0); }
            }
        }
        if (width >= 16) e.ldx_imm(0);
    }
};

struct VariableNode : public ExprAST {
    std::string name;
    std::string scopePrefix;
    VariableNode(const std::string& n, const std::string& scope = "") : name(n), scopePrefix(scope) {}
    uint32_t getValue(AssemblerParser* parser) const override {
        if (!parser) return 0;
        Symbol* sym = parser->resolveSymbol(name, scopePrefix);
        return sym ? sym->value : 0;
    }
    bool isConstant(AssemblerParser* parser) const override {
        if (!parser) return false;
        Symbol* sym = parser->resolveSymbol(name, scopePrefix);
        return sym ? !sym->isAddress : false;
    }
    bool is16Bit(AssemblerParser* parser) const override {
        if (!parser) return true;
        Symbol* sym = parser->resolveSymbol(name, scopePrefix);
        return sym ? sym->size > 1 : true;
    }
    void emit(std::vector<uint8_t>& binary, AssemblerParser* parser, int width, const std::string& target) override {
        if (!parser) return;
        Symbol* sym = parser->resolveSymbol(name, scopePrefix);
        if (!sym) return;
        M65Emitter e(binary, parser->getZPStart());
        if (!sym->isAddress) {
            e.lda_imm(sym->value & 0xFF);
            if (width >= 16) e.ldx_imm((sym->value >> 8) & 0xFF);
        } else {
            if (sym->isStackRelative) {
                e.lda_stack((uint8_t)sym->stackOffset);
                if (width >= 16) e.ldx_imm(0); 
            } else {
                e.lda_abs(sym->value);
                if (width >= 16) e.ldx_abs(sym->value + 1);
            }
        }
    }
};

struct UnaryExpr : public ExprAST {
    std::string op;
    std::unique_ptr<ExprAST> operand;
    UnaryExpr(std::string o, std::unique_ptr<ExprAST> opnd) : op(o), operand(std::move(opnd)) {}
    uint32_t getValue(AssemblerParser* parser) const override {
        uint32_t val = operand ? operand->getValue(parser) : 0;
        if (op == "!") return val == 0 ? 1 : 0;
        if (op == "~") return ~val;
        if (op == "-") return -val;
        return 0;
    }
    bool isConstant(AssemblerParser* parser) const override { return operand ? operand->isConstant(parser) : true; }
    bool is16Bit(AssemblerParser* parser) const override { return operand ? operand->is16Bit(parser) : false; }
    void emit(std::vector<uint8_t>& binary, AssemblerParser* parser, int width, const std::string& target) override {
        if (!parser || !operand) return;
        operand->emit(binary, parser, width, target);
        M65Emitter e(binary, parser->getZPStart());
        if (op == "!") {
            e.bne(0x05); if (width >= 16) { e.txa(); e.bne(0x02); }
            e.lda_imm(1); e.bra(0x02); e.lda_imm(0);
            if (width >= 16) { e.tax(); e.ldx_imm(0); }
        } else if (op == "~") {
            e.eor_imm(0xFF);
            if (width >= 16) { e.pha(); e.txa(); e.eor_imm(0xFF); e.tax(); e.pla(); }
        } else if (op == "-") {
            e.neg_a();
            if (width >= 16) { e.pha(); e.txa(); e.eor_imm(0xFF); e.adc_imm(0); e.tax(); e.pla(); }
        }
    }
};

struct DereferenceNode : public ExprAST {
    std::unique_ptr<ExprAST> address;
    bool isFlat;
    DereferenceNode(std::unique_ptr<ExprAST> addr, bool flat = false) : address(std::move(addr)), isFlat(flat) {}
    uint32_t getValue(AssemblerParser* parser) const override { return 0; }
    bool isConstant(AssemblerParser* parser) const override { return false; }
    bool is16Bit(AssemblerParser* parser) const override { return true; }
    void emit(std::vector<uint8_t>& binary, AssemblerParser* parser, int width, const std::string& target) override {
        if (!parser || !address) return;
        M65Emitter e(binary, parser->getZPStart());
        address->emit(binary, parser, 16, ".AX");
        e.sta_s(0); e.stx_s(1);
        e.lda_ind_zs(0, isFlat);
        if (width >= 16) { e.inc_s(0); e.lda_ind_zs(0, isFlat); e.tax(); e.dec_s(0); }
    }
};

struct BinaryExpr : public ExprAST {
    std::string op;
    std::unique_ptr<ExprAST> left, right;
    BinaryExpr(std::string o, std::unique_ptr<ExprAST> l, std::unique_ptr<ExprAST> r)
        : op(o), left(std::move(l)), right(std::move(r)) {}
    uint32_t getValue(AssemblerParser* parser) const override {
        uint32_t l = left ? left->getValue(parser) : 0; uint32_t r = right ? right->getValue(parser) : 0;
        if (op == "+") return l + r;
        if (op == "-") return l - r;
        if (op == "*") return l * r;
        if (op == "/") { 
            if (r == 0) throw std::runtime_error("Division by zero in expression");
            return l / r; 
        }
        if (op == "&") return l & r;
        if (op == "|") return l | r;
        if (op == "^") return l ^ r;
        if (op == "<<") return l << r;
        if (op == ">>") return l >> r;
        if (op == "==") return l == r;
        if (op == "!=") return l != r;
        if (op == "<") return l < r;
        if (op == ">") return l > r;
        if (op == "<=") return l <= r;
        if (op == ">=") return l >= r;
        if (op == "&&") return l && r;
        if (op == "||") return l || r;
        return 0;
    }
    bool isConstant(AssemblerParser* parser) const override { return (left ? left->isConstant(parser) : true) && (right ? right->isConstant(parser) : true); }
    bool is16Bit(AssemblerParser* parser) const override { return getValue(parser) > 0xFF || (left && left->is16Bit(parser)) || (right && right->is16Bit(parser)); }
    void emit(std::vector<uint8_t>& binary, AssemblerParser* parser, int width, const std::string& target) override {
        if (!parser || !left || !right) return;
        M65Emitter e(binary, parser->getZPStart());
        int bytes = width / 8; if (bytes < 1) bytes = 1; if (bytes > 4) bytes = 4;

        if (op == "*" || op == "/" || op == "+" || op == "-") {
            left->emit(binary, parser, width, ".A");
            uint8_t base = (op == "/") ? 0x60 : 0x70;
            e.sta_abs(0xD700 + base); if (bytes >= 2) e.stx_abs(0xD701 + base);
            right->emit(binary, parser, width, ".A");
            e.sta_abs(0xD700 + base + 4); if (bytes >= 2) e.stx_abs(0xD701 + base + 4);
            if (op == "*") { for (int i = 0; i < bytes; ++i) { e.lda_abs(0xD778 + i); if (i == 1) e.tax(); } }
            else if (op == "/") { e.bit_abs(0xD70F); e.bne(-5); for (int i = 0; i < bytes; ++i) { e.lda_abs(0xD768 + i); if (i == 1) e.tax(); } }
            else if (op == "+") { for (int i = 0; i < bytes; ++i) { e.lda_abs(0xD77C + i); if (i == 1) e.tax(); } }
            else if (op == "-") { 
                e.sec(); 
                for (int i = 0; i < bytes; ++i) { 
                    e.lda_abs(0xD770 + i); 
                    e.sbc_abs(0xD774 + i); 
                    e.sta_abs(0xD770 + i); 
                } 
                e.lda_abs(0xD770); 
                if (bytes >= 2) e.ldx_abs(0xD771); 
            }
        } else if (op == "&" || op == "|" || op == "^") {
            left->emit(binary, parser, width, ".A");
            e.push_ax();
            right->emit(binary, parser, width, ".A");
            e.sta_s(0); if (width >= 16) e.stx_s(1);
            e.pop_ax();
            if (op == "&") e.and_s(0); else if (op == "|") e.ora_s(0); else e.eor_s(0);
            if (width >= 16) { e.pha(); e.txa(); if (op == "&") e.and_s(1); else if (op == "|") e.ora_s(1); else e.eor_s(1); e.tax(); e.pla(); }
        } else if (op == "<<" || op == ">>") {
            left->emit(binary, parser, width, ".A");
            e.push_ax();
            right->emit(binary, parser, width, ".A");
            e.sta_s(0);
            e.pop_ax();
            e.lda_s(0);
            e.beq((width >= 16) ? 0x07 : 0x05); e.dec_s(0);
            if (op == "<<") { e.asl_a(); if (width >= 16) { e.rol_a(); e.txa(); e.rol_a(); e.tax(); } }
            else { if (width >= 16) { e.txa(); e.lsr_a(); e.tax(); e.ror_a(); } else e.lsr_a(); }
            e.bra((width >= 16) ? -11 : -9);
        }
    }
};

// --- AssemblerParser Implementation ---

AssemblerParser::AssemblerParser(const std::vector<AssemblerToken>& tokens) : tokens(tokens), pos(0), pc(0) {}

AssemblerParser::AssemblerParser(const std::vector<AssemblerToken>& tokens, const std::map<std::string, uint32_t>& predefinedSymbols) 
    : tokens(tokens), pos(0), pc(0) 
{
    for (const auto& [name, val] : predefinedSymbols) {
        symbolTable[name] = {val, false, 2};
    }
}

const AssemblerToken& AssemblerParser::peek() const { if (pos >= tokens.size()) return tokens.back(); return tokens[pos]; }
const AssemblerToken& AssemblerParser::advance() { if (pos < tokens.size()) pos++; return tokens[pos - 1]; }
bool AssemblerParser::match(AssemblerTokenType type) { if (peek().type == type) { advance(); return true; } return false; }
const AssemblerToken& AssemblerParser::expect(AssemblerTokenType type, const std::string& message) { 
    if (peek().type == type) return advance(); 
    // TODO: Improve error handling to provide helpful suggestions instead of generic runtime_errors.
    throw std::runtime_error(message + " at " + std::to_string(peek().line) + ":" + std::to_string(peek().column)); 
}

static uint32_t parseNumericLiteral(const std::string& literal) {
    if (literal.empty()) return 0;
    try {
        // TODO: Add checks for integer overflow during parsing.
        if (literal[0] == '$') return std::stoul(literal.substr(1), nullptr, 16);
        if (literal[0] == '%') return std::stoul(literal.substr(1), nullptr, 2);
        return std::stoul(literal);
    } catch (...) { return 0; }
}

std::unique_ptr<ExprAST> parseExprAST(const std::vector<AssemblerToken>& tokens, int& idx, std::map<std::string, Symbol>& symbolTable, const std::string& scopePrefix = "") {
    auto parsePrimary = [&]() -> std::unique_ptr<ExprAST> {
        if (idx >= (int)tokens.size()) return nullptr;
        const auto& t = tokens[idx++];
        if (t.type == AssemblerTokenType::DECIMAL_LITERAL || t.type == AssemblerTokenType::HEX_LITERAL) {
            if (idx < (int)tokens.size() && tokens[idx].type == AssemblerTokenType::COMMA) {
                if (idx + 1 < (int)tokens.size() && (tokens[idx + 1].value == "s" || tokens[idx + 1].value == "S")) {
                    uint32_t val = (t.type == AssemblerTokenType::HEX_LITERAL) ? parseNumericLiteral(t.value) : std::stoul(t.value);
                    idx += 2;
                    std::string tempName = "__stack_" + std::to_string(val);
                    symbolTable[tempName] = {val, true, 2, false, 0, true, (int)val};
                    return std::make_unique<VariableNode>(tempName, "");
                }
            }
        }
        if (t.type == AssemblerTokenType::DECIMAL_LITERAL) return std::make_unique<ConstantNode>(std::stoul(t.value));
        if (t.type == AssemblerTokenType::HEX_LITERAL) return std::make_unique<ConstantNode>(parseNumericLiteral(t.value));
        if (t.type == AssemblerTokenType::BINARY_LITERAL) return std::make_unique<ConstantNode>(parseNumericLiteral(t.value));
        if (t.type == AssemblerTokenType::REGISTER) return std::make_unique<RegisterNode>(t.value);
        if (t.type == AssemblerTokenType::FLAG) return std::make_unique<FlagNode>(t.value[0]);
        if (t.type == AssemblerTokenType::IDENTIFIER) return std::make_unique<VariableNode>(t.value, scopePrefix);
        if (t.type == AssemblerTokenType::STAR) return std::make_unique<DereferenceNode>(parseExprAST(tokens, idx, symbolTable, scopePrefix));
        if (t.type == AssemblerTokenType::OPEN_BRACKET) { auto addr = parseExprAST(tokens, idx, symbolTable, scopePrefix); if (idx < (int)tokens.size() && tokens[idx].type == AssemblerTokenType::CLOSE_BRACKET) idx++; return std::make_unique<DereferenceNode>(std::move(addr), true); }
        if (t.type == AssemblerTokenType::BANG || t.type == AssemblerTokenType::TILDE || (t.type == AssemblerTokenType::MINUS && idx < (int)tokens.size() && tokens[idx].type != AssemblerTokenType::DECIMAL_LITERAL)) { std::string op = t.value; return std::make_unique<UnaryExpr>(op, parseExprAST(tokens, idx, symbolTable, scopePrefix)); }
        if (t.type == AssemblerTokenType::OPEN_PAREN) { auto expr = parseExprAST(tokens, idx, symbolTable, scopePrefix); if (idx < (int)tokens.size() && tokens[idx].type == AssemblerTokenType::CLOSE_PAREN) idx++; return expr; }
        return nullptr;
    };

    auto parseMultiplicative = [&]() -> std::unique_ptr<ExprAST> {
        auto left = parsePrimary();
        while (idx < (int)tokens.size() && (tokens[idx].type == AssemblerTokenType::STAR || tokens[idx].type == AssemblerTokenType::SLASH)) {
            std::string op = tokens[idx++].value; auto right = parsePrimary(); left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
        }
        return left;
    };

    auto parseAdditive = [&]() -> std::unique_ptr<ExprAST> {
        auto left = parseMultiplicative();
        while (idx < (int)tokens.size() && (tokens[idx].type == AssemblerTokenType::PLUS || tokens[idx].type == AssemblerTokenType::MINUS)) {
            std::string op = tokens[idx++].value; auto right = parseMultiplicative(); left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
        }
        return left;
    };

    auto parseShift = [&]() -> std::unique_ptr<ExprAST> {
        auto left = parseAdditive();
        while (idx < (int)tokens.size() && (tokens[idx].type == AssemblerTokenType::LSHIFT || tokens[idx].type == AssemblerTokenType::RSHIFT)) {
            std::string op = tokens[idx++].value; auto right = parseAdditive(); left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
        }
        return left;
    };

    return parseShift(); 
}

uint32_t AssemblerParser::evaluateExpressionAt(int index, const std::string& scopePrefix) {
    int idx = index; 
    auto ast = parseExprAST(tokens, idx, symbolTable, scopePrefix);
    if (!ast) {
        throw std::runtime_error("Expected expression at " + std::to_string(tokens[index].line) + ":" + std::to_string(tokens[index].column));
    }
    return ast->getValue(this);
}

Symbol* AssemblerParser::resolveSymbol(const std::string& name, const std::string& scopePrefix) {
    std::string current = scopePrefix;
    while (true) {
        std::string fullName = current + name;
        if (symbolTable.count(fullName)) {
            return &symbolTable.at(fullName);
        }
        if (current.empty()) break;
        if (current.size() < 2) { current = ""; continue; }
        size_t pos = current.find_last_of(':', current.size() - 2);
        if (pos == std::string::npos) current = "";
        else current = current.substr(0, pos + 1);
    }
    return nullptr;
}

uint32_t AssemblerParser::getZPStart() const {
    if (symbolTable.count("ca45.zeroPageStart")) {
        return symbolTable.at("ca45.zeroPageStart").value;
    }
    return 0x02;
}

void AssemblerParser::pass1() {
    pc = 0; pos = 0; statements.clear(); scopeStack.clear(); nextScopeId = 0;
    std::vector<ProcContext*> pass1ProcStack;
    auto currentScopePrefix = [&]() {
        std::string prefix = "";
        for (const auto& s : scopeStack) prefix += s + ":";
        return prefix;
    };

    while (pos < tokens.size()) {
        while (match(AssemblerTokenType::NEWLINE));
        if (peek().type == AssemblerTokenType::END_OF_FILE) break;
        auto stmt = std::make_unique<Statement>();
        stmt->address = pc; stmt->line = peek().line;
        stmt->scopePrefix = currentScopePrefix();

        if (peek().type == AssemblerTokenType::IDENTIFIER && pos + 1 < tokens.size() && tokens[pos+1].type == AssemblerTokenType::COLON) {
            stmt->label = stmt->scopePrefix + advance().value; advance(); symbolTable[stmt->label] = {pc, true, 2};
        }

        if (peek().type == AssemblerTokenType::NEWLINE) {
            if (stmt->label.empty()) {
                advance();
                continue;
            }
        }

        if (match(AssemblerTokenType::OPEN_CURLY)) {
            scopeStack.push_back("_S" + std::to_string(nextScopeId++));
            stmt->size = 0;
            statements.push_back(std::move(stmt));
            continue;
        } else if (match(AssemblerTokenType::CLOSE_CURLY)) {
            if (!scopeStack.empty()) scopeStack.pop_back();
            stmt->size = 0;
            statements.push_back(std::move(stmt));
            continue;
        } else if (match(AssemblerTokenType::DIRECTIVE)) {
            // ... existing directive handling ...
            stmt->type = Statement::DIRECTIVE; stmt->dir.name = tokens[pos-1].value;
            if (stmt->dir.name == "var") {
                std::string varName = expect(AssemblerTokenType::IDENTIFIER, "Expected var name").value;
                std::string scopedVarName = stmt->scopePrefix + varName;
                stmt->dir.varName = scopedVarName;
                if (match(AssemblerTokenType::EQUALS)) {
                    stmt->dir.varType = Directive::ASSIGN; stmt->dir.tokenIndex = (int)pos;
                    uint32_t val = evaluateExpressionAt((int)pos, stmt->scopePrefix);
                    while (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) advance();
                    symbolTable[scopedVarName] = {val, false, 2, true, val};
                } else if (match(AssemblerTokenType::INCREMENT)) { 
                    stmt->dir.varType = Directive::INC; 
                    if (symbolTable.count(scopedVarName)) symbolTable[scopedVarName].value++; 
                }
                else if (match(AssemblerTokenType::DECREMENT)) { 
                    stmt->dir.varType = Directive::DEC; 
                    if (symbolTable.count(scopedVarName)) symbolTable[scopedVarName].value--; 
                }
                stmt->size = 0;
            } else if (stmt->dir.name == "cleanup") {
                stmt->dir.tokenIndex = (int)pos; uint32_t val = evaluateExpressionAt((int)pos, stmt->scopePrefix);
                while (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) advance();
                if (currentProc) { currentProc->totalParamSize += val; }
                stmt->size = 0;
            } else if (stmt->dir.name == "basicUpstart") { stmt->type = Statement::BASIC_UPSTART; stmt->basicUpstartTokenIndex = (int)pos; while (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) advance(); stmt->size = 12; }
            else {
                while (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) {
                    if (peek().type == AssemblerTokenType::COMMA) { advance(); continue; }
                    std::string val = advance().value; 
                    if (!val.empty()) stmt->dir.arguments.push_back(val);
                }
                if (stmt->dir.name == "org") { 
                    stmt->address = pc; 
                    if (!stmt->dir.arguments.empty()) { pc = parseNumericLiteral(stmt->dir.arguments[0]); }
                    stmt->size = 0; 
                }
                else if (stmt->dir.name == "cpu") { stmt->size = 0; }
                else { stmt->size = calculateDirectiveSize(stmt->dir); }
            }
        } else if (peek().type == AssemblerTokenType::STAR && pos + 1 < tokens.size() && tokens[pos+1].type == AssemblerTokenType::EQUALS) {
            advance(); advance(); stmt->type = Statement::DIRECTIVE; stmt->dir.name = "org"; stmt->address = pc;
            while (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) { if (peek().type == AssemblerTokenType::COMMA) { advance(); continue; } stmt->dir.arguments.push_back(advance().value); }
            if (!stmt->dir.arguments.empty()) { pc = parseNumericLiteral(stmt->dir.arguments[0]); }
            stmt->size = 0;
        } else if (match(AssemblerTokenType::INSTRUCTION)) {
            stmt->type = Statement::INSTRUCTION; stmt->instr.mnemonic = tokens[pos-1].value;
            std::transform(stmt->instr.mnemonic.begin(), stmt->instr.mnemonic.end(), stmt->instr.mnemonic.begin(), ::toupper);
            if (stmt->instr.mnemonic == "PROC") {
                std::string procName = advance().value; stmt->label = procName; symbolTable[procName] = {pc, true, 2};
                ProcContext ctx; ctx.name = procName; ctx.totalParamSize = 0; std::vector<std::pair<std::string, int>> args;
                while (match(AssemblerTokenType::COMMA)) {
                    bool isByte = false; 
                    if (peek().type == AssemblerTokenType::IDENTIFIER && (peek().value == "B" || peek().value == "W")) { isByte = (advance().value == "B"); match(AssemblerTokenType::HASH); }
                    std::string argName = advance().value; int size = isByte ? 1 : 2; args.push_back({argName, size}); ctx.totalParamSize += size;
                }

                scopeStack.push_back(procName);
                stmt->scopePrefix = currentScopePrefix();

                int currentOffset = 2;
                for (int i = (int)args.size() - 1; i >= 0; --i) {
                    std::string scopedArg = stmt->scopePrefix + args[i].first;
                    std::string scopedArgN = stmt->scopePrefix + "ARG" + std::to_string(i + 1);
                    ctx.localArgs[args[i].first] = currentOffset; ctx.localArgs["ARG" + std::to_string(i + 1)] = currentOffset;
                    symbolTable[scopedArg] = {(uint32_t)currentOffset, false, (int)args[i].second, true, (uint32_t)currentOffset, false, currentOffset};
                    symbolTable[scopedArgN] = {(uint32_t)currentOffset, false, (int)args[i].second, true, (uint32_t)currentOffset, false, currentOffset};
                    currentOffset += args[i].second;
                }
                auto res = procedures.emplace(pc, ctx); 
                pass1ProcStack.push_back(currentProc);
                currentProc = &res.first->second; stmt->procCtx = currentProc; stmt->size = 0;
            } else if (stmt->instr.mnemonic == "ENDPROC") { 
                if (currentProc) { 
                    stmt->instr.procParamSize = currentProc->totalParamSize; 
                    stmt->procCtx = currentProc; 
                    currentProc = pass1ProcStack.empty() ? nullptr : pass1ProcStack.back();
                    if (!pass1ProcStack.empty()) pass1ProcStack.pop_back();
                } 
                if (!scopeStack.empty()) scopeStack.pop_back();
                stmt->size = 2; 
            }
            else if (stmt->instr.mnemonic == "CALL") {
                stmt->instr.operand = advance().value;
                while (match(AssemblerTokenType::COMMA)) {
                    if (match(AssemblerTokenType::HASH)) { stmt->instr.callArgs.push_back(std::string("#") + advance().value); }
                    else if (peek().type == AssemblerTokenType::IDENTIFIER && (peek().value == "B" || peek().value == "W")) { std::string p = advance().value; match(AssemblerTokenType::HASH); stmt->instr.callArgs.push_back(p + "#" + advance().value); }
                    else { stmt->instr.callArgs.push_back(advance().value); }
                }
                stmt->size = calculateInstructionSize(stmt->instr, pc, stmt->scopePrefix);
            } else if (stmt->instr.mnemonic == "EXPR") {
                stmt->type = Statement::EXPR; const auto& target = advance(); stmt->exprTarget = (target.type == AssemblerTokenType::REGISTER ? "." : "") + target.value;
                expect(AssemblerTokenType::COMMA, "Expected ,"); stmt->exprTokenIndex = (int)pos;
                while (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) advance();
                std::vector<uint8_t> dummy; emitExpressionCode(dummy, stmt->exprTarget, stmt->exprTokenIndex, stmt->scopePrefix); stmt->size = dummy.size();
            } else if (stmt->instr.mnemonic.substr(0, 3) == "MUL" || stmt->instr.mnemonic.substr(0, 3) == "DIV") {
                stmt->type = (stmt->instr.mnemonic.substr(0, 3) == "MUL") ? Statement::MUL : Statement::DIV;
                std::string m = stmt->instr.mnemonic; if (m.size() > 4 && m[3] == '.') stmt->mulWidth = std::stoi(m.substr(4)); else stmt->mulWidth = 8;
                const auto& dest = advance(); stmt->instr.operand = (dest.type == AssemblerTokenType::REGISTER ? "." : "") + dest.value;
                if (!match(AssemblerTokenType::COMMA)) throw std::runtime_error("Expected , after destination");
                stmt->exprTokenIndex = (int)pos; while (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) advance();
                std::vector<uint8_t> dummy; if (stmt->type == Statement::MUL) emitMulCode(dummy, stmt->mulWidth, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix); else emitDivCode(dummy, stmt->mulWidth, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix); stmt->size = dummy.size();
            } else {
                if (match(AssemblerTokenType::HASH)) {
                    stmt->instr.operandTokenIndex = (int)pos;
                    std::string op;
                    while (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE && peek().type != AssemblerTokenType::COMMA) {
                        op += advance().value;
                    }
                    stmt->instr.operand = op;
                    if (stmt->instr.mnemonic == "PHW") stmt->instr.mode = AddressingMode::IMMEDIATE16; else stmt->instr.mode = AddressingMode::IMMEDIATE;
                }
                else if (match(AssemblerTokenType::OPEN_PAREN)) {
                    stmt->instr.operandTokenIndex = (int)pos;
                    stmt->instr.operand = advance().value;
                    if (match(AssemblerTokenType::COMMA)) {
                        std::string r = advance().value; if (r == "X" || r == "x") { expect(AssemblerTokenType::CLOSE_PAREN, "Expected )"); try { uint32_t val = parseNumericLiteral(stmt->instr.operand); if (val > 0xFF || stmt->instr.mnemonic == "JSR" || stmt->instr.mnemonic == "JMP") stmt->instr.mode = AddressingMode::ABSOLUTE_X_INDIRECT; else stmt->instr.mode = AddressingMode::BASE_PAGE_X_INDIRECT; } catch(...) { stmt->instr.mode = AddressingMode::ABSOLUTE_X_INDIRECT; } }
                        else if (r == "SP" || r == "sp") { expect(AssemblerTokenType::CLOSE_PAREN, "Expected )"); expect(AssemblerTokenType::COMMA, "Expected ,"); advance(); stmt->instr.mode = AddressingMode::STACK_RELATIVE; }
                    } else {
                        expect(AssemblerTokenType::CLOSE_PAREN, "Expected )");
                        if (match(AssemblerTokenType::COMMA)) { std::string r = advance().value; if (r == "Y" || r == "y") stmt->instr.mode = AddressingMode::BASE_PAGE_INDIRECT_Y; else if (r == "Z" || r == "z") stmt->instr.mode = AddressingMode::BASE_PAGE_INDIRECT_Z; }
                        else { try { uint32_t val = parseNumericLiteral(stmt->instr.operand); if (val > 0xFF || stmt->instr.mnemonic == "JSR" || stmt->instr.mnemonic == "JMP") stmt->instr.mode = AddressingMode::ABSOLUTE_INDIRECT; else stmt->instr.mode = AddressingMode::INDIRECT; } catch(...) { stmt->instr.mode = AddressingMode::ABSOLUTE_INDIRECT; } }
                    }
                } else if (match(AssemblerTokenType::OPEN_BRACKET)) { stmt->instr.operandTokenIndex = (int)pos; stmt->instr.operand = advance().value; expect(AssemblerTokenType::CLOSE_BRACKET, "Expected ]"); expect(AssemblerTokenType::COMMA, "Expected ,"); advance(); stmt->instr.mode = AddressingMode::FLAT_INDIRECT_Z; }
                else if (peek().type == AssemblerTokenType::REGISTER && (peek().value == "A" || peek().value == "a")) { advance(); stmt->instr.mode = AddressingMode::ACCUMULATOR; }
                else if (peek().type == AssemblerTokenType::IDENTIFIER || peek().type == AssemblerTokenType::HEX_LITERAL || peek().type == AssemblerTokenType::DECIMAL_LITERAL) {
                    stmt->instr.operandTokenIndex = (int)pos;
                    std::string op = advance().value;
                    while (peek().type != AssemblerTokenType::COMMA && peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) {
                        op += advance().value;
                    }
                    stmt->instr.operand = op;
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
                stmt->size = calculateInstructionSize(stmt->instr, pc, stmt->scopePrefix);
            }
        } else if (stmt->label.empty()) { 
            advance(); 
            continue; 
        }
        pc += stmt->size; statements.push_back(std::move(stmt));
    }

    // Resolve labels and branch sizes iteratively
    bool changed = true;
    for (int iter = 0; iter < 10 && changed; ++iter) {
        changed = false; pc = 0;
        for (auto& stmt : statements) {
            if (stmt->type == Statement::DIRECTIVE && stmt->dir.name == "org") { if (!stmt->dir.arguments.empty()) pc = parseNumericLiteral(stmt->dir.arguments[0]); stmt->address = pc; stmt->size = 0; }
            else {
                stmt->address = pc;
                if (!stmt->label.empty() && symbolTable.count(stmt->label)) { if (symbolTable.at(stmt->label).value != pc) { symbolTable.at(stmt->label).value = pc; changed = true; } }
                int oldSize = stmt->size;
                if (stmt->type == Statement::INSTRUCTION) { stmt->size = calculateInstructionSize(stmt->instr, stmt->address, stmt->scopePrefix); }
                else if (stmt->type == Statement::MUL) { std::vector<uint8_t> dummy; emitMulCode(dummy, stmt->mulWidth, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix); stmt->size = dummy.size(); }
                else if (stmt->type == Statement::DIV) { std::vector<uint8_t> dummy; emitDivCode(dummy, stmt->mulWidth, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix); stmt->size = dummy.size(); }
                else if (stmt->type == Statement::EXPR) { std::vector<uint8_t> dummy; emitExpressionCode(dummy, stmt->exprTarget, stmt->exprTokenIndex, stmt->scopePrefix); stmt->size = dummy.size(); }
                if (stmt->size != oldSize) changed = true;
                pc += stmt->size;
            }
        }
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

int AssemblerParser::calculateExprSize(int tokenIndex, const std::string& scopePrefix) {
    int idx = tokenIndex; auto ast = parseExprAST(tokens, idx, symbolTable, scopePrefix); if (!ast) return 0;
    std::vector<uint8_t> dummy; ast->emit(dummy, this, 16, ".AX"); return dummy.size();
}

int AssemblerParser::calculateInstructionSize(const Instruction& instr, uint32_t currentAddr, const std::string& scopePrefix) {
    if (instr.mnemonic == "PROC") return 0; if (instr.mnemonic == "ENDPROC") return 2;
    if (instr.mnemonic == "CALL") return 3 + (int)instr.callArgs.size() * 3;
    int size = 0; bool isQuad = (instr.mnemonic.size() > 1 && instr.mnemonic.back() == 'Q' && instr.mnemonic != "LDQ" && instr.mnemonic != "STQ" && instr.mnemonic != "BEQ" && instr.mnemonic != "BNE" && instr.mnemonic != "BRA" && instr.mnemonic != "BSR");
    if (isQuad) size += 2; if (instr.mode == AddressingMode::FLAT_INDIRECT_Z) size += 1;
    if (instr.mnemonic == "PHW") size += 3; else if (instr.mnemonic == "RTN") size += 2; 
    else if (instr.mnemonic == "BSR") size += 3;
    else if (instr.mnemonic == "BRA" || (instr.mnemonic[0] == 'B' && instr.mnemonic.size() == 3 && instr.mnemonic != "BIT" && instr.mnemonic != "BRK" && instr.mnemonic.substr(0,3) != "BBR" && instr.mnemonic.substr(0,3) != "BBS")) {
        Symbol* sym = resolveSymbol(instr.operand, scopePrefix);
        if (sym) {
            uint32_t target = sym->value;
            int32_t offset = (int32_t)target - (int32_t)(currentAddr + 2);
            if (offset >= -128 && offset <= 127) size = 2; else size = 3;
        } else size = 3; 
    } else {
        switch (instr.mode) {
            case AddressingMode::IMPLIED: case AddressingMode::ACCUMULATOR: size += 1; break;
            case AddressingMode::IMMEDIATE: case AddressingMode::STACK_RELATIVE: case AddressingMode::RELATIVE: case AddressingMode::BASE_PAGE: case AddressingMode::BASE_PAGE_X: case AddressingMode::BASE_PAGE_Y: case AddressingMode::BASE_PAGE_X_INDIRECT: case AddressingMode::BASE_PAGE_INDIRECT_Y: case AddressingMode::BASE_PAGE_INDIRECT_Z: case AddressingMode::FLAT_INDIRECT_Z: size += 2; break;
            case AddressingMode::IMMEDIATE16: case AddressingMode::RELATIVE16: case AddressingMode::BASE_PAGE_RELATIVE: default: size += 3; break;
        }
    }
    return size;
}

std::string AssemblerParser::AddressingModeToString(AddressingMode mode) {
    switch (mode) {
        case AddressingMode::IMPLIED: return "Implied";
        case AddressingMode::ACCUMULATOR: return "A";
        case AddressingMode::IMMEDIATE: return "#imm";
        case AddressingMode::IMMEDIATE16: return "#imm16";
        case AddressingMode::BASE_PAGE: return "zp";
        case AddressingMode::BASE_PAGE_X: return "zp,X";
        case AddressingMode::BASE_PAGE_Y: return "zp,Y";
        case AddressingMode::ABSOLUTE: return "abs";
        case AddressingMode::ABSOLUTE_X: return "abs,X";
        case AddressingMode::ABSOLUTE_Y: return "abs,Y";
        case AddressingMode::INDIRECT: return "(zp)";
        case AddressingMode::BASE_PAGE_X_INDIRECT: return "(zp,X)";
        case AddressingMode::BASE_PAGE_INDIRECT_Y: return "(zp),Y";
        case AddressingMode::BASE_PAGE_INDIRECT_Z: return "(zp),Z";
        case AddressingMode::BASE_PAGE_INDIRECT_SP_Y: return "(zp,SP),Y";
        case AddressingMode::ABSOLUTE_INDIRECT: return "(abs)";
        case AddressingMode::ABSOLUTE_X_INDIRECT: return "(abs,X)";
        case AddressingMode::RELATIVE: return "rel";
        case AddressingMode::RELATIVE16: return "rel16";
        case AddressingMode::BASE_PAGE_RELATIVE: return "zp,rel";
        case AddressingMode::FLAT_INDIRECT_Z: return "[zp],Z";
        case AddressingMode::QUAD_Q: return "Q";
        case AddressingMode::STACK_RELATIVE: return "offset,S";
        default: return "Unknown";
    }
}

static const std::map<std::pair<std::string, AddressingMode>, uint8_t>& getOpcodeMap() {
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
        { {"SEC", AddressingMode::IMPLIED}, 0x38 }, { {"SED", AddressingMode::IMPLIED}, 0xF8 }, { {"SEE", AddressingMode::IMPLIED}, 0x03 }, { {"SEI", AddressingMode::IMPLIED}, 0x78 }, { {"SMB0", AddressingMode::BASE_PAGE}, 0x87 }, { {"SMB1", AddressingMode::BASE_PAGE}, 0x97 }, { {"SMB2", AddressingMode::BASE_PAGE}, 0xA7 }, { {"SMB3", AddressingMode::BASE_PAGE}, 0xB7 }, { {"SMB4", AddressingMode::BASE_PAGE}, 0xC7 }, { {"SMB5", AddressingMode::BASE_PAGE}, 0xD7 }, { {"SMB6", AddressingMode::BASE_PAGE}, 0xE7 }, { {"SMB7", AddressingMode::BASE_PAGE}, 0xF7 },
        { {"STA", AddressingMode::BASE_PAGE_INDIRECT_Y}, 0x91 }, { {"STA", AddressingMode::BASE_PAGE_INDIRECT_Z}, 0x92 }, { {"STA", AddressingMode::STACK_RELATIVE}, 0x82 }, { {"STA", AddressingMode::BASE_PAGE_X_INDIRECT}, 0x81 }, { {"STA", AddressingMode::ABSOLUTE}, 0x8D }, { {"STA", AddressingMode::ABSOLUTE_X}, 0x9D }, { {"STA", AddressingMode::ABSOLUTE_Y}, 0x99 }, { {"STA", AddressingMode::BASE_PAGE}, 0x85 }, { {"STA", AddressingMode::BASE_PAGE_X}, 0x95 },
        { {"STX", AddressingMode::ABSOLUTE}, 0x8E }, { {"STX", AddressingMode::ABSOLUTE_Y}, 0x9B }, { {"STX", AddressingMode::BASE_PAGE}, 0x86 }, { {"STX", AddressingMode::BASE_PAGE_Y}, 0x96 },
        { {"STY", AddressingMode::ABSOLUTE}, 0x8C }, { {"STY", AddressingMode::ABSOLUTE_X}, 0x8B }, { {"STY", AddressingMode::BASE_PAGE}, 0x84 }, { {"STY", AddressingMode::BASE_PAGE_X}, 0x94 },
        { {"STZ", AddressingMode::ABSOLUTE}, 0x9C }, { {"STZ", AddressingMode::ABSOLUTE_X}, 0x9E }, { {"STZ", AddressingMode::BASE_PAGE}, 0x64 }, { {"STZ", AddressingMode::BASE_PAGE_X}, 0x74 }, { {"STQ", AddressingMode::BASE_PAGE}, 0x85 }, { {"STQ", AddressingMode::ABSOLUTE}, 0x8D }, { {"STQ", AddressingMode::BASE_PAGE_INDIRECT_Z}, 0x92 }, { {"STQ", AddressingMode::FLAT_INDIRECT_Z}, 0x92 },
        { {"TAB", AddressingMode::IMPLIED}, 0x5B }, { {"TAX", AddressingMode::IMPLIED}, 0xAA }, { {"TAY", AddressingMode::IMPLIED}, 0xA8 }, { {"TAZ", AddressingMode::IMPLIED}, 0x4B }, { {"TBA", AddressingMode::IMPLIED}, 0x7B }, { {"TRB", AddressingMode::ABSOLUTE}, 0x1C }, { {"TRB", AddressingMode::BASE_PAGE}, 0x14 }, { {"TSB", AddressingMode::ABSOLUTE}, 0x0C }, { {"TSB", AddressingMode::BASE_PAGE}, 0x04 }, { {"TSX", AddressingMode::IMPLIED}, 0xBA }, { {"TSY", AddressingMode::IMPLIED}, 0x0B }, { {"TXA", AddressingMode::IMPLIED}, 0x8A }, { {"TXS", AddressingMode::IMPLIED}, 0x9A }, { {"TYA", AddressingMode::IMPLIED}, 0x98 }, { {"TYS", AddressingMode::IMPLIED}, 0x2B }, { {"TZA", AddressingMode::IMPLIED}, 0x6B },
    };
    return opcodes;
}

uint8_t AssemblerParser::getOpcode(const std::string& m, AddressingMode mode) {
    std::string baseM = m; 
    if (m.size() > 1 && m.back() == 'Q' && m != "LDQ" && m != "STQ" && m.substr(0, 3) != "BEQ" && m.substr(0, 3) != "BNE" && m.substr(0, 3) != "BRA" && m.substr(0, 3) != "BSR") {
        baseM = m.substr(0, m.size() - 1);
    }
    const auto& opcodes = getOpcodeMap();
    auto it = opcodes.find({baseM, mode});
    if (it != opcodes.end()) return it->second;
    return 0;
}

std::vector<AddressingMode> AssemblerParser::getValidAddressingModes(const std::string& mnemonic) {
    std::string baseM = mnemonic;
    if (mnemonic.size() > 1 && mnemonic.back() == 'Q' && mnemonic != "LDQ" && mnemonic != "STQ" && mnemonic.substr(0, 3) != "BEQ" && mnemonic.substr(0, 3) != "BNE" && mnemonic.substr(0, 3) != "BRA" && mnemonic.substr(0, 3) != "BSR") {
        baseM = mnemonic.substr(0, mnemonic.size() - 1);
    }
    std::vector<AddressingMode> modes;
    const auto& opcodes = getOpcodeMap();
    for (const auto& entry : opcodes) {
        if (entry.first.first == baseM) {
            modes.push_back(entry.first.second);
        }
    }
    return modes;
}

void AssemblerParser::emitExpressionCode(std::vector<uint8_t>& binary, const std::string& target, int tokenIndex, const std::string& scopePrefix) {
    int idx = tokenIndex; auto ast = parseExprAST(tokens, idx, symbolTable, scopePrefix); if (!ast) return;
    M65Emitter e(binary, getZPStart());
    int width = 8; if (target == ".AX" || target == ".AY" || target == ".AZ" || target == ".XY") width = 16; else if (target == ".AXY") width = 24; else if (target == ".Q" || target == ".AXYZ") width = 32; else if (target[0] != '.') width = 16;
    if (ast->isConstant(this)) { uint32_t val = ast->getValue(this); e.lda_imm(val & 0xFF); if (width >= 16) e.ldx_imm((val >> 8) & 0xFF); }
    else ast->emit(binary, this, width, target);
    if (target == ".X") e.tax(); else if (target == ".Y") e.tay(); else if (target == ".Z") e.taz();
    else if (target[0] != '.') { 
        Symbol* sym = resolveSymbol(target, scopePrefix);
        uint32_t addr = sym ? sym->value : parseNumericLiteral(target); 
        e.sta_abs(addr); if (width >= 16) e.stx_abs(addr + 1); 
    }
}

void AssemblerParser::emitMulCode(std::vector<uint8_t>& binary, int width, const std::string& dest, int tokenIndex, const std::string& scopePrefix) {
    int idx = tokenIndex; auto srcAst = parseExprAST(tokens, idx, symbolTable, scopePrefix); if (!srcAst) return;
    M65Emitter e(binary, getZPStart());
    int bytes = width / 8; if (bytes < 1) bytes = 1; if (bytes > 4) bytes = 4;

    bool isStackRel = false;
    int stackOff = 0;
    if (idx < (int)tokens.size() && tokens[idx].type == AssemblerTokenType::COMMA) {
        if (idx + 1 < (int)tokens.size() && (tokens[idx + 1].value == "s" || tokens[idx + 1].value == "S")) {
            isStackRel = true;
            stackOff = srcAst->getValue(this);
        }
    }

    auto storeMath = [&](uint8_t regAddr, int byteIdx, const std::string& src) {
        if (src == ".A") e.sta_abs(0xD700 + regAddr + byteIdx);
        else if (src == ".X") e.stx_abs(0xD700 + regAddr + byteIdx);
        else if (src == ".Y") e.sty_abs(0xD700 + regAddr + byteIdx);
        else if (src == ".Z") e.stz_abs(0xD700 + regAddr + byteIdx);
        else {
            Symbol* sym = resolveSymbol(src, scopePrefix);
            if (sym && sym->isStackRelative) {
                e.lda_stack((uint8_t)(sym->stackOffset + byteIdx));
                e.sta_abs(0xD700 + regAddr + byteIdx);
            } else {
                uint32_t addr = sym ? sym->value : parseNumericLiteral(src); 
                e.lda_abs(addr + byteIdx); e.sta_abs(0xD700 + regAddr + byteIdx); 
            }
        }
    };

    if (dest == ".A" || dest == ".AX" || dest == ".AXY" || dest == ".AXYZ" || dest == ".Q") { if (bytes >= 1) storeMath(0x70, 0, ".A"); if (bytes >= 2) storeMath(0x70, 1, ".X"); if (bytes >= 3) storeMath(0x70, 2, ".Y"); if (bytes >= 4) storeMath(0x70, 3, ".Z"); } else for (int i = 0; i < bytes; ++i) storeMath(0x70, i, dest);

    if (isStackRel) {
        for (int i = 0; i < bytes; ++i) { e.lda_stack((uint8_t)(stackOff + i)); e.sta_abs(0xD774 + i); }
    } else if (srcAst->isConstant(this)) { 
        uint32_t val = srcAst->getValue(this); for (int i = 0; i < bytes; ++i) { e.lda_imm((val >> (i * 8)) & 0xFF); e.sta_abs(0xD774 + i); } 
    }
    else { int tIdx = tokenIndex; std::string srcName = tokens[tIdx].value; if (tokens[tIdx].type == AssemblerTokenType::REGISTER) srcName = "." + srcName; if (srcName == ".A" || srcName == ".AX" || srcName == ".AXY" || srcName == ".AXYZ" || srcName == ".Q") { if (bytes >= 1) storeMath(0x74, 0, ".A"); if (bytes >= 2) storeMath(0x74, 1, ".X"); if (bytes >= 3) storeMath(0x74, 2, ".Y"); if (bytes >= 4) storeMath(0x74, 3, ".Z"); } else for (int i = 0; i < bytes; ++i) storeMath(0x74, i, srcName); }
    for (int i = 0; i < bytes; ++i) { e.lda_abs(0xD778 + i); if (dest == ".A" || dest == ".AX" || dest == ".AXY" || dest == ".AXYZ" || dest == ".Q") { if (i == 1) e.tax(); else if (i == 2) e.tay(); else if (i == 3) e.taz(); } else { Symbol* sym = resolveSymbol(dest, scopePrefix); uint32_t addr = sym ? sym->value : parseNumericLiteral(dest); e.sta_abs(addr + i); } }
}

void AssemblerParser::emitDivCode(std::vector<uint8_t>& binary, int width, const std::string& dest, int tokenIndex, const std::string& scopePrefix) {
    int idx = tokenIndex; auto srcAst = parseExprAST(tokens, idx, symbolTable, scopePrefix); if (!srcAst) return;
    M65Emitter e(binary, getZPStart());
    int bytes = width / 8; if (bytes < 1) bytes = 1; if (bytes > 4) bytes = 4;

    bool isStackRel = false;
    int stackOff = 0;
    if (idx < (int)tokens.size() && tokens[idx].type == AssemblerTokenType::COMMA) {
        if (idx + 1 < (int)tokens.size() && (tokens[idx + 1].value == "s" || tokens[idx + 1].value == "S")) {
            isStackRel = true;
            stackOff = srcAst->getValue(this);
        }
    }

    auto storeMath = [&](uint8_t regAddr, int byteIdx, const std::string& src) {
        if (src == ".A") e.sta_abs(0xD700 + regAddr + byteIdx);
        else if (src == ".X") e.stx_abs(0xD700 + regAddr + byteIdx);
        else if (src == ".Y") e.sty_abs(0xD700 + regAddr + byteIdx);
        else if (src == ".Z") e.stz_abs(0xD700 + regAddr + byteIdx);
        else {
            Symbol* sym = resolveSymbol(src, scopePrefix);
            if (sym && sym->isStackRelative) {
                e.lda_stack((uint8_t)(sym->stackOffset + byteIdx));
                e.sta_abs(0xD700 + regAddr + byteIdx);
            } else {
                uint32_t addr = sym ? sym->value : parseNumericLiteral(src); 
                e.lda_abs(addr + byteIdx); e.sta_abs(0xD700 + regAddr + byteIdx); 
            }
        }
    };

    if (dest == ".A" || dest == ".AX" || dest == ".AXY" || dest == ".AXYZ" || dest == ".Q") { if (bytes >= 1) storeMath(0x60, 0, ".A"); if (bytes >= 2) storeMath(0x60, 1, ".X"); if (bytes >= 3) storeMath(0x60, 2, ".Y"); if (bytes >= 4) storeMath(0x60, 3, ".Z"); } else for (int i = 0; i < bytes; ++i) storeMath(0x60, i, dest);

    if (isStackRel) {
        for (int i = 0; i < bytes; ++i) { e.lda_stack((uint8_t)(stackOff + i)); e.sta_abs(0xD764 + i); }
    } else if (srcAst->isConstant(this)) { 
        uint32_t val = srcAst->getValue(this); 
        if (val == 0) {
            throw std::runtime_error("Division by zero at assembly time (constant divisor 0) at line " + std::to_string(tokens[tokenIndex].line));
        }
        for (int i = 0; i < bytes; ++i) { e.lda_imm((val >> (i * 8)) & 0xFF); e.sta_abs(0xD764 + i); }
    } else { 
        // Note: Runtime division by zero results in undefined behavior.

        int tIdx = tokenIndex; 
        std::string srcName = tokens[tIdx].value; 
        if (tokens[tIdx].type == AssemblerTokenType::REGISTER) srcName = "." + srcName; 
        if (srcName == ".A" || srcName == ".AX" || srcName == ".AXY" || srcName == ".AXYZ" || srcName == ".Q") { 
            if (bytes >= 1) storeMath(0x64, 0, ".A"); 
            if (bytes >= 2) storeMath(0x64, 1, ".X"); 
            if (bytes >= 3) storeMath(0x64, 2, ".Y"); 
            if (bytes >= 4) storeMath(0x64, 3, ".Z"); 
        } else {
            for (int i = 0; i < bytes; ++i) storeMath(0x64, i, srcName); 
        }
    }
    e.bit_abs(0xD70F); e.bne(-5);
    for (int i = 0; i < bytes; ++i) { e.lda_abs(0xD768 + i); if (dest == ".A" || dest == ".AX" || dest == ".AXY" || dest == ".AXYZ" || dest == ".Q") { if (i == 1) e.tax(); else if (i == 2) e.tay(); else if (i == 3) e.taz(); } else { Symbol* sym = resolveSymbol(dest, scopePrefix); uint32_t addr = sym ? sym->value : parseNumericLiteral(dest); e.sta_abs(addr + i); } }
}

std::vector<uint8_t> AssemblerParser::pass2() {
    std::vector<uint8_t> binary; ProcContext* currentPass2Proc = nullptr; bool isDeadCode = false;
    std::vector<ProcContext*> pass2ProcStack;
    M65Emitter e(binary, getZPStart());
    for (auto& [name, symbol] : symbolTable) if (symbol.isVariable) symbol.value = symbol.initialValue;
    for (auto& stmt : statements) {
        if (!stmt->label.empty()) { isDeadCode = false; }
        if (stmt->type == Statement::EXPR) { if (!isDeadCode) emitExpressionCode(binary, stmt->exprTarget, stmt->exprTokenIndex, stmt->scopePrefix); continue; }
        if (stmt->type == Statement::MUL) { if (!isDeadCode) emitMulCode(binary, stmt->mulWidth, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix); continue; }
        if (stmt->type == Statement::DIV) { if (!isDeadCode) emitDivCode(binary, stmt->mulWidth, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix); continue; }
        if (stmt->type == Statement::BASIC_UPSTART) {
            if (!isDeadCode) {
                uint32_t addr = 0; const auto& t = tokens[stmt->basicUpstartTokenIndex]; if (t.type == AssemblerTokenType::IDENTIFIER) { Symbol* sym = resolveSymbol(t.value, stmt->scopePrefix); if (sym) addr = sym->value; } else addr = parseNumericLiteral(t.value);
                std::string addrStr = std::to_string(addr); while (addrStr.length() < 4) addrStr = " " + addrStr; if (addrStr.length() > 4) addrStr = addrStr.substr(addrStr.length() - 4);
                uint16_t nextLine = (uint16_t)(stmt->address + 12 - 2); 
                binary.push_back(nextLine & 0xFF); binary.push_back(nextLine >> 8);
                binary.push_back(0x0A); binary.push_back(0x00); binary.push_back(0x9E); for (char c : addrStr) binary.push_back((uint8_t)c); binary.push_back(0x00); binary.push_back(0x00); binary.push_back(0x00);
            }
            continue;
        }
        if (stmt->type == Statement::INSTRUCTION) {
            if (stmt->instr.mnemonic == "PROC") {
                pass2ProcStack.push_back(currentPass2Proc);
                currentPass2Proc = stmt->procCtx;
                continue;
            }
            else if (stmt->instr.mnemonic == "ENDPROC") { 
                if (!isDeadCode) { 
                    if (stmt->instr.procParamSize == 0) e.pha(); /* Placeholder logic */ 
                    binary.push_back(0x60); 
                } 
                currentPass2Proc = pass2ProcStack.empty() ? nullptr : pass2ProcStack.back();
                if (!pass2ProcStack.empty()) pass2ProcStack.pop_back();
                isDeadCode = false; 
            }
            else if (stmt->instr.mnemonic == "CALL") {
                if (!isDeadCode) {
                    for (const auto& arg : stmt->instr.callArgs) {
                        bool isByte = (arg.size() >= 2 && arg.substr(0, 2) == "B#"); 
                        std::string v = isByte ? arg.substr(2) : ( (arg.size() >= 2 && arg.substr(0, 2) == "W#") ? arg.substr(2) : (arg[0] == '#' ? arg.substr(1) : arg));
                        uint32_t val; Symbol* sym = resolveSymbol(v, stmt->scopePrefix); if (sym) val = sym->value; else val = parseNumericLiteral(v);
                        if (!isByte && !arg.empty() && arg[0] != '#' && arg.size() >= 2 && arg.substr(0,2) != "W#" && sym) isByte = (sym->size == 1);
                        if (isByte) { e.lda_imm(val & 0xFF); e.pha(); } else { binary.push_back(0xF2); binary.push_back(val & 0xFF); binary.push_back(val >> 8); }
                    }
                    Symbol* sym = resolveSymbol(stmt->instr.operand, stmt->scopePrefix);
                    uint32_t target = sym ? sym->value : 0;
                    binary.push_back(0x20); binary.push_back(target & 0xFF); binary.push_back(target >> 8);
                }
            } else {
                if (!isDeadCode) {
                    bool isQuad = (stmt->instr.mnemonic.size() > 1 && stmt->instr.mnemonic.back() == 'Q' && stmt->instr.mnemonic != "LDQ" && stmt->instr.mnemonic != "STQ" && stmt->instr.mnemonic != "BEQ" && stmt->instr.mnemonic != "BNE" && stmt->instr.mnemonic != "BRA" && stmt->instr.mnemonic != "BSR");
                    if (isQuad) { binary.push_back(0x42); binary.push_back(0x42); }
                    if (stmt->instr.mode == AddressingMode::FLAT_INDIRECT_Z) e.eom();
                    bool isBranch = (stmt->instr.mnemonic == "BEQ" || stmt->instr.mnemonic == "BNE" || stmt->instr.mnemonic == "BRA" || stmt->instr.mnemonic == "BCC" || stmt->instr.mnemonic == "BCS" || stmt->instr.mnemonic == "BPL" || stmt->instr.mnemonic == "BMI" || stmt->instr.mnemonic == "BVC" || stmt->instr.mnemonic == "BVS" || stmt->instr.mnemonic == "BSR");
                    if (!isBranch) { 
                        uint8_t op = getOpcode(stmt->instr.mnemonic, stmt->instr.mode); 
                        if (op == 0 && stmt->instr.mnemonic != "BRK") {
                            std::string errorMsg = "Invalid instruction or addressing mode: " + stmt->instr.mnemonic + " " + AddressingModeToString(stmt->instr.mode);
                            auto validModes = getValidAddressingModes(stmt->instr.mnemonic);
                            if (validModes.empty()) {
                                errorMsg = "Unknown mnemonic: " + stmt->instr.mnemonic;
                            } else {
                                errorMsg += ". Valid addressing modes for " + stmt->instr.mnemonic + " are: ";
                                for (size_t i = 0; i < validModes.size(); ++i) {
                                    errorMsg += AddressingModeToString(validModes[i]);
                                    if (i < validModes.size() - 1) errorMsg += ", ";
                                }
                            }
                            throw std::runtime_error(errorMsg + " at line " + std::to_string(stmt->line));
                        }
                        binary.push_back(op); 
                    }
                    if (stmt->instr.mode == AddressingMode::IMMEDIATE || stmt->instr.mode == AddressingMode::STACK_RELATIVE || stmt->instr.mode == AddressingMode::BASE_PAGE_INDIRECT_Z || stmt->instr.mode == AddressingMode::FLAT_INDIRECT_Z || stmt->instr.mode == AddressingMode::BASE_PAGE_INDIRECT_SP_Y || stmt->instr.mode == AddressingMode::BASE_PAGE_X_INDIRECT || stmt->instr.mode == AddressingMode::BASE_PAGE_INDIRECT_Y || stmt->instr.mode == AddressingMode::BASE_PAGE || stmt->instr.mode == AddressingMode::BASE_PAGE_X || stmt->instr.mode == AddressingMode::BASE_PAGE_Y) {
                        std::string op = stmt->instr.operand;
                        bool lowByte = false, highByte = false;
                        if (!op.empty() && op[0] == '<') { lowByte = true; op = op.substr(1); }
                        else if (!op.empty() && op[0] == '>') { highByte = true; op = op.substr(1); }
                        
                        uint32_t v;
                        if (stmt->instr.operandTokenIndex != -1) {
                            int tIdx = stmt->instr.operandTokenIndex;
                            if (lowByte || highByte) tIdx++; 
                            v = evaluateExpressionAt(tIdx, stmt->scopePrefix);
                        } else {
                            Symbol* sym = resolveSymbol(op, stmt->scopePrefix);
                            v = sym ? sym->value : parseNumericLiteral(op);
                        }
                        
                        if (lowByte) v = v & 0xFF;
                        else if (highByte) v = (v >> 8) & 0xFF;
                        
                        binary.push_back((uint8_t)v);
                    } else if (stmt->instr.mode == AddressingMode::ABSOLUTE || stmt->instr.mode == AddressingMode::ABSOLUTE_X || stmt->instr.mode == AddressingMode::ABSOLUTE_Y || stmt->instr.mode == AddressingMode::ABSOLUTE_INDIRECT || stmt->instr.mode == AddressingMode::ABSOLUTE_X_INDIRECT || stmt->instr.mode == AddressingMode::IMMEDIATE16) {
                        std::string op = stmt->instr.operand;
                        bool lowByte = false, highByte = false;
                        if (!op.empty() && op[0] == '<') { lowByte = true; op = op.substr(1); }
                        else if (!op.empty() && op[0] == '>') { highByte = true; op = op.substr(1); }
                        
                        uint32_t a;
                        if (stmt->instr.operandTokenIndex != -1) {
                            int tIdx = stmt->instr.operandTokenIndex;
                            if (lowByte || highByte) tIdx++;
                            a = evaluateExpressionAt(tIdx, stmt->scopePrefix);
                        } else {
                            Symbol* sym = resolveSymbol(op, stmt->scopePrefix);
                            a = sym ? sym->value : parseNumericLiteral(op);
                        }
                        
                        if (lowByte) a = a & 0xFF;
                        else if (highByte) a = (a >> 8) & 0xFF;
                        
                        binary.push_back(a & 0xFF); binary.push_back(a >> 8);
                    } else if (stmt->instr.mnemonic == "BEQ" || stmt->instr.mnemonic == "BNE" || stmt->instr.mnemonic == "BRA" || stmt->instr.mnemonic == "BCC" || stmt->instr.mnemonic == "BCS" || stmt->instr.mnemonic == "BPL" || stmt->instr.mnemonic == "BMI" || stmt->instr.mnemonic == "BVC" || stmt->instr.mnemonic == "BVS") {
                        Symbol* sym = resolveSymbol(stmt->instr.operand, stmt->scopePrefix);
                        uint32_t t = sym ? sym->value : 0; int32_t off2 = (int32_t)t - (int32_t)(stmt->address + 2); int32_t off3 = (int32_t)t - (int32_t)(stmt->address + 3);
                        if (stmt->size == 2) { binary.push_back(getOpcode(stmt->instr.mnemonic, AddressingMode::RELATIVE)); binary.push_back((uint8_t)(int8_t)off2); }
                        else { binary.push_back(getOpcode(stmt->instr.mnemonic, AddressingMode::RELATIVE16)); binary.push_back(off3 & 0xFF); binary.push_back(off3 >> 8); }
                    } else if (stmt->instr.mnemonic == "BSR") {
                        Symbol* sym = resolveSymbol(stmt->instr.operand, stmt->scopePrefix);
                        uint32_t t = sym ? sym->value : 0; int32_t off = (int32_t)t - (int32_t)(stmt->address + 3);
                        binary.push_back(0x63); binary.push_back(off & 0xFF); binary.push_back(off >> 8);
                    } else if (stmt->instr.mode == AddressingMode::BASE_PAGE_RELATIVE) {
                        Symbol* sym = resolveSymbol(stmt->instr.operand, stmt->scopePrefix);
                        uint32_t v = sym ? sym->value : parseNumericLiteral(stmt->instr.operand); binary.push_back((uint8_t)v);
                        Symbol* tsym = resolveSymbol(stmt->instr.bitBranchTarget, stmt->scopePrefix);
                        uint32_t t = tsym ? tsym->value : 0; int32_t off = (int32_t)t - (int32_t)(stmt->address + 3); binary.push_back((uint8_t)off);
                    } else if (stmt->instr.mnemonic == "RTN") {
                        Symbol* sym = resolveSymbol(stmt->instr.operand, stmt->scopePrefix);
                        uint32_t v = sym ? sym->value : parseNumericLiteral(stmt->instr.operand);
                        if (v == 0) binary.push_back(0x60); else { binary.push_back(0x62); binary.push_back((uint8_t)v); }
                    }
                }
                if (stmt->instr.mnemonic == "RTS" || stmt->instr.mnemonic == "RTN" || stmt->instr.mnemonic == "RTI") isDeadCode = true;
            }
        } else if (stmt->type == Statement::DIRECTIVE) {
            if (!isDeadCode || stmt->dir.name == "org") {
                if (stmt->dir.name == "var") {
                    if (stmt->dir.varType == Directive::ASSIGN) symbolTable[stmt->dir.varName].value = evaluateExpressionAt(stmt->dir.tokenIndex, stmt->scopePrefix);
                    else if (stmt->dir.varType == Directive::INC) symbolTable[stmt->dir.varName].value++;
                    else if (stmt->dir.varType == Directive::DEC) symbolTable[stmt->dir.varName].value--;
                } else if (stmt->dir.name == "cleanup") { if (currentPass2Proc) currentPass2Proc->totalParamSize += evaluateExpressionAt(stmt->dir.tokenIndex, stmt->scopePrefix); }
                else if (stmt->dir.name == "byte") for (const auto& a : stmt->dir.arguments) binary.push_back((uint8_t)parseNumericLiteral(a));
                else if (stmt->dir.name == "word") for (const auto& a : stmt->dir.arguments) { uint32_t v = parseNumericLiteral(a); binary.push_back(v & 0xFF); binary.push_back(v >> 8); }
                else if (stmt->dir.name == "dword" || stmt->dir.name == "long") for (const auto& a : stmt->dir.arguments) { uint32_t v = parseNumericLiteral(a); binary.push_back(v & 0xFF); binary.push_back(v >> 8); binary.push_back(v >> 16); binary.push_back(v >> 24); }
                else if (stmt->dir.name == "float") { for (const auto& a : stmt->dir.arguments) { double v = std::stod(a); std::vector<uint8_t> enc = encodeFloat(v); for (uint8_t eb : enc) binary.push_back(eb); } }
                else if (stmt->dir.name == "text") { for (char c : stmt->dir.arguments[0]) binary.push_back(toPetscii(c)); }
                else if (stmt->dir.name == "ascii") { for (char c : stmt->dir.arguments[0]) binary.push_back((uint8_t)c); }
            }
        }
    }
    return binary;
}
