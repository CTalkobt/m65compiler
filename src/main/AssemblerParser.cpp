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

    if (val < 0) {
 negative = true;
 val = -val;
 
}

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


}
;


struct ConstantNode : public ExprAST {

    uint32_t value;

    ConstantNode(uint32_t v) : value(v) {

}

    uint32_t getValue(AssemblerParser*) const override {
 return value;
 
}

    bool isConstant(AssemblerParser*) const override {
 return true;
 
}

    bool is16Bit(AssemblerParser*) const override {
 return value > 0xFF;
 
}

    void emit(std::vector<uint8_t>& binary, AssemblerParser* parser, int width, const std::string&) override {

        if (!parser) return;

        M65Emitter e(binary, parser->getZPStart());

        e.lda_imm(value & 0xFF);

        if (width >= 16) e.ldx_imm((value >> 8) & 0xFF);

    
}


}
;


struct RegisterNode : public ExprAST {

    std::string name;

    RegisterNode(const std::string& n) : name(n) {

}

    uint32_t getValue(AssemblerParser*) const override {
 return 0;
 
}

    bool isConstant(AssemblerParser*) const override {
 return false;
 
}

    bool is16Bit(AssemblerParser*) const override {
 return name.size() > 2;
 
}

    void emit(std::vector<uint8_t>& binary, AssemblerParser* parser, int width, const std::string&) override {

        if (!parser) return;

        M65Emitter e(binary, parser->getZPStart());

        if (width <= 8) {

            if (name == ".X") e.txa();

            else if (name == ".Y") e.tya();

            else if (name == ".Z") e.tza();

            else if (name == ".SP") {
 e.tsx();
 e.txa();
 
}

        
}
 else {

            if (name == ".AY") {
 e.phy();
 e.plx();
 
}

            else if (name == ".AZ") {
 e.phz();
 e.plx();
 
}

            else if (name == ".XY") {
 e.txa();
 e.phy();
 e.plx();
 
}

            else if (name == ".YZ") {
 e.tya();
 e.phz();
 e.plx();
 
}

            else if (name == ".A") {
 e.ldx_imm(0);
 
}

            else if (name == ".X") {
 e.txa();
 e.ldx_imm(0);
 
}

            else if (name == ".Y") {
 e.tya();
 e.ldx_imm(0);
 
}

            else if (name == ".Z") {
 e.tza();
 e.ldx_imm(0);
 
}

        
}

    
}


}
;


struct FlagNode : public ExprAST {

    char flag;

    FlagNode(char f) : flag(f) {

}

    uint32_t getValue(AssemblerParser*) const override {
 return 0;
 
}

    bool isConstant(AssemblerParser*) const override {
 return false;
 
}

    bool is16Bit(AssemblerParser*) const override {
 return false;
 
}

    void emit(std::vector<uint8_t>& binary, AssemblerParser* parser, int width, const std::string&) override {

        if (!parser) return;

        M65Emitter e(binary, parser->getZPStart());

        if (flag == 'C') {
 e.lda_imm(0);
 e.adc_imm(0);
 
}

        else {

            int8_t branchOp = 0;
 switch (flag) {
 case 'Z': branchOp = 0xD0;
 break;
 case 'V': branchOp = 0x50;
 break;
 case 'N': branchOp = 0x10;
 break;
 
}

            if (branchOp != 0) {
 e.lda_imm(0);
 e.bne(0x02);
 e.lda_imm(1);
 
}

            else {

                uint8_t mask = 0;
 if (flag == 'I') mask = 0x04;
 else if (flag == 'D') mask = 0x08;
 else if (flag == 'B') mask = 0x10;

                if (mask != 0) {
 e.pha();
 e.pla();
 e.and_imm(mask);
 e.beq(0x02);
 e.lda_imm(1);
 
}
 else e.lda_imm(0);

            
}

        
}

        if (width >= 16) e.ldx_imm(0);

    
}


}
;


struct VariableNode : public ExprAST {

    std::string name;
 std::string scopePrefix;

    VariableNode(const std::string& n, const std::string& scope = "") : name(n), scopePrefix(scope) {

}

    uint32_t getValue(AssemblerParser* parser) const override {
 Symbol* sym = parser->resolveSymbol(name, scopePrefix);
 return sym ? sym->value : 0;
 
}

    bool isConstant(AssemblerParser* parser) const override {
 Symbol* sym = parser->resolveSymbol(name, scopePrefix);
 return sym ? !sym->isAddress : false;
 
}

    bool is16Bit(AssemblerParser* parser) const override {
 Symbol* sym = parser->resolveSymbol(name, scopePrefix);
 return sym ? sym->size > 1 : true;
 
}

    void emit(std::vector<uint8_t>& binary, AssemblerParser* parser, int width, const std::string&) override {

        if (!parser) return;
 Symbol* sym = parser->resolveSymbol(name, scopePrefix);
 if (!sym) return;

        M65Emitter e(binary, parser->getZPStart());

        if (!sym->isAddress) {
 e.lda_imm(sym->value & 0xFF);
 if (width >= 16) e.ldx_imm((sym->value >> 8) & 0xFF);
 
}

        else {
 if (sym->isStackRelative) {
 e.lda_stack((uint8_t)sym->stackOffset);
 if (width >= 16) {
 e.lda_stack((uint8_t)(sym->stackOffset + 1));
 e.tax();
 e.lda_stack((uint8_t)sym->stackOffset);
 
}
 
}

               else {
 e.lda_abs(sym->value);
 if (width >= 16) e.ldx_abs(sym->value + 1);
 
}
 
}

    
}


}
;


struct UnaryExpr : public ExprAST {

    std::string op;
 std::unique_ptr<ExprAST> operand;

    UnaryExpr(std::string o, std::unique_ptr<ExprAST> opnd) : op(o), operand(std::move(opnd)) {

}

    uint32_t getValue(AssemblerParser* parser) const override {

        uint32_t val = operand ? operand->getValue(parser) : 0;

        if (op == "!") return val == 0 ? 1 : 0;
 if (op == "~") return ~val;
 if (op == "-") return -val;
 if (op == "<") return val & 0xFF;
 if (op == ">") return (val >> 8) & 0xFF;
 return 0;

    
}

    bool isConstant(AssemblerParser* parser) const override {
 return operand ? operand->isConstant(parser) : true;
 
}

    bool is16Bit(AssemblerParser* parser) const override {
 return operand ? operand->is16Bit(parser) : false;
 
}

    void emit(std::vector<uint8_t>& binary, AssemblerParser* parser, int width, const std::string& target) override {

        if (!parser || !operand) return;
 operand->emit(binary, parser, width, target);
 M65Emitter e(binary, parser->getZPStart());

        if (op == "!") {
 e.bne(0x05);
 if (width >= 16) {
 e.txa();
 e.bne(0x02);
 
}
 e.lda_imm(1);
 e.bra(0x02);
 e.lda_imm(0);
 if (width >= 16) {
 e.tax();
 e.ldx_imm(0);
 
}
 
}

        else if (op == "~") {
 e.eor_imm(0xFF);
 if (width >= 16) {
 e.pha();
 e.txa();
 e.eor_imm(0xFF);
 e.tax();
 e.pla();
 
}
 
}

        else if (op == "-") {
 e.neg_a();
 if (width >= 16) {
 e.pha();
 e.txa();
 e.eor_imm(0xFF);
 e.adc_imm(0);
 e.tax();
 e.pla();
 
}
 
}

        else if (op == "<") {
 if (width >= 16) e.ldx_imm(0);
 
}
 else if (op == ">") {
 if (width >= 16) {
 e.txa();
 e.ldx_imm(0);
 
}
 else {
 e.txa();
 
}
 
}

    
}


}
;


struct DereferenceNode : public ExprAST {

    std::unique_ptr<ExprAST> address;
 bool isFlat;

    DereferenceNode(std::unique_ptr<ExprAST> addr, bool flat = false) : address(std::move(addr)), isFlat(flat) {

}

    uint32_t getValue(AssemblerParser*) const override {
 return 0;
 
}

    bool isConstant(AssemblerParser*) const override {
 return false;
 
}

    bool is16Bit(AssemblerParser*) const override {
 return true;
 
}

    void emit(std::vector<uint8_t>& binary, AssemblerParser* parser, int width, const std::string&) override {

        if (!parser || !address) return;
 M65Emitter e(binary, parser->getZPStart());
 address->emit(binary, parser, 16, ".AX");

        e.sta_s(0);
 e.stx_s(1);

        e.lda_ind_zs(0, isFlat);
 if (width >= 16) {
 e.inc_s(0);
 e.lda_ind_zs(0, isFlat);
 e.tax();
 e.dec_s(0);
 
}

    
}


}
;


struct BinaryExpr : public ExprAST {

    std::string op;
 std::unique_ptr<ExprAST> left, right;

    BinaryExpr(std::string o, std::unique_ptr<ExprAST> l, std::unique_ptr<ExprAST> r) : op(o), left(std::move(l)), right(std::move(r)) {

}

    uint32_t getValue(AssemblerParser* parser) const override {

        uint32_t l = left ? left->getValue(parser) : 0;
 uint32_t r = right ? right->getValue(parser) : 0;

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

    bool isConstant(AssemblerParser* parser) const override {
 return (left ? left->isConstant(parser) : true) && (right ? right->isConstant(parser) : true);
 
}

    bool is16Bit(AssemblerParser* parser) const override {
 return getValue(parser) > 0xFF || (left && left->is16Bit(parser)) || (right && right->is16Bit(parser));
 
}

    void emit(std::vector<uint8_t>& binary, AssemblerParser* parser, int width, const std::string& target) override {

        if (!parser || !left || !right) return;
 M65Emitter e(binary, parser->getZPStart());
 int bytes = width / 8;
 if (bytes < 1) bytes = 1;
 if (bytes > 4) bytes = 4;

        if (op == "*" || op == "/" || op == "+" || op == "-") {

            left->emit(binary, parser, width, ".A");
 uint8_t base = (op == "/") ? 0x60 : 0x70;
 e.sta_abs(0xD700 + base);
 if (bytes >= 2) e.stx_abs(0xD701 + base);

            right->emit(binary, parser, width, ".A");
 e.sta_abs(0xD700 + base + 4);
 if (bytes >= 2) e.stx_abs(0xD701 + base + 4);

            if (op == "*") {
 for (int i = 0;
 i < bytes;
 ++i) {
 e.lda_abs(0xD778 + i);
 if (i == 1) e.tax();
 
}
 
}

            else if (op == "/") {
 e.bit_abs(0xD70F);
 e.bne(-5);
 for (int i = 0;
 i < bytes;
 ++i) {
 e.lda_abs(0xD768 + i);
 if (i == 1) e.tax();
 
}
 
}

            else if (op == "+") {
 for (int i = 0;
 i < bytes;
 ++i) {
 e.lda_abs(0xD77C + i);
 if (i == 1) e.tax();
 
}
 
}

            else if (op == "-") {
 e.sec();
 for (int i = 0;
 i < bytes;
 ++i) {
 e.lda_abs(0xD770 + i);
 e.sbc_abs(0xD774 + i);
 e.sta_abs(0xD770 + i);
 
}
 e.lda_abs(0xD770);
 if (bytes >= 2) e.ldx_abs(0xD771);
 
}

        
}
 else if (op == "&" || op == "|" || op == "^") {

            left->emit(binary, parser, width, ".A");
 e.push_ax();
 right->emit(binary, parser, width, ".A");
 e.sta_s(0);
 if (width >= 16) e.stx_s(1);
 e.pop_ax();
 if (op == "&") e.and_s(0);
 else if (op == "|") e.ora_s(0);
 else e.eor_s(0);

            if (width >= 16) {
 e.pha();
 e.txa();
 if (op == "&") e.and_s(1);
 else if (op == "|") e.ora_s(1);
 else e.eor_s(1);
 e.tax();
 e.pla();
 
}

        
}
 else if (op == "<<" || op == ">>") {

            left->emit(binary, parser, width, ".A");
 e.push_ax();
 right->emit(binary, parser, width, ".A");
 e.sta_s(0);
 e.pop_ax();
 e.lda_s(0);
 e.beq((width >= 16) ? 0x07 : 0x05);
 e.dec_s(0);
 if (op == "<<") {
 e.asl_a();
 if (width >= 16) {
 e.rol_a();
 e.txa();
 e.rol_a();
 e.tax();
 
}
 
}
 else {
 if (width >= 16) {
 e.txa();
 e.lsr_a();
 e.tax();
 e.ror_a();
 
}
 else e.lsr_a();
 
}
 e.bra((width >= 16) ? -11 : -9);

        
}

    
}


}
;


// --- AssemblerParser Implementation ---

AssemblerParser::AssemblerParser(const std::vector<AssemblerToken>& tokens) : tokens(tokens), pos(0), pc(0) {

}

AssemblerParser::AssemblerParser(const std::vector<AssemblerToken>& tokens, const std::map<std::string, uint32_t>& predefinedSymbols) : tokens(tokens), pos(0), pc(0), nextScopeId(0) {
    for (const auto& [name, val] : predefinedSymbols) symbolTable[name] = {val, false, 2, false, 0, false, 0};
}

const AssemblerToken& AssemblerParser::peek() const {
 if (pos >= tokens.size()) return tokens.back();
 return tokens[pos];
 
}

const AssemblerToken& AssemblerParser::advance() {
 if (pos < tokens.size()) pos++;
 return tokens[pos - 1];
 
}

bool AssemblerParser::match(AssemblerTokenType type) {
 if (peek().type == type) {
 advance();
 return true;
 
}
 return false;
 
}


static uint32_t parseNumericLiteral(const std::string& literal) {

    if (literal.empty()) throw std::runtime_error("Empty numeric literal");

    try {
 if (literal[0] == '$') return std::stoul(literal.substr(1), nullptr, 16);
 if (literal[0] == '%') return std::stoul(literal.substr(1), nullptr, 2);
 return std::stoul(literal);
 
}

    catch (...) {
 throw std::runtime_error("Invalid numeric literal: " + literal);
 
}


}


std::unique_ptr<ExprAST> parseExprAST(const std::vector<AssemblerToken>& tokens, int& idx, std::map<std::string, Symbol>& symbolTable, const std::string& scopePrefix = "") {

    auto parsePrimary = [&]() -> std::unique_ptr<ExprAST> {

        if (idx >= (int)tokens.size()) return nullptr;
 const auto& t = tokens[idx++];

        if (t.type == AssemblerTokenType::DECIMAL_LITERAL || t.type == AssemblerTokenType::HEX_LITERAL) {
 if (idx < (int)tokens.size() && tokens[idx].type == AssemblerTokenType::COMMA) {
 if (idx + 1 < (int)tokens.size() && (tokens[idx + 1].value == "s" || tokens[idx + 1].value == "S")) {
 uint32_t val = (t.type == AssemblerTokenType::HEX_LITERAL) ? std::stoul(t.value.substr(1), nullptr, 16) : std::stoul(t.value);
 idx += 2;
                 std::string tempName = "__stack_" + std::to_string(val);
                 symbolTable[tempName] = {val, true, 2, false, 0, true, (int)val};
                 return std::make_unique<VariableNode>(tempName, "");
 
}
 
}
 
}

        if (t.type == AssemblerTokenType::DECIMAL_LITERAL) return std::make_unique<ConstantNode>(std::stoul(t.value));
 if (t.type == AssemblerTokenType::HEX_LITERAL) return std::make_unique<ConstantNode>(std::stoul(t.value.substr(1), nullptr, 16));
 if (t.type == AssemblerTokenType::BINARY_LITERAL) return std::make_unique<ConstantNode>(std::stoul(t.value.substr(1), nullptr, 2));
 if (t.type == AssemblerTokenType::REGISTER) return std::make_unique<RegisterNode>(t.value);
 if (t.type == AssemblerTokenType::FLAG) return std::make_unique<FlagNode>(t.value[0]);
 if (t.type == AssemblerTokenType::IDENTIFIER) return std::make_unique<VariableNode>(t.value, scopePrefix);
 if (t.type == AssemblerTokenType::STAR) return std::make_unique<DereferenceNode>(parseExprAST(tokens, idx, symbolTable, scopePrefix));
 if (t.type == AssemblerTokenType::OPEN_BRACKET) {
 auto addr = parseExprAST(tokens, idx, symbolTable, scopePrefix);
 if (idx < (int)tokens.size() && tokens[idx].type == AssemblerTokenType::CLOSE_BRACKET) idx++;
 return std::make_unique<DereferenceNode>(std::move(addr), true);
 
}
 if (t.type == AssemblerTokenType::BANG || t.type == AssemblerTokenType::TILDE || t.type == AssemblerTokenType::LESS_THAN || t.type == AssemblerTokenType::GREATER_THAN || (t.type == AssemblerTokenType::MINUS && idx < (int)tokens.size() && tokens[idx].type != AssemblerTokenType::DECIMAL_LITERAL)) {
 std::string op = t.value;
 return std::make_unique<UnaryExpr>(op, parseExprAST(tokens, idx, symbolTable, scopePrefix));
 
}
 if (t.type == AssemblerTokenType::OPEN_PAREN) {
 auto expr = parseExprAST(tokens, idx, symbolTable, scopePrefix);
 if (idx < (int)tokens.size() && tokens[idx].type == AssemblerTokenType::CLOSE_PAREN) idx++;
 return expr;
 
}
 return nullptr;

    
}
;

    auto parseMultiplicative = [&]() -> std::unique_ptr<ExprAST> {
 auto left = parsePrimary();
 while (idx < (int)tokens.size() && (tokens[idx].type == AssemblerTokenType::STAR || tokens[idx].type == AssemblerTokenType::SLASH)) {
 std::string op = tokens[idx++].value;
 auto right = parsePrimary();
 left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
 
}
 return left;
 
}
;

    auto parseAdditive = [&]() -> std::unique_ptr<ExprAST> {
 auto left = parseMultiplicative();
 while (idx < (int)tokens.size() && (tokens[idx].type == AssemblerTokenType::PLUS || tokens[idx].type == AssemblerTokenType::MINUS)) {
 std::string op = tokens[idx++].value;
 auto right = parseMultiplicative();
 left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
 
}
 return left;
 
}
;

    auto parseShift = [&]() -> std::unique_ptr<ExprAST> {
 auto left = parseAdditive();
 while (idx < (int)tokens.size() && (tokens[idx].type == AssemblerTokenType::LSHIFT || tokens[idx].type == AssemblerTokenType::RSHIFT)) {
 std::string op = tokens[idx++].value;
 auto right = parseAdditive();
 left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
 
}
 return left;
 
}
;

    return parseShift();
 

}


uint32_t AssemblerParser::evaluateExpressionAt(int index, const std::string& scopePrefix) {

    if (index < 0 || index >= (int)tokens.size()) return 0;

    int idx = index;
 auto ast = parseExprAST(tokens, idx, symbolTable, scopePrefix);
 if (!ast) throw std::runtime_error("Expected expression at line " + std::to_string(tokens[index].line));
 return ast->getValue(this);


}


Symbol* AssemblerParser::resolveSymbol(const std::string& name, const std::string& scopePrefix) {

    std::string current = scopePrefix;

    while (true) {

        std::string fullName = current + name;
 if (symbolTable.count(fullName)) return &symbolTable.at(fullName);
 if (current.empty()) break;

        if (current.size() < 2) {
 current = "";
 continue;
 
}
 size_t p = current.find_last_of(':', current.size() - 2);
 if (p == std::string::npos) current = "";
 else current = current.substr(0, p + 1);

    
}

    return nullptr;


}


uint32_t AssemblerParser::getZPStart() const {
 if (symbolTable.count("cc45.zeroPageStart")) return symbolTable.at("cc45.zeroPageStart").value; return 0x02;
 
}

const AssemblerToken& AssemblerParser::expect(AssemblerTokenType type, const std::string& message) {
    if (peek().type == type) return advance();
    throw std::runtime_error(message + " at " + std::to_string(peek().line) + ":" + std::to_string(peek().column));
}

bool AssemblerParser::isStackRelativeOperand(int tokenIndex, uint32_t& offset, const std::string& scopePrefix) {
    if (tokenIndex < 0 || tokenIndex >= (int)tokens.size()) return false;
    int idx = tokenIndex;
    try {
        auto ast = parseExprAST(tokens, idx, symbolTable, scopePrefix);
        if (ast && idx + 1 < (int)tokens.size() && 
            tokens[idx].type == AssemblerTokenType::COMMA &&
            (tokens[idx+1].value == "s" || tokens[idx+1].value == "S")) {
            offset = ast->getValue(this);
            return true;
        }
    } catch (...) {}
    return false;
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
        case AddressingMode::STACK_RELATIVE: return "offset,s";
        case AddressingMode::STACK_RELATIVE_INDIRECT_Y: return "(offset,s),y";
        case AddressingMode::LINEAR_ABSOLUTE: return "lin_abs";
        case AddressingMode::LINEAR_ABSOLUTE_X: return "lin_abs,X";
        case AddressingMode::LINEAR_ABSOLUTE_Y: return "lin_abs,Y";
        case AddressingMode::FLAT_INDIRECT_Z: return "[zp],Z";
        case AddressingMode::QUAD_Q: return "Q";
        case AddressingMode::NONE: return "None";
        default: return "Unknown";
    }
}


static const std::map<std::pair<std::string, AddressingMode>, uint8_t>& getOpcodeMapInternal() {

    static const std::map<std::pair<std::string, AddressingMode>, uint8_t> opcodes = {

        {
 {
"ADC", AddressingMode::BASE_PAGE_INDIRECT_Y
}
, 0x71 
}
, {
 {
"ADC", AddressingMode::BASE_PAGE_INDIRECT_Z
}
, 0x72 
}
, {
 {
"ADC", AddressingMode::FLAT_INDIRECT_Z
}
, 0x72 
}
, {
 {
"ADC", AddressingMode::STACK_RELATIVE
}
, 0x72 
}
, {
 {
"ADC", AddressingMode::BASE_PAGE_X_INDIRECT
}
, 0x61 
}
, {
 {
"ADC", AddressingMode::ABSOLUTE
}
, 0x6D 
}
, {
 {
"ADC", AddressingMode::ABSOLUTE_X
}
, 0x7D 
}
, {
 {
"ADC", AddressingMode::ABSOLUTE_Y
}
, 0x79 
}
, {
 {
"ADC", AddressingMode::BASE_PAGE
}
, 0x65 
}
, {
 {
"ADC", AddressingMode::BASE_PAGE_X
}
, 0x75 
}
, {
 {
"ADC", AddressingMode::IMMEDIATE
}
, 0x69 
}
,
        {
 {
"AND", AddressingMode::BASE_PAGE_INDIRECT_Y
}
, 0x31 
}
, {
 {
"AND", AddressingMode::BASE_PAGE_INDIRECT_Z
}
, 0x32 
}
, {
 {
"AND", AddressingMode::FLAT_INDIRECT_Z
}
, 0x32 
}
, {
 {
"AND", AddressingMode::STACK_RELATIVE
}
, 0x32 
}
, {
 {
"AND", AddressingMode::BASE_PAGE_X_INDIRECT
}
, 0x21 
}
, {
 {
"AND", AddressingMode::ABSOLUTE
}
, 0x2D 
}
, {
 {
"AND", AddressingMode::ABSOLUTE_X
}
, 0x3D 
}
, {
 {
"AND", AddressingMode::ABSOLUTE_Y
}
, 0x39 
}
, {
 {
"AND", AddressingMode::BASE_PAGE
}
, 0x25 
}
, {
 {
"AND", AddressingMode::BASE_PAGE_X
}
, 0x35 
}
, {
 {
"AND", AddressingMode::IMMEDIATE
}
, 0x29 
}
,
        {
 {
"ASL", AddressingMode::ABSOLUTE
}
, 0x0E 
}
, {
 {
"ASL", AddressingMode::ABSOLUTE_X
}
, 0x1E 
}
, {
 {
"ASL", AddressingMode::ACCUMULATOR
}
, 0x0A 
}
, {
 {
"ASL", AddressingMode::BASE_PAGE
}
, 0x06 
}
, {
 {
"ASL", AddressingMode::BASE_PAGE_X
}
, 0x16 
}
,
        {
 {
"ASR", AddressingMode::ACCUMULATOR
}
, 0x43 
}
, {
 {
"ASR", AddressingMode::BASE_PAGE
}
, 0x44 
}
, {
 {
"ASR", AddressingMode::BASE_PAGE_X
}
, 0x54 
}
, {
 {
"ASW", AddressingMode::ABSOLUTE
}
, 0xCB 
}
,
        {
 {
"BBR0", AddressingMode::BASE_PAGE_RELATIVE
}
, 0x0F 
}
, {
 {
"BBR1", AddressingMode::BASE_PAGE_RELATIVE
}
, 0x1F 
}
, {
 {
"BBR2", AddressingMode::BASE_PAGE_RELATIVE
}
, 0x2F 
}
, {
 {
"BBR3", AddressingMode::BASE_PAGE_RELATIVE
}
, 0x3F 
}
, {
 {
"BBR4", AddressingMode::BASE_PAGE_RELATIVE
}
, 0x4F 
}
, {
 {
"BBR5", AddressingMode::BASE_PAGE_RELATIVE
}
, 0x5F 
}
, {
 {
"BBR6", AddressingMode::BASE_PAGE_RELATIVE
}
, 0x6F 
}
, {
 {
"BBR7", AddressingMode::BASE_PAGE_RELATIVE
}
, 0x7F 
}
,
        {
 {
"BBS0", AddressingMode::BASE_PAGE_RELATIVE
}
, 0x8F 
}
, {
 {
"BBS1", AddressingMode::BASE_PAGE_RELATIVE
}
, 0x9F 
}
, {
 {
"BBS2", AddressingMode::BASE_PAGE_RELATIVE
}
, 0xAF 
}
, {
 {
"BBS3", AddressingMode::BASE_PAGE_RELATIVE
}
, 0xBF 
}
, {
 {
"BBS4", AddressingMode::BASE_PAGE_RELATIVE
}
, 0xCF 
}
, {
 {
"BBS5", AddressingMode::BASE_PAGE_RELATIVE
}
, 0xDF 
}
, {
 {
"BBS6", AddressingMode::BASE_PAGE_RELATIVE
}
, 0xEF 
}
, {
 {
"BBS7", AddressingMode::BASE_PAGE_RELATIVE
}
, 0xFF 
}
,
        {
 {
"BCC", AddressingMode::RELATIVE
}
, 0x90 
}
, {
 {
"BCC", AddressingMode::RELATIVE16
}
, 0x93 
}
, {
 {
"BCS", AddressingMode::RELATIVE
}
, 0xB0 
}
, {
 {
"BCS", AddressingMode::RELATIVE16
}
, 0xB3 
}
, {
 {
"BEQ", AddressingMode::RELATIVE
}
, 0xF0 
}
, {
 {
"BEQ", AddressingMode::RELATIVE16
}
, 0xF3 
}
,
        {
 {
"BIT", AddressingMode::ABSOLUTE
}
, 0x2C 
}
, {
 {
"BIT", AddressingMode::ABSOLUTE_X
}
, 0x3C 
}
, {
 {
"BIT", AddressingMode::BASE_PAGE
}
, 0x24 
}
, {
 {
"BIT", AddressingMode::BASE_PAGE_X
}
, 0x34 
}
, {
 {
"BIT", AddressingMode::IMMEDIATE
}
, 0x89 
}
,
        {
 {
"BMI", AddressingMode::RELATIVE
}
, 0x30 
}
, {
 {
"BMI", AddressingMode::RELATIVE16
}
, 0x33 
}
, {
 {
"BNE", AddressingMode::RELATIVE
}
, 0xD0 
}
, {
 {
"BNE", AddressingMode::RELATIVE16
}
, 0xD3 
}
, {
 {
"BPL", AddressingMode::RELATIVE
}
, 0x10 
}
, {
 {
"BPL", AddressingMode::RELATIVE16
}
, 0x13 
}
, {
 {
"BRA", AddressingMode::RELATIVE
}
, 0x80 
}
, {
 {
"BRA", AddressingMode::RELATIVE16
}
, 0x83 
}
,
        {
 {
"BRK", AddressingMode::IMPLIED
}
, 0x00 
}
, {
 {
"BSR", AddressingMode::RELATIVE16
}
, 0x63 
}
, {
 {
"BVC", AddressingMode::RELATIVE
}
, 0x50 
}
, {
 {
"BVC", AddressingMode::RELATIVE16
}
, 0x53 
}
, {
 {
"BVS", AddressingMode::RELATIVE
}
, 0x70 
}
, {
 {
"BVS", AddressingMode::RELATIVE16
}
, 0x73 
}
,
        {
 {
"CLC", AddressingMode::IMPLIED
}
, 0x18 
}
, {
 {
"CLD", AddressingMode::IMPLIED
}
, 0xD8 
}
, {
 {
"CLE", AddressingMode::IMPLIED
}
, 0x02 
}
, {
 {
"CLI", AddressingMode::IMPLIED
}
, 0x58 
}
, {
 {
"CLV", AddressingMode::IMPLIED
}
, 0xB8 
}
,
        {
 {
"CMP", AddressingMode::BASE_PAGE_INDIRECT_Y
}
, 0xD1 
}
, {
 {
"CMP", AddressingMode::BASE_PAGE_INDIRECT_Z
}
, 0xD2 
}
, {
 {
"CMP", AddressingMode::FLAT_INDIRECT_Z
}
, 0xD2 
}
, {
 {
"CMP", AddressingMode::STACK_RELATIVE
}
, 0xD2 
}
, {
 {
"CMP", AddressingMode::BASE_PAGE_X_INDIRECT
}
, 0xC1 
}
, {
 {
"CMP", AddressingMode::ABSOLUTE
}
, 0xCD 
}
, {
 {
"CMP", AddressingMode::ABSOLUTE_X
}
, 0xDD 
}
, {
 {
"CMP", AddressingMode::ABSOLUTE_Y
}
, 0xD9 
}
, {
 {
"CMP", AddressingMode::BASE_PAGE
}
, 0xC5 
}
, {
 {
"CMP", AddressingMode::BASE_PAGE_X
}
, 0xD5 
}
, {
 {
"CMP", AddressingMode::IMMEDIATE
}
, 0xC9 
}
,
        {
 {
"CPX", AddressingMode::ABSOLUTE
}
, 0xEC 
}
, {
 {
"CPX", AddressingMode::BASE_PAGE
}
, 0xE4 
}
, {
 {
"CPX", AddressingMode::IMMEDIATE
}
, 0xE0 
}
, {
 {
"CPY", AddressingMode::ABSOLUTE
}
, 0xCC 
}
, {
 {
"CPY", AddressingMode::BASE_PAGE
}
, 0xC4 
}
, {
 {
"CPY", AddressingMode::IMMEDIATE
}
, 0xC0 
}
, {
 {
"CPZ", AddressingMode::ABSOLUTE
}
, 0xDC 
}
, {
 {
"CPZ", AddressingMode::BASE_PAGE
}
, 0xD4 
}
, {
 {
"CPZ", AddressingMode::IMMEDIATE
}
, 0xC2 
}
,
        {
 {
"DEC", AddressingMode::ABSOLUTE
}
, 0xCE 
}
, {
 {
"DEC", AddressingMode::ABSOLUTE_X
}
, 0xDE 
}
, {
 {
"DEC", AddressingMode::ACCUMULATOR
}
, 0x3A 
}
, {
 {
"DEC", AddressingMode::BASE_PAGE
}
, 0xC6 
}
, {
 {
"DEC", AddressingMode::BASE_PAGE_X
}
, 0xD6 
}
, {
 {
"DEW", AddressingMode::BASE_PAGE
}
, 0xC3 
}
, {
 {
"DEX", AddressingMode::IMPLIED
}
, 0xCA 
}
, {
 {
"DEY", AddressingMode::IMPLIED
}
, 0x88 
}
, {
 {
"DEZ", AddressingMode::IMPLIED
}
, 0x3B 
}
, {
 {
"EOM", AddressingMode::IMPLIED
}
, 0xEA 
}
,
        {
 {
"EOR", AddressingMode::BASE_PAGE_INDIRECT_Y
}
, 0x51 
}
, {
 {
"EOR", AddressingMode::BASE_PAGE_INDIRECT_Z
}
, 0x52 
}
, {
 {
"EOR", AddressingMode::FLAT_INDIRECT_Z
}
, 0x52 
}
, {
 {
"EOR", AddressingMode::STACK_RELATIVE
}
, 0x52 
}
, {
 {
"EOR", AddressingMode::BASE_PAGE_X_INDIRECT
}
, 0x41 
}
, {
 {
"EOR", AddressingMode::ABSOLUTE
}
, 0x4D 
}
, {
 {
"EOR", AddressingMode::ABSOLUTE_X
}
, 0x5D 
}
, {
 {
"EOR", AddressingMode::ABSOLUTE_Y
}
, 0x59 
}
, {
 {
"EOR", AddressingMode::BASE_PAGE
}
, 0x45 
}
, {
 {
"EOR", AddressingMode::BASE_PAGE_X
}
, 0x55 
}
, {
 {
"EOR", AddressingMode::IMMEDIATE
}
, 0x49 
}
,
        {
 {
"INC", AddressingMode::ABSOLUTE
}
, 0xEE 
}
, {
 {
"INC", AddressingMode::ABSOLUTE_X
}
, 0xFE 
}
, {
 {
"INC", AddressingMode::ACCUMULATOR
}
, 0x1A 
}
, {
 {
"INC", AddressingMode::BASE_PAGE
}
, 0xE6 
}
, {
 {
"INC", AddressingMode::BASE_PAGE_X
}
, 0xF6 
}
, {
 {
"INW", AddressingMode::BASE_PAGE
}
, 0xE3 
}
, {
 {
"INX", AddressingMode::IMPLIED
}
, 0xE8 
}
, {
 {
"INY", AddressingMode::IMPLIED
}
, 0xC8 
}
, {
 {
"INZ", AddressingMode::IMPLIED
}
, 0x1B 
}
,
        {
 {
"JMP", AddressingMode::ABSOLUTE_INDIRECT
}
, 0x6C 
}
, {
 {
"JMP", AddressingMode::ABSOLUTE_X_INDIRECT
}
, 0x7C 
}
, {
 {
"JMP", AddressingMode::ABSOLUTE
}
, 0x4C 
}
, {
 {
"JSR", AddressingMode::ABSOLUTE_INDIRECT
}
, 0x22 
}
, {
 {
"JSR", AddressingMode::ABSOLUTE_X_INDIRECT
}
, 0x23 
}
, {
 {
"JSR", AddressingMode::ABSOLUTE
}
, 0x20 
}
,
        {
 {
"LDA", AddressingMode::BASE_PAGE_INDIRECT_Y
}
, 0xB1 
}
, {
 {
"LDA", AddressingMode::BASE_PAGE_INDIRECT_Z
}
, 0xB2 
}
, {
 {
"LDA", AddressingMode::FLAT_INDIRECT_Z
}
, 0xB2 
}
, {
 {
"LDA", AddressingMode::STACK_RELATIVE
}
, 0xE2 
}
, {
 {
"LDA", AddressingMode::BASE_PAGE_X_INDIRECT
}
, 0xA1 
}
, {
 {
"LDA", AddressingMode::ABSOLUTE
}
, 0xAD 
}
, {
 {
"LDA", AddressingMode::ABSOLUTE_X
}
, 0xBD 
}
, {
 {
"LDA", AddressingMode::ABSOLUTE_Y
}
, 0xB9 
}
, {
 {
"LDA", AddressingMode::BASE_PAGE
}
, 0xA5 
}
, {
 {
"LDA", AddressingMode::BASE_PAGE_X
}
, 0xB5 
}
, {
 {
"LDA", AddressingMode::IMMEDIATE
}
, 0xA9 
}
,
        {
 {
"LDX", AddressingMode::ABSOLUTE
}
, 0xAE 
}
, {
 {
"LDX", AddressingMode::ABSOLUTE_Y
}
, 0xBE 
}
, {
 {
"LDX", AddressingMode::BASE_PAGE
}
, 0xA6 
}
, {
 {
"LDX", AddressingMode::BASE_PAGE_Y
}
, 0xB6 
}
, {
 {
"LDX", AddressingMode::IMMEDIATE
}
, 0xA2 
}
,
        {
 {
"LDY", AddressingMode::ABSOLUTE
}
, 0xAC 
}
, {
 {
"LDY", AddressingMode::ABSOLUTE_X
}
, 0xBC 
}
, {
 {
"LDY", AddressingMode::BASE_PAGE
}
, 0xA4 
}
, {
 {
"LDY", AddressingMode::BASE_PAGE_X
}
, 0xB4 
}
, {
 {
"LDY", AddressingMode::IMMEDIATE
}
, 0xA0 
}
,
        {
 {
"LDZ", AddressingMode::ABSOLUTE
}
, 0xAB 
}
, {
 {
"LDZ", AddressingMode::ABSOLUTE_X
}
, 0xBB 
}
, {
 {
"LDZ", AddressingMode::IMMEDIATE
}
, 0xA3 
}
, {
 {
"LDQ", AddressingMode::BASE_PAGE
}
, 0xA5 
}
, {
 {
"LDQ", AddressingMode::ABSOLUTE
}
, 0xAD 
}
, {
 {
"LDQ", AddressingMode::BASE_PAGE_INDIRECT_Z
}
, 0xB2 
}
, {
 {
"LDQ", AddressingMode::FLAT_INDIRECT_Z
}
, 0xB2 
}
,
        {
 {
"LSR", AddressingMode::ABSOLUTE
}
, 0x4E 
}
, {
 {
"LSR", AddressingMode::ABSOLUTE_X
}
, 0x5E 
}
, {
 {
"LSR", AddressingMode::ACCUMULATOR
}
, 0x4A 
}
, {
 {
"LSR", AddressingMode::BASE_PAGE
}
, 0x46 
}
, {
 {
"LSR", AddressingMode::BASE_PAGE_X
}
, 0x56 
}
, {
 {
"MAP", AddressingMode::IMPLIED
}
, 0x5C 
}
, {
 {
 "NEG", AddressingMode::ACCUMULATOR
 }
 , 0x42 
 }
 , {
 {
 "NOP", AddressingMode::IMPLIED
 }
 , 0xEA 
 }
 , {
 {
 "ORA", AddressingMode::BASE_PAGE_INDIRECT_Y
 }
 , 0x11 
 }
, {
 {
"ORA", AddressingMode::BASE_PAGE_INDIRECT_Z
}
, 0x12 
}
, {
 {
"ORA", AddressingMode::FLAT_INDIRECT_Z
}
, 0x12 
}
, {
 {
"ORA", AddressingMode::STACK_RELATIVE
}
, 0x12 
}
, {
 {
"ORA", AddressingMode::BASE_PAGE_X_INDIRECT
}
, 0x01 
}
, {
 {
"ORA", AddressingMode::ABSOLUTE
}
, 0x0D 
}
, {
 {
"ORA", AddressingMode::ABSOLUTE_X
}
, 0x1D 
}
, {
 {
"ORA", AddressingMode::ABSOLUTE_Y
}
, 0x19 
}
, {
 {
"ORA", AddressingMode::BASE_PAGE
}
, 0x05 
}
, {
 {
"ORA", AddressingMode::BASE_PAGE_X
}
, 0x15 
}
, {
 {
"ORA", AddressingMode::IMMEDIATE
}
, 0x09 
}
,
        {
 {
"PHA", AddressingMode::IMPLIED
}
, 0x48 
}
, {
 {
"PHP", AddressingMode::IMPLIED
}
, 0x08 
}
, {
 {
"PHW", AddressingMode::ABSOLUTE
}
, 0xFC 
}
, {
 {
"PHW", AddressingMode::IMMEDIATE16
}
, 0xF4 
}
, {
 {
"PHX", AddressingMode::IMPLIED
}
, 0xDA 
}
, {
 {
"PHY", AddressingMode::IMPLIED
}
, 0x5A 
}
, {
 {
"PHZ", AddressingMode::IMPLIED
}
, 0xDB 
}
, {
 {
"PLA", AddressingMode::IMPLIED
}
, 0x68 
}
, {
 {
"PLP", AddressingMode::IMPLIED
}
, 0x28 
}
, {
 {
"PLX", AddressingMode::IMPLIED
}
, 0xFA 
}
, {
 {
"PLY", AddressingMode::IMPLIED
}
, 0x7A 
}
, {
 {
"PLZ", AddressingMode::IMPLIED
}
, 0xFB 
}
,
        {
 {
"RMB0", AddressingMode::BASE_PAGE
}
, 0x07 
}
, {
 {
"RMB1", AddressingMode::BASE_PAGE
}
, 0x17 
}
, {
 {
"RMB2", AddressingMode::BASE_PAGE
}
, 0x27 
}
, {
 {
"RMB3", AddressingMode::BASE_PAGE
}
, 0x37 
}
, {
 {
"RMB4", AddressingMode::BASE_PAGE
}
, 0x47 
}
, {
 {
"RMB5", AddressingMode::BASE_PAGE
}
, 0x57 
}
, {
 {
"RMB6", AddressingMode::BASE_PAGE
}
, 0x67 
}
, {
 {
"RMB7", AddressingMode::BASE_PAGE
}
, 0x77 
}
,
        {
 {
"ROL", AddressingMode::ABSOLUTE
}
, 0x2E 
}
, {
 {
"ROL", AddressingMode::ABSOLUTE_X
}
, 0x3E 
}
, {
 {
"ROL", AddressingMode::ACCUMULATOR
}
, 0x2A 
}
, {
 {
"ROL", AddressingMode::BASE_PAGE
}
, 0x26 
}
, {
 {
"ROL", AddressingMode::BASE_PAGE_X
}
, 0x36 
}
, {
 {
"ROR", AddressingMode::ABSOLUTE
}
, 0x6E 
}
, {
 {
"ROR", AddressingMode::ABSOLUTE_X
}
, 0x7E 
}
, {
 {
"ROR", AddressingMode::ACCUMULATOR
}
, 0x6A 
}
, {
 {
"ROR", AddressingMode::BASE_PAGE
}
, 0x66 
}
, {
 {
"ROR", AddressingMode::BASE_PAGE_X
}
, 0x76 
}
, {
 {
"ROW", AddressingMode::ABSOLUTE
}
, 0xEB 
}
,
        {
 {
"RTI", AddressingMode::IMPLIED
}
, 0x40 
}
, {
 {
"RTS", AddressingMode::IMMEDIATE
}
, 0x62 
}
, {
 {
"RTS", AddressingMode::IMPLIED
}
, 0x60 
}
,
        {
 {
"SBC", AddressingMode::BASE_PAGE_INDIRECT_Y
}
, 0xF1 
}
, {
 {
"SBC", AddressingMode::BASE_PAGE_INDIRECT_Z
}
, 0xF2 
}
, {
 {
"SBC", AddressingMode::FLAT_INDIRECT_Z
}
, 0xF2 
}
, {
 {
"SBC", AddressingMode::STACK_RELATIVE
}
, 0xF2 
}
, {
 {
"SBC", AddressingMode::BASE_PAGE_X_INDIRECT
}
, 0xE1 
}
, {
 {
"SBC", AddressingMode::ABSOLUTE
}
, 0xED 
}
, {
 {
"SBC", AddressingMode::ABSOLUTE_X
}
, 0xFD 
}
, {
 {
"SBC", AddressingMode::ABSOLUTE_Y
}
, 0xF9 
}
, {
 {
"SBC", AddressingMode::BASE_PAGE
}
, 0xE5 
}
, {
 {
"SBC", AddressingMode::BASE_PAGE_X
}
, 0xF5 
}
, {
 {
"SBC", AddressingMode::IMMEDIATE
}
, 0xE9 
}
,
        {
 {
"SEC", AddressingMode::IMPLIED
}
, 0x38 
}
, {
 {
"SED", AddressingMode::IMPLIED
}
, 0xF8 
}
, {
 {
"SEE", AddressingMode::IMPLIED
}
, 0x03 
}
, {
 {
"SEI", AddressingMode::IMPLIED
}
, 0x78 
}
, {
 {
"SMB0", AddressingMode::BASE_PAGE
}
, 0x87 
}
, {
 {
"SMB1", AddressingMode::BASE_PAGE
}
, 0x97 
}
, {
 {
"SMB2", AddressingMode::BASE_PAGE
}
, 0xA7 
}
, {
 {
"SMB3", AddressingMode::BASE_PAGE
}
, 0xB7 
}
, {
 {
"SMB4", AddressingMode::BASE_PAGE
}
, 0xC7 
}
, {
 {
"SMB5", AddressingMode::BASE_PAGE
}
, 0xD7 
}
, {
 {
"SMB6", AddressingMode::BASE_PAGE
}
, 0xE7 
}
, {
 {
"SMB7", AddressingMode::BASE_PAGE
}
, 0xF7 
}
,
        {
 {
"STA", AddressingMode::BASE_PAGE_INDIRECT_Y
}
, 0x91 
}
, {
 {
"STA", AddressingMode::BASE_PAGE_INDIRECT_Z
}
, 0x92 
}
, {
 {
"STA", AddressingMode::FLAT_INDIRECT_Z
}
, 0x92 
}
, {
 {
"STA", AddressingMode::STACK_RELATIVE
}
, 0x82 
}
, {
 {
"STA", AddressingMode::BASE_PAGE_X_INDIRECT
}
, 0x81 
}
, {
 {
"STA", AddressingMode::ABSOLUTE
}
, 0x8D 
}
, {
 {
"STA", AddressingMode::ABSOLUTE_X
}
, 0x9D 
}
, {
 {
"STA", AddressingMode::ABSOLUTE_Y
}
, 0x99 
}
, {
 {
"STA", AddressingMode::BASE_PAGE
}
, 0x85 
}
, {
 {
"STA", AddressingMode::BASE_PAGE_X
}
, 0x95 
}
,
        {
 {
"STX", AddressingMode::ABSOLUTE
}
, 0x8E 
}
, {
 {
"STX", AddressingMode::ABSOLUTE_Y
}
, 0x9B 
}
, {
 {
"STX", AddressingMode::BASE_PAGE
}
, 0x86 
}
, {
 {
"STX", AddressingMode::BASE_PAGE_Y
}
, 0x96 
}
,
        {
 {
"STY", AddressingMode::ABSOLUTE
}
, 0x8C 
}
, {
 {
"STY", AddressingMode::ABSOLUTE_X
}
, 0x8B 
}
, {
 {
"STY", AddressingMode::BASE_PAGE
}
, 0x84 
}
, {
 {
"STY", AddressingMode::BASE_PAGE_X
}
, 0x94 
}
,
        {
 {
"STZ", AddressingMode::ABSOLUTE
}
, 0x9C 
}
, {
 {
"STZ", AddressingMode::ABSOLUTE_X
}
, 0x9E 
}
, {
 {
"STZ", AddressingMode::BASE_PAGE
}
, 0x64 
}
, {
 {
"STZ", AddressingMode::BASE_PAGE_X
}
, 0x74 
}
, {
 {
"STQ", AddressingMode::BASE_PAGE
}
, 0x85 
}
, {
 {
"STQ", AddressingMode::ABSOLUTE
}
, 0x8D 
}
, {
 {
"STQ", AddressingMode::BASE_PAGE_INDIRECT_Z
}
, 0x92 
}
, {
 {
"STQ", AddressingMode::FLAT_INDIRECT_Z
}
, 0x92 
}
,
        {
 {
"TAB", AddressingMode::IMPLIED
}
, 0x5B 
}
, {
 {
"TAX", AddressingMode::IMPLIED
}
, 0xAA 
}
, {
 {
"TAY", AddressingMode::IMPLIED
}
, 0xA8 
}
, {
 {
"TAZ", AddressingMode::IMPLIED
}
, 0x4B 
}
, {
 {
"TBA", AddressingMode::IMPLIED
}
, 0x7B 
}
, {
 {
"TRB", AddressingMode::ABSOLUTE
}
, 0x1C 
}
, {
 {
"TRB", AddressingMode::BASE_PAGE
}
, 0x14 
}
, {
 {
"TSB", AddressingMode::ABSOLUTE
}
, 0x0C 
}
, {
 {
"TSB", AddressingMode::BASE_PAGE
}
, 0x04 
}
, {
 {
"TSX", AddressingMode::IMPLIED
}
, 0xBA 
}
, {
 {
"TSY", AddressingMode::IMPLIED
}
, 0x0B 
}
, {
 {
"TXA", AddressingMode::IMPLIED
}
, 0x8A 
}
, {
 {
"TXS", AddressingMode::IMPLIED
}
, 0x9A 
}
, {
 {
"TYA", AddressingMode::IMPLIED
}
, 0x98 
}
, {
 {
"TYS", AddressingMode::IMPLIED
}
, 0x2B 
}
, {
 {
"TZA", AddressingMode::IMPLIED
}
, 0x6B 
}
,
    
}
;

    return opcodes;


}


uint8_t AssemblerParser::getOpcode(const std::string& m, AddressingMode mode) {

    std::string baseM = m;
 if (m.size() > 1 && m.back() == 'Q' && m != "LDQ" && m != "STQ" && m.substr(0, 3) != "BEQ" && m.substr(0, 3) != "BNE" && m.substr(0, 3) != "BRA" && m.substr(0, 3) != "BSR") baseM = m.substr(0, m.size() - 1);

    const auto& opMap = getOpcodeMapInternal();
 auto it = opMap.find({
baseM, mode
}
);
 if (it != opMap.end()) return it->second;
 return 0;


}


std::vector<AddressingMode> AssemblerParser::getValidAddressingModes(const std::string& mnemonic) {

    std::string baseM = mnemonic;
 if (mnemonic.size() > 1 && mnemonic.back() == 'Q' && mnemonic != "LDQ" && mnemonic != "STQ" && mnemonic.substr(0, 3) != "BEQ" && mnemonic.substr(0, 3) != "BNE" && mnemonic.substr(0, 3) != "BRA" && mnemonic.substr(0, 3) != "BSR") baseM = mnemonic.substr(0, mnemonic.size() - 1);

    std::vector<AddressingMode> modes;
 const auto& opMap = getOpcodeMapInternal();
 for (const auto& entry : opMap) if (entry.first.first == baseM) modes.push_back(entry.first.second);

    return modes;


}


void AssemblerParser::emitExpressionCode(std::vector<uint8_t>& binary, const std::string& target, int tokenIndex, const std::string& scopePrefix) {

    int idx = tokenIndex;
 auto ast = parseExprAST(tokens, idx, symbolTable, scopePrefix);
 if (!ast) return;

    int width = 8;
 if (target == ".AX" || target == ".AY" || target == ".AZ") width = 16;
 else if (target == ".AXY" || target == ".AXZ") width = 24;
 else if (target == ".Q" || target == ".AXYZ") width = 32;
 else if (target[0] != '.') width = 16;

    if (ast->isConstant(this)) {
 uint32_t val = ast->getValue(this);
 M65Emitter e(binary, getZPStart());
 e.lda_imm(val & 0xFF);
 if (width >= 16) e.ldx_imm((val >> 8) & 0xFF);
 
}

    else ast->emit(binary, this, width, target);

    if (target[0] != '.') {
 Symbol* sym = resolveSymbol(target, scopePrefix);
 uint32_t addr = sym ? sym->value : parseNumericLiteral(target);
 M65Emitter e(binary, getZPStart());
 e.sta_abs(addr);
 if (width >= 16) {
 e.txa();
 e.sta_abs(addr + 1);
 
}
 
}


}


void AssemblerParser::emitMulCode(std::vector<uint8_t>& binary, int width, const std::string& dest, int tokenIndex, const std::string& scopePrefix) {

    int bytes = width / 8;
 if (bytes < 1) bytes = 1;
 if (bytes > 4) bytes = 4;

    int idx = tokenIndex;
 auto srcAst = parseExprAST(tokens, idx, symbolTable, scopePrefix);
 if (!srcAst) return;

    M65Emitter e(binary, getZPStart());
 auto storeMath = [&](uint8_t base, int i, const std::string& src) {
 if (src == ".A") e.sta_abs(0xD700 + base + i);
 else if (src == ".X") {
 e.txa();
 e.sta_abs(0xD700 + base + i);
 
}
 else if (src == ".Y") {
 e.tya();
 e.sta_abs(0xD700 + base + i);
 
}
 else if (src == ".Z") {
 e.tza();
 e.sta_abs(0xD700 + base + i);
 
}
 else {
 Symbol* sym = resolveSymbol(src, scopePrefix);
 uint32_t addr = (sym ? sym->value : parseNumericLiteral(src)) + i;
 e.lda_abs(addr);
 e.sta_abs(0xD700 + base + i);
 
}
 
}
;

    if (dest == ".A" || dest == ".AX" || dest == ".AXY" || dest == ".AXYZ" || dest == ".Q") {
 if (bytes >= 1) storeMath(0x70, 0, ".A");
 if (bytes >= 2) storeMath(0x70, 1, ".X");
 if (bytes >= 3) storeMath(0x70, 2, ".Y");
 if (bytes >= 4) storeMath(0x70, 3, ".Z");
 
}
 else for (int i = 0;
 i < bytes;
 ++i) storeMath(0x70, i, dest);

    if (srcAst->isConstant(this)) {
 uint32_t val = srcAst->getValue(this);
 for (int i = 0;
 i < bytes;
 ++i) {
 e.lda_imm((val >> (i * 8)) & 0xFF);
 e.sta_abs(0xD774 + i);
 
}
 
}

    else {
 std::string srcName = tokens[tokenIndex].value;
 if (tokens[tokenIndex].type == AssemblerTokenType::REGISTER) srcName = "." + srcName;
 if (srcName == ".A" || srcName == ".AX" || srcName == ".AXY" || srcName == ".AXYZ" || srcName == ".Q") {
 if (bytes >= 1) storeMath(0x74, 0, ".A");
 if (bytes >= 2) storeMath(0x74, 1, ".X");
 if (bytes >= 3) storeMath(0x74, 2, ".Y");
 if (bytes >= 4) storeMath(0x74, 3, ".Z");
 
}
 else for (int i = 0;
 i < bytes;
 ++i) storeMath(0x74, i, srcName);
 
}

    for (int i = 0;
 i < bytes;
 ++i) {
 e.lda_abs(0xD778 + i);
 if (dest == ".A" || dest == ".AX" || dest == ".AXY" || dest == ".AXYZ" || dest == ".Q") {
 if (i == 1) e.tax();
 else if (i == 2) e.tay();
 else if (i == 3) e.taz();
 
}
 else {
 Symbol* sym = resolveSymbol(dest, scopePrefix);
 uint32_t addr = sym ? sym->value : parseNumericLiteral(dest);
 e.sta_abs(addr + i);
 
}
 
}


}


void AssemblerParser::emitDivCode(std::vector<uint8_t>& binary, int width, const std::string& dest, int tokenIndex, const std::string& scopePrefix) {

    int bytes = width / 8;
 if (bytes < 1) bytes = 1;
 if (bytes > 4) bytes = 4;

    int idx = tokenIndex;
 auto srcAst = parseExprAST(tokens, idx, symbolTable, scopePrefix);
 if (!srcAst) return;

    M65Emitter e(binary, getZPStart());
 auto storeMath = [&](uint8_t base, int i, const std::string& src) {
 if (src == ".A") e.sta_abs(0xD700 + base + i);
 else if (src == ".X") {
 e.txa();
 e.sta_abs(0xD700 + base + i);
 
}
 else if (src == ".Y") {
 e.tya();
 e.sta_abs(0xD700 + base + i);
 
}
 else if (src == ".Z") {
 e.tza();
 e.sta_abs(0xD700 + base + i);
 
}
 else {
 Symbol* sym = resolveSymbol(src, scopePrefix);
 uint32_t addr = (sym ? sym->value : parseNumericLiteral(src)) + i;
 e.lda_abs(addr);
 e.sta_abs(0xD700 + base + i);
 
}
 
}
;

    if (dest == ".A" || dest == ".AX" || dest == ".AXY" || dest == ".AXYZ" || dest == ".Q") {
 if (bytes >= 1) storeMath(0x60, 0, ".A");
 if (bytes >= 2) storeMath(0x60, 1, ".X");
 if (bytes >= 3) storeMath(0x60, 2, ".Y");
 if (bytes >= 4) storeMath(0x60, 3, ".Z");
 
}
 else for (int i = 0;
 i < bytes;
 ++i) storeMath(0x60, i, dest);

    if (srcAst->isConstant(this)) {
 uint32_t val = srcAst->getValue(this);
 if (val == 0) throw std::runtime_error("Division by zero at assembly time (constant divisor 0) at line " + std::to_string(tokens[tokenIndex].line));
 for (int i = 0;
 i < bytes;
 ++i) {
 e.lda_imm((val >> (i * 8)) & 0xFF);
 e.sta_abs(0xD764 + i);
 
}
 
}

    else {
 int tIdx = tokenIndex;
 std::string srcName = tokens[tIdx].value;
 if (tokens[tIdx].type == AssemblerTokenType::REGISTER) srcName = "." + srcName;
 if (srcName == ".A" || srcName == ".AX" || srcName == ".AXY" || srcName == ".AXYZ" || srcName == ".Q") {
 if (bytes >= 1) storeMath(0x64, 0, ".A");
 if (bytes >= 2) storeMath(0x64, 1, ".X");
 if (bytes >= 3) storeMath(0x64, 2, ".Y");
 if (bytes >= 4) storeMath(0x64, 3, ".Z");
 
}
 else for (int i = 0;
 i < bytes;
 ++i) storeMath(0x64, i, srcName);
 
}

    e.bit_abs(0xD70F);
 e.bne(-5);
 for (int i = 0;
 i < bytes;
 ++i) {
 e.lda_abs(0xD768 + i);
 if (dest == ".A" || dest == ".AX" || dest == ".AXY" || dest == ".AXYZ" || dest == ".Q") {
 if (i == 1) e.tax();
 else if (i == 2) e.tay();
 else if (i == 3) e.taz();
 
}
 else {
 Symbol* sym = resolveSymbol(dest, scopePrefix);
 uint32_t addr = sym ? sym->value : parseNumericLiteral(dest);
 e.sta_abs(addr + i);
 
}
 
}


}


void AssemblerParser::emitStackIncDecCode(std::vector<uint8_t>& binary, bool isInc, int tokenIndex, const std::string& scopePrefix) {

    uint32_t offset = evaluateExpressionAt(tokenIndex, scopePrefix);
 M65Emitter e(binary, getZPStart());
 e.tsx();
 if (isInc) {
 e.inc_abs_x(0x0101 + offset);
 e.bne(0x03);
 e.inc_abs_x(0x0101 + offset + 1);
 
}
 else {
 e.lda_abs_x(0x0101 + offset);
 e.bne(0x03);
 e.dec_abs_x(0x0101 + offset + 1);
 e.dec_abs_x(0x0101 + offset);
 
}


}


void AssemblerParser::optimize() {
    struct RegState {
        bool known = false;
        std::string var; 
        AddressingMode mode;
        std::string imm; // Last immediate value loaded
    };
    RegState regA, regX, regY, regZ;

    auto invalidate = [&](RegState& r) { r.known = false; r.var = ""; r.imm = ""; };

    for (size_t i = 0; i < statements.size(); ++i) {
        Statement* s = statements[i].get();
        if (s->deleted) continue;

        // Barrier: Non-dont-care labels reset all knowledge
        if (!s->label.empty() && s->label[0] != '@') {
            invalidate(regA); invalidate(regX); invalidate(regY); invalidate(regZ);
        }

        if (s->type == Statement::INSTRUCTION || s->type == Statement::LDW || s->type == Statement::STW) {
            std::string m = (s->type == Statement::INSTRUCTION) ? s->instr.mnemonic : (s->type == Statement::LDW ? "LDAX" : "STAX");
            AddressingMode mode = s->instr.mode;
            std::string op = s->instr.operand;
            
            if (s->type == Statement::LDW) {
                std::string regStr = s->instr.operand; 
                std::transform(regStr.begin(), regStr.end(), regStr.begin(), ::toupper);
                char r2 = 'X';
                if (regStr == ".AY") r2 = 'Y';
                else if (regStr == ".AZ") r2 = 'Z';
                
                // Track A and the second register
                regA.known = true; regA.mode = AddressingMode::NONE; // Word loads are complex
                if (r2 == 'X') regX.known = true;
                else if (r2 == 'Y') regY.known = true;
                else if (r2 == 'Z') regZ.known = true;
                
                // If it's immediate, we can track the values
                // But simulated LDW doesn't store the value in s->instr.operand if it's not a simple literal
                // For now, just invalidate to be safe but track that they ARE known variables if applicable
                continue;
            }

            if (s->type == Statement::STW) {
                continue; // Storing doesn't necessarily clobber (if simulated correctly)
            }

            // 4.2 Load-after-Store (Existing logic)
            if (m == "LDA" && ( (regA.known && regA.mode == mode && regA.var == op) || (mode == AddressingMode::IMMEDIATE && regA.imm == op) )) {
                s->deleted = true; s->size = 0; continue;
            }
            if (m == "LDX" && ( (regX.known && regX.mode == mode && regX.var == op) || (mode == AddressingMode::IMMEDIATE && regX.imm == op) )) {
                s->deleted = true; s->size = 0; continue;
            }
            if (m == "LDY" && ( (regY.known && regY.mode == mode && regY.var == op) || (mode == AddressingMode::IMMEDIATE && regY.imm == op) )) {
                s->deleted = true; s->size = 0; continue;
            }
            if (m == "LDZ" && ( (regZ.known && regZ.mode == mode && regZ.var == op) || (mode == AddressingMode::IMMEDIATE && regZ.imm == op) )) {
                s->deleted = true; s->size = 0; continue;
            }

            // LDY #0
            if (m == "LDY" && mode == AddressingMode::IMMEDIATE && op == "#$00" && regY.imm == "#$00") {
                s->deleted = true; s->size = 0; continue;
            }

            // Update knowledge
            if (m == "LDA") { 
                regA.known = true; regA.mode = mode; regA.var = op; 
                if (mode == AddressingMode::IMMEDIATE) regA.imm = op; else regA.imm = "";
            }
            else if (m == "STA") { 
                if (mode != AddressingMode::IMMEDIATE) { regA.known = true; regA.mode = mode; regA.var = op; }
            }
            else if (m == "LDX") { 
                regX.known = true; regX.mode = mode; regX.var = op; 
                if (mode == AddressingMode::IMMEDIATE) regX.imm = op; else regX.imm = "";
            }
            else if (m == "STX") { 
                if (mode != AddressingMode::IMMEDIATE) { regX.known = true; regX.mode = mode; regX.var = op; }
            }
            else if (m == "LDY") { 
                regY.known = true; regY.mode = mode; regY.var = op; 
                if (mode == AddressingMode::IMMEDIATE) regY.imm = op; else regY.imm = "";
            }
            else if (m == "STY") { 
                if (mode != AddressingMode::IMMEDIATE) { regY.known = true; regY.mode = mode; regY.var = op; }
            }
            else if (m == "LDZ") { 
                regZ.known = true; regZ.mode = mode; regZ.var = op; 
                if (mode == AddressingMode::IMMEDIATE) regZ.imm = op; else regZ.imm = "";
            }
            else if (m == "STZ") { 
                if (mode != AddressingMode::IMMEDIATE) { regZ.known = true; regZ.mode = mode; regZ.var = op; }
            }
            else if (m == "TAX") { regX = regA; }
            else if (m == "TXA") { regA = regX; }
            else if (m == "TAY") { regY = regA; }
            else if (m == "TYA") { regA = regY; }
            else if (m == "TAZ") { regZ = regA; }
            else if (m == "TZA") { regA = regZ; }
            else if (m == "JMP" && mode == AddressingMode::ABSOLUTE) {
                // 4.1 Branch Shortening: JMP -> BRA (BRA can be 2 or 3 bytes, JMP is always 3)
                // Only convert if it's a label, to avoid breaking explicit absolute jumps and tests
                if (s->instr.operandTokenIndex != -1 && tokens[s->instr.operandTokenIndex].type == AssemblerTokenType::IDENTIFIER) {
                    s->instr.mnemonic = "BRA";
                }
                invalidate(regA); invalidate(regX); invalidate(regY); invalidate(regZ);
            }
            else if (m == "PHA" || m == "PLA" || m == "PHX" || m == "PLX" || m == "PHY" || m == "PLY" || m == "PHZ" || m == "PLZ" || m == "PHW" || m == "JSR" || m == "CALL" || m == "RTN" || m == "RTS") {
                // Stack or subroutine calls invalidate everything to be safe
                invalidate(regA); invalidate(regX); invalidate(regY); invalidate(regZ);
            }
            else {
                // Most other instructions change a register or flags in ways we don't track here yet
                // For Load-after-Store, we only care if the register was explicitly changed.
                // ADC, SBC, AND, ORA, EOR change A.
                if (m == "ADC" || m == "SBC" || m == "AND" || m == "ORA" || m == "EOR" || m == "ASL" || m == "LSR" || m == "ROL" || m == "ROR" || m == "DEC" || m == "INC") {
                    invalidate(regA); // Simplified: assume any of these change A if Implied or Accumulator
                    // Actually they might change X/Y/Z if they are INX etc.
                }
                if (m == "INX" || m == "DEX") invalidate(regX);
                if (m == "INY" || m == "DEY") invalidate(regY);
                if (m == "INZ" || m == "DEZ") invalidate(regZ);
                
                // If it's a branch, we don't know where we came from after the branch target
                if (m == "BRA" || m == "BEQ" || m == "BNE" || m == "BCC" || m == "BCS" || m == "BPL" || m == "BMI" || m == "BVC" || m == "BVS") {
                    // We don't invalidate here because we are following the linear path.
                    // But we must invalidate at the target label (handled above).
                }
            }
        } else if (s->type == Statement::EXPR || s->type == Statement::MUL || s->type == Statement::DIV || s->type == Statement::STACK_INC || s->type == Statement::STACK_DEC) {
            invalidate(regA); invalidate(regX); invalidate(regY); invalidate(regZ);
        }
    }
}


void AssemblerParser::emitStackIncDec8Code(std::vector<uint8_t>& binary, bool isInc, int tokenIndex, const std::string& scopePrefix) {
    uint32_t offset = evaluateExpressionAt(tokenIndex, scopePrefix);
    M65Emitter e(binary, getZPStart());
    e.tsx();
    if (isInc) {
        e.inc_abs_x(0x0101 + offset);
    } else {
        e.dec_abs_x(0x0101 + offset);
    }
}


void AssemblerParser::emitAddSub16Code(std::vector<uint8_t>& binary, bool isAdd, const std::string& dest, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    int idx = tokenIndex;
    auto srcAst = parseExprAST(tokens, idx, symbolTable, scopePrefix);
    if (!srcAst) return;

    std::string DEST = dest;
    if (!DEST.empty() && DEST[0] != '.') DEST = "." + DEST;
    std::transform(DEST.begin(), DEST.end(), DEST.begin(), ::toupper);

    if (DEST == ".AX") {
        if (isAdd) e.clc(); else e.sec();
        
        if (srcAst->isConstant(this)) {
            uint32_t val = srcAst->getValue(this);
            if (isAdd) e.adc_imm(val & 0xFF); else e.sbc_imm(val & 0xFF);
            e.pha();
            e.txa();
            if (isAdd) e.adc_imm((val >> 8) & 0xFF); else e.sbc_imm((val >> 8) & 0xFF);
            e.tax();
            e.pla();
        } else {
            // Memory or other src
            std::string src = tokens[tokenIndex].value;
            if (tokens[tokenIndex].type == AssemblerTokenType::REGISTER) src = "." + src;
            else if (!src.empty() && src[0] != '.' && (src=="A"||src=="X"||src=="Y"||src=="Z"||src=="a"||src=="x"||src=="y"||src=="z")) src = "." + src;
            
            Symbol* sym = resolveSymbol(src, scopePrefix);
            uint32_t addr = sym ? sym->value : parseNumericLiteral(src);
            
            if (isAdd) e.adc_abs(addr); else e.sbc_abs(addr);
            e.pha();
            e.txa();
            if (isAdd) e.adc_abs(addr + 1); else e.sbc_abs(addr + 1);
            e.tax();
            e.pla();
        }
    } else {
        throw std::runtime_error("Simulated ADD.16/SUB.16 currently only supports .AX destination, found " + DEST);
    }
}

void AssemblerParser::emitBitwise16Code(std::vector<uint8_t>& binary, const std::string& mnemonic, const std::string& dest, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    int idx = tokenIndex;
    auto srcAst = parseExprAST(tokens, idx, symbolTable, scopePrefix);
    if (!srcAst) return;

    std::string DEST = dest;
    if (!DEST.empty() && DEST[0] != '.') DEST = "." + DEST;
    std::transform(DEST.begin(), DEST.end(), DEST.begin(), ::toupper);

    std::string M = mnemonic;
    std::transform(M.begin(), M.end(), M.begin(), ::toupper);

    if (DEST == ".AX") {
        if (srcAst->isConstant(this)) {
            uint32_t val = srcAst->getValue(this);
            if (M == "AND.16") e.and_imm(val & 0xFF);
            else if (M == "ORA.16") e.ora_imm(val & 0xFF);
            else if (M == "EOR.16") e.eor_imm(val & 0xFF);

            e.pha();
            e.txa();
            
            if (M == "AND.16") e.and_imm((val >> 8) & 0xFF);
            else if (M == "ORA.16") e.ora_imm((val >> 8) & 0xFF);
            else if (M == "EOR.16") e.eor_imm((val >> 8) & 0xFF);
            
            e.tax();
            e.pla();
        } else {
            std::string src = tokens[tokenIndex].value;
            if (tokens[tokenIndex].type == AssemblerTokenType::REGISTER) src = "." + src;
            else if (!src.empty() && src[0] != '.' && (src=="A"||src=="X"||src=="Y"||src=="Z"||src=="a"||src=="x"||src=="y"||src=="z")) src = "." + src;
            Symbol* sym = resolveSymbol(src, scopePrefix);
            uint32_t addr = sym ? sym->value : parseNumericLiteral(src);
            
            if (M == "AND.16") e.and_abs(addr);
            else if (M == "ORA.16") e.ora_abs(addr);
            else if (M == "EOR.16") e.eor_abs(addr);

            e.pha();
            e.txa();

            if (M == "AND.16") e.and_abs(addr + 1);
            else if (M == "ORA.16") e.ora_abs(addr + 1);
            else if (M == "EOR.16") e.eor_abs(addr + 1);

            e.tax();
            e.pla();
        }
    } else {
        throw std::runtime_error("Simulated bitwise 16-bit currently only supports .AX destination");
    }
}

void AssemblerParser::emitCPWCode(std::vector<uint8_t>& binary, const std::string& src1, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    int idx = tokenIndex;
    auto src2Ast = parseExprAST(tokens, idx, symbolTable, scopePrefix);
    if (!src2Ast) return;

    std::string SRC1 = src1;
    if (!SRC1.empty() && SRC1[0] != '.') SRC1 = "." + SRC1;
    std::transform(SRC1.begin(), SRC1.end(), SRC1.begin(), ::toupper);

    if (SRC1 == ".AX") {
        if (src2Ast->isConstant(this)) {
            uint32_t val = src2Ast->getValue(this);
            e.cmp_imm(val & 0xFF);
            e.bne(0x03); 
            e.txa();
            e.cmp_imm((val >> 8) & 0xFF);
        } else {
            std::string src2 = tokens[tokenIndex].value;
            if (tokens[tokenIndex].type == AssemblerTokenType::REGISTER) src2 = "." + src2;
            else if (!src2.empty() && src2[0] != '.' && (src2=="A"||src2=="X"||src2=="Y"||src2=="Z"||src2=="a"||src2=="x"||src2=="y"||src2=="z")) src2 = "." + src2;
            Symbol* sym = resolveSymbol(src2, scopePrefix);
            uint32_t addr = sym ? sym->value : parseNumericLiteral(src2);
            e.cmp_abs(addr);
            e.bne(0x04);
            e.txa();
            e.cmp_abs(addr + 1);
        }
    } else {
        throw std::runtime_error("Simulated CPW currently only supports .AX as first operand, found " + SRC1);
    }
}

void AssemblerParser::emitLDWCode(std::vector<uint8_t>& binary, const std::string& dest, int tokenIndex, const std::string& scopePrefix, bool forceStack) {
    M65Emitter e(binary, getZPStart());
    int idx = tokenIndex;
    
    uint32_t offset = 0;
    bool isStack = false;
    if (forceStack) {
        isStack = true;
        offset = evaluateExpressionAt(tokenIndex, scopePrefix);
    } else {
        isStack = isStackRelativeOperand(tokenIndex, offset, scopePrefix);
    }

    std::string DEST = dest;
    if (!DEST.empty() && DEST[0] != '.') DEST = "." + DEST;
    std::transform(DEST.begin(), DEST.end(), DEST.begin(), ::toupper);

    if (DEST == ".AX" || DEST == ".AY" || DEST == ".AZ") {
        char reg2 = DEST[2]; // 'X', 'Y' or 'Z'
        if (isStack) {
            e.lda_stack(offset);
            e.pha();
            e.lda_stack(offset + 1);
            if (reg2 == 'X') e.tax();
            else if (reg2 == 'Y') e.tya();
            else if (reg2 == 'Z') e.tza();
            e.pla();
        } else {
            bool isImm = false;
            if (idx < (int)tokens.size() && tokens[idx].type == AssemblerTokenType::HASH) {
                isImm = true;
                idx++;
            }
            auto srcAst = parseExprAST(tokens, idx, symbolTable, scopePrefix);
            if (!srcAst) return;
            if (isImm || srcAst->isConstant(this)) {
                uint32_t val = srcAst->getValue(this);
                e.lda_imm(val & 0xFF);
                uint8_t val2 = (val >> 8) & 0xFF;
                if (reg2 == 'X') e.ldx_imm(val2);
                else if (reg2 == 'Y') e.ldy_imm(val2);
                else if (reg2 == 'Z') e.ldz_imm(val2);
            } else {
                std::string src = tokens[tokenIndex].value;
                if (tokens[tokenIndex].type == AssemblerTokenType::REGISTER) src = "." + src;
                else if (!src.empty() && src[0] != '.' && (src=="A"||src=="X"||src=="Y"||src=="Z"||src=="a"||src=="x"||src=="y"||src=="z")) src = "." + src;
                Symbol* sym = resolveSymbol(src, scopePrefix);
                uint32_t addr = sym ? sym->value : parseNumericLiteral(src);
                e.lda_abs(addr);
                uint32_t addr2 = addr + 1;
                if (reg2 == 'X') e.ldx_abs(addr2);
                else if (reg2 == 'Y') e.ldy_abs(addr2);
                else if (reg2 == 'Z') e.ldz_abs(addr2);
            }
        }
    } else {
        throw std::runtime_error("Simulated LDW currently only supports .AX, .AY, .AZ destinations, found " + DEST);
    }
}

void AssemblerParser::emitSTWCode(std::vector<uint8_t>& binary, const std::string& src, int tokenIndex, const std::string& scopePrefix, bool forceStack) {
    M65Emitter e(binary, getZPStart());
    int idx = tokenIndex;
    
    uint32_t offset = 0;
    bool isStack = false;
    if (forceStack) {
        isStack = true;
        offset = evaluateExpressionAt(tokenIndex, scopePrefix);
    } else {
        isStack = isStackRelativeOperand(tokenIndex, offset, scopePrefix);
    }

    std::string SRC = src;
    if (!SRC.empty() && SRC[0] != '.' && SRC[0] != '#') SRC = "." + SRC;
    std::transform(SRC.begin(), SRC.end(), SRC.begin(), ::toupper);

    if (SRC[0] == '#') {
        // STW #imm, dest
        int valIdx = tokenIndex - 1;
        while (valIdx >= 0 && tokens[valIdx].type != AssemblerTokenType::HASH) valIdx--;
        if (valIdx >= 0) valIdx++;
        else valIdx = tokenIndex - 2; // Fallback

        uint32_t val = evaluateExpressionAt(valIdx, scopePrefix);
        uint8_t low = val & 0xFF;
        uint8_t high = (val >> 8) & 0xFF;

        if (isStack) {
            e.lda_imm(low);
            e.sta_stack(offset);
            if (high == 0) e.stz_stack(offset + 1);
            else { e.lda_imm(high); e.sta_stack(offset + 1); }
        } else {
            std::string dest = tokens[tokenIndex].value;
            if (tokens[tokenIndex].type == AssemblerTokenType::REGISTER) dest = "." + dest;
            Symbol* sym = resolveSymbol(dest, scopePrefix);
            uint32_t addr = sym ? sym->value : parseNumericLiteral(dest);
            e.lda_imm(low);
            e.sta_abs(addr);
            if (high == 0) e.stz_abs(addr + 1);
            else { e.lda_imm(high); e.sta_abs(addr + 1); }
        }
        return;
    }

    if (SRC == ".AX" || SRC == ".AY" || SRC == ".AZ") {
        char reg2 = SRC[2];
        if (isStack) {
            e.sta_stack(offset);
            e.pha();
            if (reg2 == 'X') e.txa();
            else if (reg2 == 'Y') e.tya();
            else if (reg2 == 'Z') e.tza();
            e.sta_stack(offset + 1);
            e.pla();
        } else {
            std::string dest = tokens[tokenIndex].value;
            if (tokens[tokenIndex].type == AssemblerTokenType::REGISTER) dest = "." + dest;
            else if (!dest.empty() && dest[0] != '.' && (dest=="A"||dest=="X"||dest=="Y"||dest=="Z"||dest=="a"||dest=="x"||dest=="y"||dest=="z")) dest = "." + dest;
            Symbol* sym = resolveSymbol(dest, scopePrefix);
            uint32_t addr = sym ? sym->value : parseNumericLiteral(dest);
            e.sta_abs(addr);
            uint32_t addr2 = addr + 1;
            if (reg2 == 'X') e.stx_abs(addr2);
            else if (reg2 == 'Y') e.sty_abs(addr2);
            else if (reg2 == 'Z') e.stz_abs(addr2);
        }
    } else {
        throw std::runtime_error("Simulated STW currently only supports .AX, .AY, .AZ sources, found " + SRC);
    }
}

void AssemblerParser::emitSwapCode(std::vector<uint8_t>& binary, const std::string& r1, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    std::string r2 = tokens[tokenIndex].value;
    if (!r2.empty() && r2[0] != '.') r2 = "." + r2;
    std::transform(r2.begin(), r2.end(), r2.begin(), ::toupper);
    
    std::string R1 = r1;
    if (!R1.empty() && R1[0] != '.') R1 = "." + R1;
    std::transform(R1.begin(), R1.end(), R1.begin(), ::toupper);

    if (R1 == ".A" || r2 == ".A") {
        std::string other = (R1 == ".A") ? r2 : R1;
        if (other == ".X") { e.pha(); e.txa(); e.plx(); }
        else if (other == ".Y") { e.pha(); e.tya(); e.ply(); }
        else if (other == ".Z") { e.pha(); e.tza(); e.plz(); }
        else throw std::runtime_error("SWAP only supports A, X, Y, Z registers: found " + R1 + ", " + r2);
    } else {
        if ((R1 == ".X" && r2 == ".Y") || (R1 == ".Y" && r2 == ".X")) { e.phx(); e.tya(); e.tax(); e.ply(); }
        else if ((R1 == ".X" && r2 == ".Z") || (R1 == ".Z" && r2 == ".X")) { e.phx(); e.tza(); e.tax(); e.plz(); }
        else if ((R1 == ".Y" && r2 == ".Z") || (R1 == ".Z" && r2 == ".Y")) { e.phy(); e.tza(); e.tay(); e.plz(); }
        else throw std::runtime_error("SWAP only supports A, X, Y, Z registers: found " + R1 + ", " + r2);
    }
}

void AssemblerParser::emitZeroCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string&) {
    M65Emitter e(binary, getZPStart());
    int idx = tokenIndex;
    while (idx < tokens.size()) {
        std::string reg = tokens[idx].value;
        std::transform(reg.begin(), reg.end(), reg.begin(), ::toupper);
        if (reg == "A") e.cla();
        else if (reg == "X") e.clx();
        else if (reg == "Y") e.cly();
        else if (reg == "Z") e.clz();
        else throw std::runtime_error("ZERO only supports A, X, Y, Z registers: found " + reg);

        if (idx + 1 < tokens.size() && tokens[idx+1].type == AssemblerTokenType::COMMA) {
            idx += 2;
        } else {
            break;
        }
    }
}

void AssemblerParser::emitNegNot16Code(std::vector<uint8_t>& binary, bool isNeg, const std::string& operand, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    
    std::string OP = operand;
    if (!OP.empty() && OP[0] != '.') OP = "." + OP;
    std::transform(OP.begin(), OP.end(), OP.begin(), ::toupper);

    if (OP == ".AX" || OP == "") {
        if (isNeg) e.neg_16();
        else e.not_16();
        return;
    }

    uint32_t offset = 0;
    if (isStackRelativeOperand(tokenIndex, offset, scopePrefix)) {
        if (isNeg) {
            // NEG: Complement and Add 1
            e.tsx();
            e.lda_abs_x(0x0101 + offset);
            e.eor_imm(0xFF);
            e.clc();
            e.adc_imm(1);
            e.sta_abs_x(0x0101 + offset);
            e.lda_abs_x(0x0102 + offset);
            e.eor_imm(0xFF);
            e.adc_imm(0);
            e.sta_abs_x(0x0102 + offset);
        } else {
            // NOT: Just complement
            e.tsx();
            e.lda_abs_x(0x0101 + offset);
            e.eor_imm(0xFF);
            e.sta_abs_x(0x0101 + offset);
            e.lda_abs_x(0x0102 + offset);
            e.eor_imm(0xFF);
            e.sta_abs_x(0x0102 + offset);
        }
    } else {
        // Memory (ZP or Absolute)
        Symbol* sym = resolveSymbol(operand, scopePrefix);
        uint32_t addr = sym ? sym->value : parseNumericLiteral(operand);
        bool isZP = (addr < 0x100);

        if (isNeg) {
            if (isZP) {
                e.lda_zp(addr); e.eor_imm(0xFF); e.clc(); e.adc_imm(1); e.sta_zp(addr);
                e.lda_zp(addr + 1); e.eor_imm(0xFF); e.adc_imm(0); e.sta_zp(addr + 1);
            } else {
                e.lda_abs(addr); e.eor_imm(0xFF); e.clc(); e.adc_imm(1); e.sta_abs(addr);
                e.lda_abs(addr + 1); e.eor_imm(0xFF); e.adc_imm(0); e.sta_abs(addr + 1);
            }
        } else {
            if (isZP) {
                e.lda_zp(addr); e.eor_imm(0xFF); e.sta_zp(addr);
                e.lda_zp(addr + 1); e.eor_imm(0xFF); e.sta_zp(addr + 1);
            } else {
                e.lda_abs(addr); e.eor_imm(0xFF); e.sta_abs(addr);
                e.lda_abs(addr + 1); e.eor_imm(0xFF); e.sta_abs(addr + 1);
            }
        }
    }
}

void AssemblerParser::emitChkZeroCode(std::vector<uint8_t>& binary, bool is16, bool isInverse, int, const std::string&) {
    M65Emitter e(binary, getZPStart());
    if (is16) {
        // Optimized 16-bit zero check
        e.cmp_imm(0); // Check low byte (A)
        e.bne(0x03);  // If A != 0, it's non-zero, skip high check
        e.txa();      // Move high byte to A
        // Flags now reflect high byte. If both were zero, Z is set.
    } else {
        e.cmp_imm(0); // Standard 8-bit check
    }
    
    if (isInverse) {
        // Convert Z flag to boolean in A (True if non-zero)
        e.bne(0x03); 
        e.lda_imm(0); 
        e.bra(0x02);
        e.lda_imm(1);
    } else {
        // Convert Z flag to boolean in A (True if zero)
        e.beq(0x03);
        e.lda_imm(0);
        e.bra(0x02);
        e.lda_imm(1);
    }
}

void AssemblerParser::emitBranch16Code(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    int idx = tokenIndex;
    if (idx >= (int)tokens.size()) return;
    std::string condition = tokens[idx++].value;
    std::transform(condition.begin(), condition.end(), condition.begin(), ::toupper);
    
    if (idx < (int)tokens.size() && tokens[idx].type == AssemblerTokenType::COMMA) idx++;
    
    if (idx >= (int)tokens.size()) return;
    std::string target = tokens[idx].value;

    Symbol* sym = resolveSymbol(target, scopePrefix);
    // ... Simplified ...
    if (condition == "BEQ") e.beq(0x00);
    else if (condition == "BNE") e.bne(0x00);
}

void AssemblerParser::emitSelectCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    int idx = tokenIndex;
    if (idx >= (int)tokens.size()) return;
    std::string reg = tokens[idx++].value;
    
    if (idx < (int)tokens.size() && tokens[idx].type == AssemblerTokenType::COMMA) idx++;
    
    auto val1Ast = parseExprAST(tokens, idx, symbolTable, scopePrefix);
    if (idx < (int)tokens.size() && tokens[idx].type == AssemblerTokenType::COMMA) idx++;
    auto val2Ast = parseExprAST(tokens, idx, symbolTable, scopePrefix);

    // If flags are set, load val1, else val2
    e.bne(0x03); 
    e.lda_imm(val2Ast->getValue(this));
    e.bra(0x02);
    e.lda_imm(val1Ast->getValue(this));
}

void AssemblerParser::emitPtrStackCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    int idx = tokenIndex;
    uint32_t offset = evaluateExpressionAt(idx, scopePrefix);
    e.tsx();
    e.txa();
    e.clc();
    e.adc_imm(0x0101 + offset);
    e.pha();
    e.lda_imm(0);
    e.adc_imm(0);
    e.tax();
    e.pla();
}

void AssemblerParser::emitPtrDerefCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    int idx = tokenIndex;
    std::string src = tokens[idx].value;
    if (tokens[idx].type == AssemblerTokenType::REGISTER) src = "." + src;

    // Assume src is ZP for now
    Symbol* sym = resolveSymbol(src, scopePrefix);
    uint32_t addr = sym ? sym->value : parseNumericLiteral(src);

    e.ldy_imm(0);
    e.lda_ind_z(addr, false);
    e.pha();
    e.ldy_imm(1);
    e.lda_ind_z(addr, false);
    e.tax();
    e.pla();
}

void AssemblerParser::emitFillCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix, bool forceStack) {
    M65Emitter e(binary, getZPStart());
    int idx = tokenIndex;
    if (idx < 0 || idx >= (int)tokens.size()) return;

    std::string destReg;
    bool destIsRegister = false;
    bool destIsIndirect = false;
    uint32_t destVal = 0;
    bool destIsStack = forceStack;

    if (destIsStack) {
        if (tokens[idx].type == AssemblerTokenType::HASH) idx++;
        if (tokens[idx].type == AssemblerTokenType::REGISTER) {
            destReg = "." + tokens[idx++].value;
            destIsRegister = true;
        } else {
            auto ast = parseExprAST(tokens, idx, symbolTable, scopePrefix);
            if (ast) destVal = ast->getValue(this);
        }
    } else {
        if (tokens[idx].type == AssemblerTokenType::REGISTER) {
            destReg = "." + tokens[idx++].value;
            destIsRegister = true;
        } else if (tokens[idx].type == AssemblerTokenType::OPEN_PAREN) {
            idx++; // (
            destReg = tokens[idx++].value;
            if (tokens[idx-1].type == AssemblerTokenType::REGISTER) destReg = "." + destReg;
            destIsIndirect = true;
            if (idx < (int)tokens.size() && tokens[idx].type == AssemblerTokenType::CLOSE_PAREN) idx++;
        } else {
            uint32_t offset = 0;
            if (isStackRelativeOperand(idx, offset, scopePrefix)) {
                destIsStack = true;
                destVal = offset;
                // Skip expression and ", s"
                parseExprAST(tokens, idx, symbolTable, scopePrefix);
                if (idx < (int)tokens.size() && tokens[idx].type == AssemblerTokenType::COMMA) idx++;
                if (idx < (int)tokens.size() && (tokens[idx].value == "s" || tokens[idx].value == "S")) idx++;
            } else {
                auto ast = parseExprAST(tokens, idx, symbolTable, scopePrefix);
                if (ast) destVal = ast->getValue(this);
            }
        }
    }

    if (idx < (int)tokens.size() && tokens[idx].type == AssemblerTokenType::COMMA) idx++;

    std::string lenReg;
    bool lenIsRegister = false;
    uint32_t lenVal = 0;
    if (idx < (int)tokens.size()) {
        if (tokens[idx].type == AssemblerTokenType::HASH) idx++;
        if (tokens[idx].type == AssemblerTokenType::REGISTER) {
            lenReg = "." + tokens[idx++].value;
            lenIsRegister = true;
        } else {
            auto ast = parseExprAST(tokens, idx, symbolTable, scopePrefix);
            if (ast) lenVal = ast->getValue(this);
        }
    }

    // DMA Setup
    e.pha(); // S+1: Fill value
    e.phw_imm(0); // S+2,3: Next Job
    e.lda_imm(0); e.pha(); // S+4: Dest Bank

    if (destIsStack) {
        e.tsx(); e.txa(); e.clc();
        if (destIsRegister) {
            if (destReg == ".X") { /* already in X */ }
            else if (destReg == ".Y") { e.tya(); e.tax(); }
            else if (destReg == ".Z") { e.tza(); e.tax(); }
            e.txa();
        } else e.adc_imm(destVal);
        e.pha(); // S+6: Lo
        e.lda_imm(1); e.adc_imm(0); e.pha(); // S+5: Hi
    } else if (destIsRegister) {
        if (destReg == ".AX") { e.phx(); e.pha(); }
        else if (destReg == ".AY") { e.phy(); e.pha(); }
        else if (destReg == ".AZ") { e.phz(); e.pha(); }
        else if (destReg == ".XY") { e.phy(); e.phx(); }
        else throw std::runtime_error("Unsupported register combo for FILL: " + destReg);
    } else if (destIsIndirect) {
        Symbol* sym = resolveSymbol(destReg, scopePrefix);
        uint32_t addr = sym ? sym->value : parseNumericLiteral(destReg);
        e.lda_zp(addr + 1); e.pha(); // S+5: Hi
        e.lda_zp(addr); e.pha(); // S+6: Lo
    } else {
        e.lda_imm(destVal >> 8); e.pha(); // S+5: Hi
        e.lda_imm(destVal & 0xFF); e.pha(); // S+6: Lo
    }

    e.lda_imm(0); e.pha(); // S+7: Src Bank
    e.lda_imm(1); e.pha(); // S+8: Src Hi (Stack Page)
    e.tsx(); e.txa(); e.clc(); e.adc_imm(8); e.pha(); // S+9: Src Lo (X+8)

    if (lenIsRegister) {
        if (lenReg == ".XY") {
            e.phy(); // S+10: Len Hi (Y)
            e.phx(); // S+11: Len Lo (X)
        } else {
            e.lda_imm(0); e.pha(); // S+10: Len Hi
            if (lenReg == ".X") e.phx();
            else if (lenReg == ".Y") e.phy();
            else if (lenReg == ".Z") e.phz();
            else { 
                // Recover A from stack (it's at S+11 after the pushes so far)
                e.tsx(); e.txa(); e.clc(); e.adc_imm(11); e.tax();
                e.lda_abs_x(0x0100); e.pha(); 
            }
        }
    } else {
        e.lda_imm(lenVal >> 8); e.pha(); // S+10: Len Hi
        e.lda_imm(lenVal & 0xFF); e.pha(); // S+11: Len Lo
    }

    e.lda_imm(0x03); e.pha(); // S+12: Command (Fill)

    e.tsx(); e.txa(); e.clc(); e.adc_imm(1); e.sta_abs(0xD701);
    e.lda_imm(1); e.sta_abs(0xD702);
    e.stz_abs(0xD703); e.stz_abs(0xD700);

    e.tsx(); e.txa(); e.clc(); e.adc_imm(12); e.tax(); e.txs();
    e.pla(); 
}

void AssemblerParser::emitMoveCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix, bool forceStack) {
    M65Emitter e(binary, getZPStart());
    int idx = tokenIndex;
    if (idx < 0 || idx >= (int)tokens.size()) return;

    struct Operand {
        bool isAZ = false;
        bool isAbsolute = false;
        bool isStack = false;
        uint32_t value = 0;
    };

    auto parseMoveOp = [&](int& curIdx) -> Operand {
        Operand op;
        if (tokens[curIdx].type == AssemblerTokenType::REGISTER && tokens[curIdx].value == "AZ") {
            op.isAZ = true;
            curIdx++;
        } else {
            uint32_t offset = 0;
            if (isStackRelativeOperand(curIdx, offset, scopePrefix)) {
                op.isStack = true;
                op.value = offset;
                parseExprAST(tokens, curIdx, symbolTable, scopePrefix);
                if (curIdx < (int)tokens.size() && tokens[curIdx].type == AssemblerTokenType::COMMA) curIdx++;
                if (curIdx < (int)tokens.size() && (tokens[curIdx].value == "s" || tokens[curIdx].value == "S")) curIdx++;
            } else {
                auto ast = parseExprAST(tokens, curIdx, symbolTable, scopePrefix);
                if (ast) {
                    op.isAbsolute = true;
                    op.value = ast->getValue(this);
                }
            }
        }
        return op;
    };

    Operand src = parseMoveOp(idx);
    if (idx < (int)tokens.size() && tokens[idx].type == AssemblerTokenType::COMMA) idx++;
    Operand dest = parseMoveOp(idx);

    if (forceStack) {
        // In move.sp, if one operand wasn't explicitly stack-relative, it's the one that IS the offset.
        if (!src.isStack && !dest.isStack) {
            if (src.isAbsolute && !dest.isAbsolute && !dest.isAZ) src.isStack = true;
            else dest.isStack = true;
        }
    }

    // DMA Setup (Copy)
    e.pha(); // dummy for fill value (S+1)
    e.phw_imm(0); // Next Job (S+2,3)

    // Dest Bank (S+4)
    e.lda_imm(0); e.pha();

    // Dest Address (S+5,6)
    if (dest.isAZ) { e.phz(); e.pha(); }
    else if (dest.isStack) {
        e.tsx(); e.txa(); e.clc(); e.adc_imm(dest.value);
        e.pha(); e.lda_imm(1); e.adc_imm(0); e.pha();
    } else {
        e.lda_imm(dest.value >> 8); e.pha();
        e.lda_imm(dest.value & 0xFF); e.pha();
    }

    // Src Bank (S+7)
    e.lda_imm(0); e.pha();

    // Src Address (S+8,9)
    if (src.isAZ) { e.phz(); e.pha(); }
    else if (src.isStack) {
        e.tsx(); e.txa(); e.clc(); e.adc_imm(src.value);
        e.pha(); e.lda_imm(1); e.adc_imm(0); e.pha();
    } else {
        e.lda_imm(src.value >> 8); e.pha();
        e.lda_imm(src.value & 0xFF); e.pha();
    }

    // Length (S+10,11) - XY
    e.phy(); e.phx();

    // Command (S+12) - Copy is $00
    e.lda_imm(0x00); e.pha();

    e.tsx(); e.txa(); e.clc(); e.adc_imm(1); e.sta_abs(0xD701);
    e.lda_imm(1); e.sta_abs(0xD702);
    e.stz_abs(0xD703); e.stz_abs(0xD700);

    e.tsx(); e.txa(); e.clc(); e.adc_imm(12); e.tax(); e.txs();
    e.pla();
}
void AssemblerParser::emitFlatMemoryCode(std::vector<uint8_t>& binary, const std::string& mnemonic, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    int idx = tokenIndex;
    std::string op = tokens[idx].value;
    Symbol* sym = resolveSymbol(op, scopePrefix);
    uint32_t addr = sym ? sym->value : parseNumericLiteral(op);

    // EOM prefix for linear 28-bit access
    e.eom();
    if (mnemonic == "LDW.F") {
        e.lda_abs(addr);
        e.eom();
        e.ldx_abs(addr + 1);
    } else if (mnemonic == "STW.F") {
        e.sta_abs(addr);
        e.eom();
        e.stx_abs(addr + 1);
    } else if (mnemonic == "INC.F") {
        e.inc_abs(addr);
    } else if (mnemonic == "DEC.F") {
        e.dec_abs(addr);
    }
}

void AssemblerParser::emitPHWStackCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    int idx = tokenIndex;
    uint32_t offset = evaluateExpressionAt(idx, scopePrefix);
    
    // PHW pushes high byte then low byte.
    // To push a word from the stack at 'offset, s':
    // 1. Load high byte from offset+1
    // 2. Push it
    // 3. Load low byte from offset+1 (because SP moved!)
    // Better: use TSX
    e.tsx();
    e.lda_abs_x(0x0102 + offset); // High byte (original offset+1)
    e.pha();
    e.lda_abs_x(0x0102 + offset); // Low byte (original offset+0, but SP moved so it's still at same X-relative)
    // Wait, let's trace:
    // S was $FE. X = $FE. High byte is at $01FE. Low byte is at $01FD.
    // LDA $0101+1, X -> LDA $0102, $FE -> LDA $0200. WRONG.
    // Trace again:
    // If offset is 0, we want to push the word at S+1, S+2.
    // High byte is at S+2. Low byte is at S+1.
    // TSX -> X = S.
    // LDA $0102, X -> High byte. Correct.
    // PHA -> S = S-1. X is still original S.
    // Low byte is at original S+1.
    // LDA $0101, X -> Low byte.
    // So:
    e.lda_abs_x(0x0102 + offset);
    e.pha();
    e.lda_abs_x(0x0101 + offset);
    e.pha();
}

void AssemblerParser::pass1() {

    pc = 0;
 pos = 0;
 statements.clear();
 scopeStack.clear();
 nextScopeId = 0;
 procedures.clear();
 currentProc = nullptr;

    std::vector<std::shared_ptr<ProcContext>> pass1ProcStack;

    auto currentScopePrefix = [&]() {
 std::string p = "";
 for (const auto& s : scopeStack) p += s + ":";
 return p;
 
}
;

    while (pos < tokens.size()) {
        // ... (rest of pass1 remains same until the convergence loop)


        while (match(AssemblerTokenType::NEWLINE));
 if (peek().type == AssemblerTokenType::END_OF_FILE) break;

        auto stmt = std::make_unique<Statement>();
 stmt->address = pc;
 stmt->line = peek().line;
 stmt->scopePrefix = currentScopePrefix();

        if (peek().type == AssemblerTokenType::IDENTIFIER && pos + 1 < tokens.size() && tokens[pos+1].type == AssemblerTokenType::COLON) {
 stmt->label = stmt->scopePrefix + advance().value;
 advance();
 symbolTable[stmt->label] = {pc, true, 2, false, 0, false, 0};
 
}

        if (peek().type == AssemblerTokenType::IDENTIFIER && pos + 1 < tokens.size() && tokens[pos+1].type == AssemblerTokenType::EQUALS) {
            std::string name = advance().value;
            advance(); // =
            uint32_t val = evaluateExpressionAt((int)pos, stmt->scopePrefix);
            symbolTable[stmt->scopePrefix + name] = {val, false, 2, false, 0, false, 0};
            while (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) advance();
            continue;
        }

        if (match(AssemblerTokenType::OPEN_CURLY)) {
 scopeStack.push_back("_S" + std::to_string(nextScopeId++));
 stmt->size = 0;
 statements.push_back(std::move(stmt));
 continue;
 
}

        else if (match(AssemblerTokenType::CLOSE_CURLY)) {
 if (!scopeStack.empty()) scopeStack.pop_back();
 stmt->size = 0;
 statements.push_back(std::move(stmt));
 continue;
 
}

        else if (match(AssemblerTokenType::DIRECTIVE)) {

            stmt->type = Statement::DIRECTIVE;
 stmt->dir.name = tokens[pos-1].value;

            if (stmt->dir.name == "var") {

                std::string vN = expect(AssemblerTokenType::IDENTIFIER, "Expected var name").value;
 std::string scVN = stmt->scopePrefix + vN;
 stmt->dir.varName = scVN;

                if (match(AssemblerTokenType::EQUALS)) {
 stmt->dir.varType = Directive::ASSIGN;
 stmt->dir.tokenIndex = (int)pos;
 uint32_t val = evaluateExpressionAt((int)pos, stmt->scopePrefix);
 while (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) advance();
 symbolTable[scVN] = {val, false, 2, true, val};
 
}

                else if (match(AssemblerTokenType::INCREMENT)) {
 stmt->dir.varType = Directive::INC;
 if (symbolTable.count(scVN)) symbolTable[scVN].value++;
 
}

                else if (match(AssemblerTokenType::DECREMENT)) {
 stmt->dir.varType = Directive::DEC;
 if (symbolTable.count(scVN)) symbolTable[scVN].value--;
 
}

                stmt->size = 0;

            
}
 else if (stmt->dir.name == "cleanup") {
 stmt->dir.tokenIndex = (int)pos;
 uint32_t val = evaluateExpressionAt((int)pos, stmt->scopePrefix);
 while (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) advance();
 if (currentProc) currentProc->totalParamSize += val;
 stmt->size = 0;
 
}

            else if (stmt->dir.name == "basicUpstart") {
 stmt->type = Statement::BASIC_UPSTART;
 stmt->basicUpstartTokenIndex = (int)pos;
 while (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) advance();
 stmt->size = 12;
 
}

            else {
 while (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) {
 if (peek().type == AssemblerTokenType::COMMA) {
 advance();
 continue;
 
}
 stmt->dir.arguments.push_back(advance().value);
 
}
 if (stmt->dir.name == "org") {
 stmt->address = pc;
 if (!stmt->dir.arguments.empty()) pc = parseNumericLiteral(stmt->dir.arguments[0]);
 stmt->size = 0;
 
}
 else if (stmt->dir.name == "cpu") stmt->size = 0;
 else stmt->size = calculateDirectiveSize(stmt->dir);
 
}

        
}
 else if (peek().type == AssemblerTokenType::STAR && pos + 1 < tokens.size() && tokens[pos+1].type == AssemblerTokenType::EQUALS) {
 advance();
 advance();
 stmt->type = Statement::DIRECTIVE;
 stmt->dir.name = "org";
 stmt->address = pc;
 while (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) {
 if (peek().type == AssemblerTokenType::COMMA) {
 advance();
 continue;
 
}
 stmt->dir.arguments.push_back(advance().value);
 
}
 if (!stmt->dir.arguments.empty()) pc = parseNumericLiteral(stmt->dir.arguments[0]);
 stmt->size = 0;
 
}

        else if (match(AssemblerTokenType::INSTRUCTION)) {

            stmt->type = Statement::INSTRUCTION;
 stmt->instr.mnemonic = tokens[pos-1].value;
 std::transform(stmt->instr.mnemonic.begin(), stmt->instr.mnemonic.end(), stmt->instr.mnemonic.begin(), ::toupper);

            if (stmt->instr.mnemonic == "PROC") {

                std::string pN = advance().value;

                stmt->label = pN;

                symbolTable[pN] = {pc, true, 2, false, 0, false, 0};

                auto ctx = std::make_shared<ProcContext>();

                ctx->name = pN;

                ctx->totalParamSize = 0;

                std::vector<std::pair<std::string, int>> args;

                while (match(AssemblerTokenType::COMMA)) {

                    bool isB = false;

                    if (peek().type == AssemblerTokenType::IDENTIFIER && (peek().value == "B" || peek().value == "W")) {

                        isB = (advance().value == "B");

                        match(AssemblerTokenType::HASH);

                    
}

                    std::string aN = advance().value;

                    int sz = isB ? 1 : 2;

                    args.push_back({
aN, sz
}
);

                    ctx->totalParamSize += sz;

                
}

                scopeStack.push_back(pN);

                stmt->scopePrefix = currentScopePrefix();

                int cO = 2;

                for (int i = (int)args.size() - 1;
 i >= 0;
 --i) {

                    std::string scA = stmt->scopePrefix + args[i].first;

                    std::string scAN = stmt->scopePrefix + "ARG" + std::to_string(i + 1);

                    ctx->localArgs[args[i].first] = cO;

                    ctx->localArgs["ARG" + std::to_string(i + 1)] = cO;

                                        symbolTable[scA] = {(uint32_t)cO, false, (int)args[i].second, true, (uint32_t)cO, false, cO};
                    symbolTable[scAN] = {(uint32_t)cO, false, (int)args[i].second, true, (uint32_t)cO, false, cO};

                    cO += args[i].second;

                
}

                procedures[pc] = ctx;

                pass1ProcStack.push_back(currentProc);

                currentProc = ctx;

                stmt->procCtx = ctx;

                stmt->size = 0;

            
}
 else if (stmt->instr.mnemonic == "ENDPROC") {

                if (currentProc) {

                    stmt->instr.procParamSize = currentProc->totalParamSize;

                    currentProc = pass1ProcStack.empty() ? nullptr : pass1ProcStack.back();

                    if (!pass1ProcStack.empty()) pass1ProcStack.pop_back();

                
}

                if (!scopeStack.empty()) scopeStack.pop_back();

                stmt->size = 2;

            
}

            else if (stmt->instr.mnemonic == "CALL") {
 stmt->instr.operand = advance().value;
 while (match(AssemblerTokenType::COMMA)) {
 if (match(AssemblerTokenType::HASH)) stmt->instr.callArgs.push_back(std::string("#") + advance().value);
 else if (peek().type == AssemblerTokenType::IDENTIFIER && (peek().value == "B" || peek().value == "W")) {
 std::string p = advance().value;
 match(AssemblerTokenType::HASH);
 stmt->instr.callArgs.push_back(p + "#" + advance().value);
 
}
 else stmt->instr.callArgs.push_back(advance().value);
 
}
 stmt->size = calculateInstructionSize(stmt->instr, pc, stmt->scopePrefix);
 
}

            else if (stmt->instr.mnemonic == "EXPR") {
 stmt->type = Statement::EXPR;
 const auto& trg = advance();
 stmt->exprTarget = (trg.type == AssemblerTokenType::REGISTER ? "." : "") + trg.value;
 expect(AssemblerTokenType::COMMA, "Expected ,");
 stmt->exprTokenIndex = (int)pos;
 while (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) advance();
 std::vector<uint8_t> d;
 emitExpressionCode(d, stmt->exprTarget, stmt->exprTokenIndex, stmt->scopePrefix);
 stmt->size = d.size();
 
}

            else if (stmt->instr.mnemonic.substr(0, 3) == "MUL" || stmt->instr.mnemonic.substr(0, 3) == "DIV") {
                stmt->type = (stmt->instr.mnemonic.substr(0, 3) == "MUL") ? Statement::MUL : Statement::DIV;
                std::string m = stmt->instr.mnemonic;
                if (m.size() > 4 && m[3] == '.') stmt->mulWidth = std::stoi(m.substr(4));
                else stmt->mulWidth = 8;
                const auto& dst = advance();
                stmt->instr.operand = (dst.type == AssemblerTokenType::REGISTER ? "." : "") + dst.value;
                expect(AssemblerTokenType::COMMA, "Expected , after destination");
                stmt->exprTokenIndex = (int)pos;
                while (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) advance();
                std::vector<uint8_t> d;
                if (stmt->type == Statement::MUL) emitMulCode(d, stmt->mulWidth, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix);
                else emitDivCode(d, stmt->mulWidth, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix);
                stmt->size = d.size();
            }
            else if (stmt->instr.mnemonic == "LDAX" || stmt->instr.mnemonic == "LDAY" || stmt->instr.mnemonic == "LDAZ" ||
                     stmt->instr.mnemonic == "STAX" || stmt->instr.mnemonic == "STAY" || stmt->instr.mnemonic == "STAZ") {
                std::string m = stmt->instr.mnemonic;
                std::string reg = "." + m.substr(2); // AX, AY, AZ
                bool isLoad = (m.substr(0, 2) == "LD");
                stmt->type = isLoad ? Statement::LDW : Statement::STW;
                stmt->instr.operand = reg;
                stmt->exprTokenIndex = (int)pos;
                while (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) advance();
                
                std::vector<uint8_t> d;
                if (isLoad) emitLDWCode(d, reg, stmt->exprTokenIndex, stmt->scopePrefix);
                else emitSTWCode(d, reg, stmt->exprTokenIndex, stmt->scopePrefix);
                stmt->size = d.size();
            }
            else if (stmt->instr.mnemonic == "ADD.16" || stmt->instr.mnemonic == "SUB.16" || 
                     stmt->instr.mnemonic == "AND.16" || stmt->instr.mnemonic == "ORA.16" || stmt->instr.mnemonic == "EOR.16" || 
                     stmt->instr.mnemonic == "CPW" || stmt->instr.mnemonic == "LDW" || stmt->instr.mnemonic == "STW" || 
                     stmt->instr.mnemonic == "SWAP" || stmt->instr.mnemonic == "NEG.16" || stmt->instr.mnemonic == "NOT.16" ||
                     stmt->instr.mnemonic == "CHKZERO.8" || stmt->instr.mnemonic == "CHKZERO.16" ||
                     stmt->instr.mnemonic == "CHKNONZERO.8" || stmt->instr.mnemonic == "CHKNONZERO.16" ||
                     stmt->instr.mnemonic == "BRANCH.16" || stmt->instr.mnemonic == "SELECT" ||
                     stmt->instr.mnemonic == "PTRSTACK" || stmt->instr.mnemonic == "PTRDEREF" ||
                     stmt->instr.mnemonic == "LDW.F" || stmt->instr.mnemonic == "STW.F" ||
                     stmt->instr.mnemonic == "STW.SP" || stmt->instr.mnemonic == "LDW.SP" ||
                     stmt->instr.mnemonic == "FILL" || stmt->instr.mnemonic == "FILL.SP" ||
                     stmt->instr.mnemonic == "MOVE" || stmt->instr.mnemonic == "MOVE.SP" ||
                     stmt->instr.mnemonic == "INC.F" || stmt->instr.mnemonic == "DEC.F" ||
                     stmt->instr.mnemonic == "PHW") {
                
                if (stmt->instr.mnemonic == "ADD.16") stmt->type = Statement::ADD16;
                else if (stmt->instr.mnemonic == "SUB.16") stmt->type = Statement::SUB16;
                else if (stmt->instr.mnemonic == "AND.16") stmt->type = Statement::AND16;
                else if (stmt->instr.mnemonic == "ORA.16") stmt->type = Statement::ORA16;
                else if (stmt->instr.mnemonic == "EOR.16") stmt->type = Statement::EOR16;
                else if (stmt->instr.mnemonic == "CPW") stmt->type = Statement::CPW;
                else if (stmt->instr.mnemonic == "LDW" || stmt->instr.mnemonic == "LDW.SP") stmt->type = Statement::LDW;
                else if (stmt->instr.mnemonic == "STW" || stmt->instr.mnemonic == "STW.SP") stmt->type = Statement::STW;
                else if (stmt->instr.mnemonic == "FILL" || stmt->instr.mnemonic == "FILL.SP") stmt->type = Statement::FILL;
                else if (stmt->instr.mnemonic == "MOVE" || stmt->instr.mnemonic == "MOVE.SP") stmt->type = Statement::COPY;
                else if (stmt->instr.mnemonic == "SWAP") stmt->type = Statement::SWAP;
                else if (stmt->instr.mnemonic == "NEG.16") stmt->type = Statement::NEG16;
                else if (stmt->instr.mnemonic == "NOT.16") stmt->type = Statement::NOT16;
                else if (stmt->instr.mnemonic == "CHKZERO.8") stmt->type = Statement::CHKZERO8;
                else if (stmt->instr.mnemonic == "CHKZERO.16") stmt->type = Statement::CHKZERO16;
                else if (stmt->instr.mnemonic == "CHKNONZERO.8") stmt->type = Statement::CHKNONZERO8;
                else if (stmt->instr.mnemonic == "CHKNONZERO.16") stmt->type = Statement::CHKNONZERO16;
                else if (stmt->instr.mnemonic == "BRANCH.16") stmt->type = Statement::BRANCH16;
                else if (stmt->instr.mnemonic == "SELECT") stmt->type = Statement::SELECT;
                else if (stmt->instr.mnemonic == "PTRSTACK") stmt->type = Statement::PTRSTACK;
                else if (stmt->instr.mnemonic == "PTRDEREF") stmt->type = Statement::PTRDEREF;
                else if (stmt->instr.mnemonic == "LDW.F") stmt->type = Statement::LDWF;
                else if (stmt->instr.mnemonic == "STW.F") stmt->type = Statement::STWF;
                else if (stmt->instr.mnemonic == "INC.F") stmt->type = Statement::INCF;
                else if (stmt->instr.mnemonic == "DEC.F") stmt->type = Statement::DECF;
                else if (stmt->instr.mnemonic == "PHW") stmt->type = Statement::PHW_STACK;

                stmt->instr.operandTokenIndex = (int)pos;

                // Handle instructions with 1 or more operands
                if (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) {
                    if (peek().type == AssemblerTokenType::REGISTER) {
                        stmt->instr.operand = "." + advance().value;
                    } else if (peek().type == AssemblerTokenType::HASH) {
                        stmt->instr.operand = "#";
                        advance();
                        int idx = (int)pos;
                        auto ast = parseExprAST(tokens, idx, symbolTable, stmt->scopePrefix);
                        pos = idx;
                    } else {
                        // Expression or label
                        int idx = (int)pos;
                        auto ast = parseExprAST(tokens, idx, symbolTable, stmt->scopePrefix);
                        if (ast) {
                             // We don't have a good way to put the whole expr into stmt->instr.operand
                             // So we just use a placeholder if it's not a simple identifier
                             if (auto* v = dynamic_cast<VariableNode*>(ast.get())) stmt->instr.operand = v->name;
                             else stmt->instr.operand = "EXPR";
                        }
                        pos = idx;
                    }
                    
                    if (match(AssemblerTokenType::COMMA)) {
                        stmt->exprTokenIndex = (int)pos;
                        // Skip rest of line
                        while (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) advance();
                    }
                }

                // Check if PHW is actually stack relative
                if (stmt->instr.mnemonic == "PHW") {
                    uint32_t offset = 0;
                    if (isStackRelativeOperand(stmt->instr.operandTokenIndex, offset, stmt->scopePrefix)) {
                        stmt->type = Statement::PHW_STACK;
                    } else {
                        stmt->type = Statement::INSTRUCTION; // Revert to standard
                    }
                }
                
                std::vector<uint8_t> d;
                if (stmt->type == Statement::ADD16 || stmt->type == Statement::SUB16) emitAddSub16Code(d, stmt->type == Statement::ADD16, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::AND16 || stmt->type == Statement::ORA16 || stmt->type == Statement::EOR16) emitBitwise16Code(d, stmt->instr.mnemonic, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::CPW) emitCPWCode(d, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::LDW) emitLDWCode(d, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix, stmt->instr.mnemonic == "LDW.SP");
                else if (stmt->type == Statement::STW) emitSTWCode(d, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix, stmt->instr.mnemonic == "STW.SP");
                else if (stmt->type == Statement::FILL) emitFillCode(d, stmt->instr.operandTokenIndex, stmt->scopePrefix, stmt->instr.mnemonic == "FILL.SP");
                else if (stmt->type == Statement::COPY) emitMoveCode(d, stmt->instr.operandTokenIndex, stmt->scopePrefix, stmt->instr.mnemonic == "MOVE.SP");
                else if (stmt->type == Statement::SWAP) emitSwapCode(d, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::NEG16 || stmt->type == Statement::NOT16) emitNegNot16Code(d, stmt->type == Statement::NEG16, stmt->instr.operand, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::CHKZERO8) emitChkZeroCode(d, false, false, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::CHKZERO16) emitChkZeroCode(d, true, false, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::CHKNONZERO8) emitChkZeroCode(d, false, true, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::CHKNONZERO16) emitChkZeroCode(d, true, true, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::BRANCH16) emitBranch16Code(d, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::SELECT) emitSelectCode(d, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::PTRSTACK) emitPtrStackCode(d, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::PTRDEREF) emitPtrDerefCode(d, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::LDWF || stmt->type == Statement::STWF || stmt->type == Statement::INCF || stmt->type == Statement::DECF) emitFlatMemoryCode(d, stmt->instr.mnemonic, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::PHW_STACK) emitPHWStackCode(d, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                
                if (stmt->type != Statement::INSTRUCTION) stmt->size = d.size();
                else {
                     // Re-parse as instruction
                     if (stmt->instr.mnemonic == "PHW") {
                         stmt->size = 3;
                         if (tokens[stmt->instr.operandTokenIndex].type == AssemblerTokenType::HASH) {
                             stmt->instr.mode = AddressingMode::IMMEDIATE16;
                             stmt->instr.operandTokenIndex++; // Skip HASH for expression evaluation
                         }
                         else stmt->instr.mode = AddressingMode::ABSOLUTE;
                     }
                }
            }
            else if (stmt->instr.mnemonic == "ZERO") {
                stmt->type = Statement::ZERO;
                stmt->instr.operandTokenIndex = (int)pos;
                while (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) advance();
                std::vector<uint8_t> d;
                emitZeroCode(d, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                stmt->size = d.size();
            }

            else {

                if (match(AssemblerTokenType::HASH)) {
 stmt->instr.operandTokenIndex = (int)pos;
 std::string o;
 while (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE && peek().type != AssemblerTokenType::COMMA) o += advance().value;
 stmt->instr.operand = o;
 if (stmt->instr.mnemonic == "PHW") stmt->instr.mode = AddressingMode::IMMEDIATE16;
 else stmt->instr.mode = AddressingMode::IMMEDIATE;
 
}

                else if (match(AssemblerTokenType::OPEN_PAREN)) {
 stmt->instr.operandTokenIndex = (int)pos;
 std::string o;
 while (peek().type != AssemblerTokenType::CLOSE_PAREN && peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE && peek().type != AssemblerTokenType::COMMA) o += advance().value;
 stmt->instr.operand = o;

                    if (match(AssemblerTokenType::COMMA)) {
 std::string r = advance().value;
 if (r == "X" || r == "x") {
 expect(AssemblerTokenType::CLOSE_PAREN, "Expected )");
 try {
 uint32_t val = parseNumericLiteral(stmt->instr.operand);
 if (val > 0xFF || stmt->instr.mnemonic == "JSR" || stmt->instr.mnemonic == "JMP") stmt->instr.mode = AddressingMode::ABSOLUTE_X_INDIRECT;
 else stmt->instr.mode = AddressingMode::BASE_PAGE_X_INDIRECT;
 
}
 catch(...) {
 stmt->instr.mode = AddressingMode::ABSOLUTE_X_INDIRECT;
 
}
 
}
 else if (r == "SP" || r == "sp") {
 expect(AssemblerTokenType::CLOSE_PAREN, "Expected )");
 expect(AssemblerTokenType::COMMA, "Expected ,");
 advance();
 stmt->instr.mode = AddressingMode::STACK_RELATIVE;
 
}
 
}

                    else {
 expect(AssemblerTokenType::CLOSE_PAREN, "Expected )");
 if (match(AssemblerTokenType::COMMA)) {
 std::string r = advance().value;
 if (r == "Y" || r == "y") stmt->instr.mode = AddressingMode::BASE_PAGE_INDIRECT_Y;
 else if (r == "Z" || r == "z") stmt->instr.mode = AddressingMode::BASE_PAGE_INDIRECT_Z;
 
}
 else {
 try {
 uint32_t val = parseNumericLiteral(stmt->instr.operand);
 if (val > 0xFF || stmt->instr.mnemonic == "JSR" || stmt->instr.mnemonic == "JMP") stmt->instr.mode = AddressingMode::ABSOLUTE_INDIRECT;
 else stmt->instr.mode = AddressingMode::INDIRECT;
 
}
 catch(...) {
 stmt->instr.mode = AddressingMode::ABSOLUTE_INDIRECT;
 
}
 
}
 
}
 
}

                else if (match(AssemblerTokenType::OPEN_BRACKET)) {
 stmt->instr.operandTokenIndex = (int)pos;
 std::string o;
 while (peek().type != AssemblerTokenType::CLOSE_BRACKET && peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) o += advance().value;
 stmt->instr.operand = o;
 expect(AssemblerTokenType::CLOSE_BRACKET, "Expected ]");
 expect(AssemblerTokenType::COMMA, "Expected ,");
 advance();
 stmt->instr.mode = AddressingMode::FLAT_INDIRECT_Z;
 
}

                else if ((peek().type == AssemblerTokenType::REGISTER || peek().type == AssemblerTokenType::IDENTIFIER) && (peek().value == "A" || peek().value == "a") && (pos + 1 >= tokens.size() || tokens[pos+1].type == AssemblerTokenType::NEWLINE || tokens[pos+1].type == AssemblerTokenType::END_OF_FILE)) {
 advance();
 stmt->instr.mode = AddressingMode::ACCUMULATOR;
 
}

                else if (peek().type == AssemblerTokenType::IDENTIFIER || peek().type == AssemblerTokenType::HEX_LITERAL || peek().type == AssemblerTokenType::DECIMAL_LITERAL || peek().type == AssemblerTokenType::BINARY_LITERAL || peek().type == AssemblerTokenType::LESS_THAN || peek().type == AssemblerTokenType::GREATER_THAN) {

                    AssemblerTokenType fT = peek().type;
 stmt->instr.operandTokenIndex = (int)pos;
 std::string o;
 while (peek().type != AssemblerTokenType::COMMA && peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) o += advance().value;
 stmt->instr.operand = o;

                    if (peek().type == AssemblerTokenType::COMMA) {
 advance();
 std::string r = advance().value;
 if (r == "X" || r == "x") stmt->instr.mode = AddressingMode::ABSOLUTE_X;
 else if (r == "Y" || r == "y") stmt->instr.mode = AddressingMode::ABSOLUTE_Y;
 else if (r == "S" || r == "s" || r == "SP" || r == "sp") stmt->instr.mode = AddressingMode::STACK_RELATIVE;
 else {
 stmt->instr.bitBranchTarget = r;
 stmt->instr.mode = AddressingMode::BASE_PAGE_RELATIVE;
 
}
 
}

                    else {
 if (fT == AssemblerTokenType::IDENTIFIER && o.find_first_of("+-*/") == std::string::npos) stmt->instr.mode = AddressingMode::ABSOLUTE;
 else {
 try {
 uint32_t val = parseNumericLiteral(o);
 if (val > 0xFF || stmt->instr.mnemonic == "JSR" || stmt->instr.mnemonic == "JMP") stmt->instr.mode = AddressingMode::ABSOLUTE;
 else stmt->instr.mode = AddressingMode::BASE_PAGE;
 
}
 catch(...) {
 stmt->instr.mode = AddressingMode::ABSOLUTE;
 
}
 
}
 
}

                
}

                if ((stmt->instr.mnemonic == "INW" || stmt->instr.mnemonic == "DEW") && stmt->instr.mode == AddressingMode::STACK_RELATIVE) {
                    stmt->type = (stmt->instr.mnemonic == "INW") ? Statement::STACK_INC : Statement::STACK_DEC;
                    stmt->size = (stmt->type == Statement::STACK_INC) ? 9 : 12;
                } else if ((stmt->instr.mnemonic == "INC" || stmt->instr.mnemonic == "DEC") && stmt->instr.mode == AddressingMode::STACK_RELATIVE) {
                    stmt->type = (stmt->instr.mnemonic == "INC") ? Statement::STACK_INC8 : Statement::STACK_DEC8;
                    stmt->size = 5; // TSX (1) + INC $0101+offset,X (4)
                } else {
                    stmt->size = calculateInstructionSize(stmt->instr, pc, stmt->scopePrefix);
                }

            
}

        
}
 else if (stmt->label.empty()) {
 advance();
 continue;
 
}

        pc += stmt->size;
 statements.push_back(std::move(stmt));

    
}

    optimize();
    statements.erase(std::remove_if(statements.begin(), statements.end(), [](const auto& s) { return s->deleted; }), statements.end());

    bool changed = true;
 while (changed) {
 changed = false;
 uint32_t cP = 0;
 for (auto& s : statements) {
 s->address = cP;
 if (!s->label.empty()) {
 if (symbolTable[s->label].value != cP) {
 symbolTable[s->label].value = cP;
 changed = true;
 
}
 
}
 if (s->type == Statement::DIRECTIVE && s->dir.name == "org") {
     if (!s->dir.arguments.empty()) cP = parseNumericLiteral(s->dir.arguments[0]);
 }
 if (s->type == Statement::INSTRUCTION) {
 int oS = s->size;
 s->size = calculateInstructionSize(s->instr, cP, s->scopePrefix);
 if (s->size != oS) changed = true;
 
}
 else if (s->type == Statement::EXPR) {
 int oS = s->size;
 std::vector<uint8_t> d;
 emitExpressionCode(d, s->exprTarget, s->exprTokenIndex, s->scopePrefix);
 s->size = d.size();
 if (s->size != oS) changed = true;
 
}
 cP += s->size;
 
}
 
}


}


int AssemblerParser::calculateInstructionSize(const Instruction& instr, uint32_t currentAddr, const std::string& scopePrefix) {
    if (instr.mnemonic == "PROC") return 0;
    if (instr.mnemonic == "ENDPROC") return 2;

    AddressingMode resolvedMode = instr.mode;
    
    // Auto-promote BASE_PAGE to ABSOLUTE if it doesn't fit in 8 bits.
    // Or promote identifiers if they don't have a fixed mode.
    if (instr.operandTokenIndex != -1 && (instr.mode == AddressingMode::BASE_PAGE || instr.mode == AddressingMode::ABSOLUTE || 
        instr.mode == AddressingMode::BASE_PAGE_X || instr.mode == AddressingMode::ABSOLUTE_X ||
        instr.mode == AddressingMode::BASE_PAGE_Y || instr.mode == AddressingMode::ABSOLUTE_Y)) {
        
        try {
            uint32_t val = evaluateExpressionAt(instr.operandTokenIndex, scopePrefix);
            bool fitsIn8 = (val <= 0xFF);
            bool forceAbs = (instr.mnemonic == "JSR" || instr.mnemonic == "JMP");

            if (instr.mode == AddressingMode::BASE_PAGE || instr.mode == AddressingMode::ABSOLUTE) {
                resolvedMode = (fitsIn8 && !forceAbs) ? AddressingMode::BASE_PAGE : AddressingMode::ABSOLUTE;
            } else if (instr.mode == AddressingMode::BASE_PAGE_X || instr.mode == AddressingMode::ABSOLUTE_X) {
                resolvedMode = (fitsIn8 && !forceAbs) ? AddressingMode::BASE_PAGE_X : AddressingMode::ABSOLUTE_X;
            } else if (instr.mode == AddressingMode::BASE_PAGE_Y || instr.mode == AddressingMode::ABSOLUTE_Y) {
                resolvedMode = (fitsIn8 && !forceAbs) ? AddressingMode::BASE_PAGE_Y : AddressingMode::ABSOLUTE_Y;
            }
        } catch(...) {
            // If we can't evaluate yet, assume ABSOLUTE to be safe
            if (instr.mode == AddressingMode::BASE_PAGE) resolvedMode = AddressingMode::ABSOLUTE;
            else if (instr.mode == AddressingMode::BASE_PAGE_X) resolvedMode = AddressingMode::ABSOLUTE_X;
            else if (instr.mode == AddressingMode::BASE_PAGE_Y) resolvedMode = AddressingMode::ABSOLUTE_Y;
        }
    }

    int size = 1;
    bool isQuad = (instr.mnemonic.size() > 1 && instr.mnemonic.back() == 'Q' && instr.mnemonic != "LDQ" && instr.mnemonic != "STQ" && instr.mnemonic != "BEQ" && instr.mnemonic != "BNE" && instr.mnemonic != "BRA" && instr.mnemonic != "BSR");

    if (isQuad) size += 2;
    if (resolvedMode == AddressingMode::FLAT_INDIRECT_Z) size += 1;

    if (instr.mnemonic == "BEQ" || instr.mnemonic == "BNE" || instr.mnemonic == "BRA" || instr.mnemonic == "BCC" || instr.mnemonic == "BCS" || instr.mnemonic == "BPL" || instr.mnemonic == "BMI" || instr.mnemonic == "BVC" || instr.mnemonic == "BVS") {
        Symbol* sym = resolveSymbol(instr.operand, scopePrefix);
        if (sym) {
            uint32_t target = sym->value;
            int32_t diff = (int32_t)target - (int32_t)(currentAddr + 2);
            if (diff >= -128 && diff <= 127) return 2;
        }
        return 3;
    }

    if (instr.mnemonic == "BSR") return 3;

    if (instr.mnemonic == "CALL") {
        int s = 0;
        for (const auto& arg : instr.callArgs) {
            bool isB = (arg.size() >= 2 && arg.substr(0, 2) == "B#");
            if (isB) s += 2;
            else {
                Symbol* sym = resolveSymbol(arg[0] == '#' ? arg.substr(1) : arg, scopePrefix);
                if (sym && sym->size == 1) s += 2;
                else s += 3;
            }
        }
        return s + 3;
    }

    if (instr.mnemonic == "RTN") return 2;

    switch (resolvedMode) {
        case AddressingMode::IMPLIED: case AddressingMode::ACCUMULATOR: return size;
        case AddressingMode::IMMEDIATE: case AddressingMode::BASE_PAGE: case AddressingMode::BASE_PAGE_X: case AddressingMode::BASE_PAGE_Y: case AddressingMode::INDIRECT: case AddressingMode::BASE_PAGE_X_INDIRECT: case AddressingMode::BASE_PAGE_INDIRECT_Y: case AddressingMode::BASE_PAGE_INDIRECT_Z: case AddressingMode::STACK_RELATIVE: return size + 1;
        case AddressingMode::ABSOLUTE: case AddressingMode::ABSOLUTE_X: case AddressingMode::ABSOLUTE_Y: case AddressingMode::ABSOLUTE_INDIRECT: case AddressingMode::ABSOLUTE_X_INDIRECT: case AddressingMode::IMMEDIATE16: return size + 2;
        case AddressingMode::BASE_PAGE_RELATIVE: return size + 2;
        case AddressingMode::FLAT_INDIRECT_Z: return size + 1;
        default: return size + 2;
    }
}


int AssemblerParser::calculateDirectiveSize(const Directive& dir) {

    if (dir.name == "byte") return (int)dir.arguments.size();

    if (dir.name == "word") return (int)dir.arguments.size() * 2;

    if (dir.name == "dword" || dir.name == "long") return (int)dir.arguments.size() * 4;

    if (dir.name == "float") return (int)dir.arguments.size() * 5;

    if (dir.name == "text" || dir.name == "ascii") return (int)dir.arguments[0].size();

    return 0;


}


std::vector<uint8_t> AssemblerParser::pass2() {

    std::vector<uint8_t> binary;
 std::shared_ptr<ProcContext> currentPass2Proc;
 bool isDeadCode = false;
 std::vector<std::shared_ptr<ProcContext>> pass2ProcStack;
 M65Emitter e(binary, getZPStart());

    for (auto& [name, symbol] : symbolTable) if (symbol.isVariable) symbol.value = symbol.initialValue;

    for (auto& stmt : statements) {

        if (!stmt->label.empty()) isDeadCode = false;

        if (stmt->type == Statement::EXPR) {
 if (!isDeadCode) emitExpressionCode(binary, stmt->exprTarget, stmt->exprTokenIndex, stmt->scopePrefix);
 continue;
 
}

        if (stmt->type == Statement::MUL) {
 if (!isDeadCode) emitMulCode(binary, stmt->mulWidth, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix);
 continue;
 
}

        if (stmt->type == Statement::DIV) {
 if (!isDeadCode) emitDivCode(binary, stmt->mulWidth, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix);
 continue;
 
}

        if (stmt->type == Statement::STACK_INC) {
 if (!isDeadCode) emitStackIncDecCode(binary, true, stmt->instr.operandTokenIndex, stmt->scopePrefix);
 continue;
 
}

        if (stmt->type == Statement::STACK_DEC) {
 if (!isDeadCode) emitStackIncDecCode(binary, false, stmt->instr.operandTokenIndex, stmt->scopePrefix);
 continue;
 
}

        if (stmt->type == Statement::STACK_INC8) {
 if (!isDeadCode) emitStackIncDec8Code(binary, true, stmt->instr.operandTokenIndex, stmt->scopePrefix);
 continue;
 
}

        if (stmt->type == Statement::STACK_DEC8) {
 if (!isDeadCode) emitStackIncDec8Code(binary, false, stmt->instr.operandTokenIndex, stmt->scopePrefix);
 continue;
 
}

        if (stmt->type == Statement::ADD16) {
            if (!isDeadCode) emitAddSub16Code(binary, true, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix);
            continue;
        }

        if (stmt->type == Statement::SUB16) {
            if (!isDeadCode) emitAddSub16Code(binary, false, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix);
            continue;
        }

        if (stmt->type == Statement::AND16 || stmt->type == Statement::ORA16 || stmt->type == Statement::EOR16) {
            if (!isDeadCode) emitBitwise16Code(binary, stmt->instr.mnemonic, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix);
            continue;
        }

        if (stmt->type == Statement::CPW) {
            if (!isDeadCode) emitCPWCode(binary, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix);
            continue;
        }

        if (stmt->type == Statement::LDW) {
            if (!isDeadCode) emitLDWCode(binary, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix, stmt->instr.mnemonic == "LDW.SP");
            continue;
        }

        if (stmt->type == Statement::STW) {
            if (!isDeadCode) emitSTWCode(binary, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix, stmt->instr.mnemonic == "STW.SP");
            continue;
        }

        if (stmt->type == Statement::FILL) {
            if (!isDeadCode) emitFillCode(binary, stmt->instr.operandTokenIndex, stmt->scopePrefix, stmt->instr.mnemonic == "FILL.SP");
            continue;
        }

        if (stmt->type == Statement::COPY) {
            if (!isDeadCode) emitMoveCode(binary, stmt->instr.operandTokenIndex, stmt->scopePrefix, stmt->instr.mnemonic == "MOVE.SP");
            continue;
        }

        if (stmt->type == Statement::SWAP) {
            if (!isDeadCode) emitSwapCode(binary, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix);
            continue;
        }

        if (stmt->type == Statement::NEG16) {
            if (!isDeadCode) emitNegNot16Code(binary, true, stmt->instr.operand, stmt->instr.operandTokenIndex, stmt->scopePrefix);
            continue;
        }

        if (stmt->type == Statement::NOT16) {
            if (!isDeadCode) emitNegNot16Code(binary, false, stmt->instr.operand, stmt->instr.operandTokenIndex, stmt->scopePrefix);
            continue;
        }

        if (stmt->type == Statement::CHKZERO8) {
            if (!isDeadCode) emitChkZeroCode(binary, false, false, stmt->instr.operandTokenIndex, stmt->scopePrefix);
            continue;
        }

        if (stmt->type == Statement::CHKZERO16) {
            if (!isDeadCode) emitChkZeroCode(binary, true, false, stmt->instr.operandTokenIndex, stmt->scopePrefix);
            continue;
        }

        if (stmt->type == Statement::CHKNONZERO8) {
            if (!isDeadCode) emitChkZeroCode(binary, false, true, stmt->instr.operandTokenIndex, stmt->scopePrefix);
            continue;
        }

        if (stmt->type == Statement::CHKNONZERO16) {
            if (!isDeadCode) emitChkZeroCode(binary, true, true, stmt->instr.operandTokenIndex, stmt->scopePrefix);
            continue;
        }

        if (stmt->type == Statement::BRANCH16) {
            if (!isDeadCode) emitBranch16Code(binary, stmt->instr.operandTokenIndex, stmt->scopePrefix);
            continue;
        }

        if (stmt->type == Statement::SELECT) {
            if (!isDeadCode) emitSelectCode(binary, stmt->instr.operandTokenIndex, stmt->scopePrefix);
            continue;
        }

        if (stmt->type == Statement::PTRSTACK) {
            if (!isDeadCode) emitPtrStackCode(binary, stmt->instr.operandTokenIndex, stmt->scopePrefix);
            continue;
        }

        if (stmt->type == Statement::PTRDEREF) {
            if (!isDeadCode) emitPtrDerefCode(binary, stmt->instr.operandTokenIndex, stmt->scopePrefix);
            continue;
        }

        if (stmt->type == Statement::LDWF || stmt->type == Statement::STWF || stmt->type == Statement::INCF || stmt->type == Statement::DECF) {
            if (!isDeadCode) emitFlatMemoryCode(binary, stmt->instr.mnemonic, stmt->instr.operandTokenIndex, stmt->scopePrefix);
            continue;
        }

        if (stmt->type == Statement::PHW_STACK) {
            if (!isDeadCode) emitPHWStackCode(binary, stmt->instr.operandTokenIndex, stmt->scopePrefix);
            continue;
        }

        if (stmt->type == Statement::AND16 || stmt->type == Statement::ORA16 || stmt->type == Statement::EOR16) {
            if (!isDeadCode) emitBitwise16Code(binary, stmt->instr.mnemonic, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix);
            continue;
        }

        if (stmt->type == Statement::ZERO) {
            if (!isDeadCode) emitZeroCode(binary, stmt->instr.operandTokenIndex, stmt->scopePrefix);
            continue;
        }

        if (stmt->type == Statement::BASIC_UPSTART) {
 if (!isDeadCode) {
 uint32_t addr = evaluateExpressionAt(stmt->basicUpstartTokenIndex, stmt->scopePrefix);
 std::string aS = std::to_string(addr);
 while (aS.length() < 4) aS = " " + aS;
 if (aS.length() > 4) aS = aS.substr(aS.length() - 4);
 uint16_t nL = (uint16_t)(stmt->address + 12 - 2);
 binary.push_back(nL & 0xFF);
 binary.push_back(nL >> 8);
 binary.push_back(0x0A);
 binary.push_back(0x00);
 binary.push_back(0x9E);
 for (char c : aS) binary.push_back((uint8_t)c);
 binary.push_back(0x00);
 binary.push_back(0x00);
 binary.push_back(0x00);
 
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
 binary.push_back(0x62);
 binary.push_back((uint8_t)stmt->instr.procParamSize);
 
}
 currentPass2Proc = pass2ProcStack.empty() ? nullptr : pass2ProcStack.back();
 if (!pass2ProcStack.empty()) pass2ProcStack.pop_back();
 isDeadCode = false;
 continue;
 
}

            else if (stmt->instr.mnemonic == "CALL") {

                if (!isDeadCode) {

                    for (const auto& arg : stmt->instr.callArgs) {
 bool isB = (arg.size() >= 2 && arg.substr(0, 2) == "B#");
 std::string v = isB ? arg.substr(2) : ( (arg.size() >= 2 && arg.substr(0, 2) == "W#") ? arg.substr(2) : (arg[0] == '#' ? arg.substr(1) : arg));
 uint32_t val;
 Symbol* sym = resolveSymbol(v, stmt->scopePrefix);
 if (sym) val = sym->value;
 else val = parseNumericLiteral(v);
 if (!isB && !arg.empty() && arg[0] != '#' && arg.size() >= 2 && arg.substr(0,2) != "W#" && sym) isB = (sym->size == 1);
 if (isB) {
 e.lda_imm(val & 0xFF);
 e.pha();
 
}
 else {
 binary.push_back(0xF2);
 binary.push_back(val & 0xFF);
 binary.push_back(val >> 8);
 
}
 
}

                    Symbol* sym = resolveSymbol(stmt->instr.operand, stmt->scopePrefix);
 uint32_t target = sym ? sym->value : 0;
 binary.push_back(0x20);
 binary.push_back(target & 0xFF);
 binary.push_back(target >> 8);

                
}

            
}
 else {

                if (!isDeadCode) {

                    AddressingMode resolvedMode = stmt->instr.mode;
                    if (stmt->instr.operandTokenIndex != -1 && (stmt->instr.mode == AddressingMode::BASE_PAGE || stmt->instr.mode == AddressingMode::ABSOLUTE || 
                        stmt->instr.mode == AddressingMode::BASE_PAGE_X || stmt->instr.mode == AddressingMode::ABSOLUTE_X ||
                        stmt->instr.mode == AddressingMode::BASE_PAGE_Y || stmt->instr.mode == AddressingMode::ABSOLUTE_Y)) {
                        
                        uint32_t val = evaluateExpressionAt(stmt->instr.operandTokenIndex, stmt->scopePrefix);
                        bool fitsIn8 = (val <= 0xFF);
                        bool forceAbs = (stmt->instr.mnemonic == "JSR" || stmt->instr.mnemonic == "JMP");

                        if (stmt->instr.mode == AddressingMode::BASE_PAGE || stmt->instr.mode == AddressingMode::ABSOLUTE) {
                            resolvedMode = (fitsIn8 && !forceAbs) ? AddressingMode::BASE_PAGE : AddressingMode::ABSOLUTE;
                        } else if (stmt->instr.mode == AddressingMode::BASE_PAGE_X || stmt->instr.mode == AddressingMode::ABSOLUTE_X) {
                            resolvedMode = (fitsIn8 && !forceAbs) ? AddressingMode::BASE_PAGE_X : AddressingMode::ABSOLUTE_X;
                        } else if (stmt->instr.mode == AddressingMode::BASE_PAGE_Y || stmt->instr.mode == AddressingMode::ABSOLUTE_Y) {
                            resolvedMode = (fitsIn8 && !forceAbs) ? AddressingMode::BASE_PAGE_Y : AddressingMode::ABSOLUTE_Y;
                        }
                    }

                    bool isQuad = (stmt->instr.mnemonic.size() > 1 && stmt->instr.mnemonic.back() == 'Q' && stmt->instr.mnemonic != "LDQ" && stmt->instr.mnemonic != "STQ" && stmt->instr.mnemonic != "BEQ" && stmt->instr.mnemonic != "BNE" && stmt->instr.mnemonic != "BRA" && stmt->instr.mnemonic != "BSR");

                    if (isQuad) {
 binary.push_back(0x42);
 binary.push_back(0x42);
 
}
 if (resolvedMode == AddressingMode::FLAT_INDIRECT_Z) e.eom();

                    bool isBranch = (stmt->instr.mnemonic == "BEQ" || stmt->instr.mnemonic == "BNE" || stmt->instr.mnemonic == "BRA" || stmt->instr.mnemonic == "BCC" || stmt->instr.mnemonic == "BCS" || stmt->instr.mnemonic == "BPL" || stmt->instr.mnemonic == "BMI" || stmt->instr.mnemonic == "BVC" || stmt->instr.mnemonic == "BVS" || stmt->instr.mnemonic == "BSR");

                    if (!isBranch) {
 
                        uint8_t op = getOpcode(stmt->instr.mnemonic, resolvedMode);
 
                        if (op == 0 && stmt->instr.mnemonic != "BRK") {

                            std::string errorMsg = "Invalid instruction or addressing mode: " + stmt->instr.mnemonic + " " + AddressingModeToString(resolvedMode);

                            auto validModes = getValidAddressingModes(stmt->instr.mnemonic);

                            if (validModes.empty()) {
 errorMsg = "Unknown mnemonic: " + stmt->instr.mnemonic;
 
}

                            else {
 errorMsg += ". Valid addressing modes for " + stmt->instr.mnemonic + " are: ";
 for (size_t i = 0;
 i < validModes.size();
 ++i) {
 errorMsg += AddressingModeToString(validModes[i]);
 if (i < validModes.size() - 1) errorMsg += ", ";
 
}
 
}

                            throw std::runtime_error(errorMsg + " at line " + std::to_string(stmt->line));

                        
}

                        binary.push_back(op);
 
                    
}

                    if (!isBranch) {
                        if (resolvedMode == AddressingMode::IMMEDIATE || resolvedMode == AddressingMode::STACK_RELATIVE || resolvedMode == AddressingMode::BASE_PAGE_INDIRECT_Z || resolvedMode == AddressingMode::FLAT_INDIRECT_Z || resolvedMode == AddressingMode::BASE_PAGE_INDIRECT_SP_Y || resolvedMode == AddressingMode::BASE_PAGE_X_INDIRECT || resolvedMode == AddressingMode::BASE_PAGE_INDIRECT_Y || resolvedMode == AddressingMode::BASE_PAGE || resolvedMode == AddressingMode::BASE_PAGE_X || resolvedMode == AddressingMode::BASE_PAGE_Y) {
                            uint32_t v = evaluateExpressionAt(stmt->instr.operandTokenIndex, stmt->scopePrefix);
                            binary.push_back((uint8_t)v);
                        }
                        else if (resolvedMode == AddressingMode::ABSOLUTE || resolvedMode == AddressingMode::ABSOLUTE_X || resolvedMode == AddressingMode::ABSOLUTE_Y || resolvedMode == AddressingMode::ABSOLUTE_INDIRECT || resolvedMode == AddressingMode::ABSOLUTE_X_INDIRECT || resolvedMode == AddressingMode::IMMEDIATE16) {
                            uint32_t a = evaluateExpressionAt(stmt->instr.operandTokenIndex, stmt->scopePrefix);
                            binary.push_back(a & 0xFF);
                            binary.push_back(a >> 8);
                        }
                    }


                    else if (stmt->instr.mnemonic == "BEQ" || stmt->instr.mnemonic == "BNE" || stmt->instr.mnemonic == "BRA" || stmt->instr.mnemonic == "BCC" || stmt->instr.mnemonic == "BCS" || stmt->instr.mnemonic == "BPL" || stmt->instr.mnemonic == "BMI" || stmt->instr.mnemonic == "BVC" || stmt->instr.mnemonic == "BVS" || stmt->instr.mnemonic == "BSR") {
                        Symbol* sym = resolveSymbol(stmt->instr.operand, stmt->scopePrefix);
                        uint32_t t = sym ? sym->value : 0;
                        if (stmt->instr.mnemonic == "BSR") {
                            int32_t off = (int32_t)t - (int32_t)(stmt->address + 3);
                            binary.push_back(0x63);
                            binary.push_back(off & 0xFF);
                            binary.push_back(off >> 8);
                        } else if (stmt->size == 2) {
                            int32_t off2 = (int32_t)t - (int32_t)(stmt->address + 2);
                            binary.push_back(getOpcode(stmt->instr.mnemonic, AddressingMode::RELATIVE));
                            binary.push_back((uint8_t)(int8_t)off2);
                        } else {
                            int32_t off3 = (int32_t)t - (int32_t)(stmt->address + 3);
                            binary.push_back(getOpcode(stmt->instr.mnemonic, AddressingMode::RELATIVE16));
                            binary.push_back(off3 & 0xFF);
                            binary.push_back(off3 >> 8);
                        }
                    }

                    else if (stmt->instr.mode == AddressingMode::BASE_PAGE_RELATIVE) {
 Symbol* sym = resolveSymbol(stmt->instr.operand, stmt->scopePrefix);
 uint32_t v = sym ? sym->value : parseNumericLiteral(stmt->instr.operand);
 binary.push_back((uint8_t)v);
 Symbol* tsym = resolveSymbol(stmt->instr.bitBranchTarget, stmt->scopePrefix);
 uint32_t t = tsym ? tsym->value : 0;
 int32_t off = (int32_t)t - (int32_t)(stmt->address + 3);
 binary.push_back((uint8_t)off);
 
}

                    else if (stmt->instr.mnemonic == "RTN") {
 Symbol* sym = resolveSymbol(stmt->instr.operand, stmt->scopePrefix);
 uint32_t v = sym ? sym->value : parseNumericLiteral(stmt->instr.operand);
 if (v == 0) binary.push_back(0x60);
 else {
 binary.push_back(0x62);
 binary.push_back((uint8_t)v);
 
}
 
}

                
}

                if (stmt->instr.mnemonic == "RTS" || stmt->instr.mnemonic == "RTN" || stmt->instr.mnemonic == "RTI") isDeadCode = true;

            
}

        
}
 else if (stmt->type == Statement::DIRECTIVE) {

            if (!isDeadCode || stmt->dir.name == "org") {

                if (stmt->dir.name == "var") {
 if (stmt->dir.varType == Directive::ASSIGN) symbolTable[stmt->dir.varName].value = evaluateExpressionAt(stmt->dir.tokenIndex, stmt->scopePrefix);
 else if (stmt->dir.varType == Directive::INC) symbolTable[stmt->dir.varName].value++;
 else if (stmt->dir.varType == Directive::DEC) symbolTable[stmt->dir.varName].value--;
 
}

                else if (stmt->dir.name == "cleanup") {
 if (currentPass2Proc) currentPass2Proc->totalParamSize += evaluateExpressionAt(stmt->dir.tokenIndex, stmt->scopePrefix);
 
}

                else if (stmt->dir.name == "byte") for (const auto& a : stmt->dir.arguments) binary.push_back((uint8_t)parseNumericLiteral(a));

                else if (stmt->dir.name == "word") for (const auto& a : stmt->dir.arguments) {
 uint32_t v = parseNumericLiteral(a);
 binary.push_back(v & 0xFF);
 binary.push_back(v >> 8);
 
}

                else if (stmt->dir.name == "dword" || stmt->dir.name == "long") for (const auto& a : stmt->dir.arguments) {
 uint32_t v = parseNumericLiteral(a);
 binary.push_back(v & 0xFF);
 binary.push_back(v >> 8);
 binary.push_back(v >> 16);
 binary.push_back(v >> 24);
 
}

                else if (stmt->dir.name == "float") {
 for (const auto& a : stmt->dir.arguments) {
 double v = std::stod(a);
 std::vector<uint8_t> enc = encodeFloat(v);
 for (uint8_t eb : enc) binary.push_back(eb);
 
}
 
}

                else if (stmt->dir.name == "text") {
 for (char c : stmt->dir.arguments[0]) binary.push_back(toPetscii(c));
 
}

                else if (stmt->dir.name == "ascii") {
 for (char c : stmt->dir.arguments[0]) binary.push_back((uint8_t)c);
 
}

            
}

        
}

    
}

    return binary;


}

