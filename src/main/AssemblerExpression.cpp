#include "AssemblerExpression.hpp"
#include "AssemblerParser.hpp"
#include "M65Emitter.hpp"
#include <stdexcept>

// ConstantNode
uint32_t ConstantNode::getValue(AssemblerParser*) const { return value; }
bool ConstantNode::isConstant(AssemblerParser*) const { return true; }
bool ConstantNode::is16Bit(AssemblerParser*) const { return value > 0xFF; }
void ConstantNode::emit(M65Emitter& e, AssemblerParser* parser, int width, const std::string&) {
    if (!parser) return;
    e.lda_imm(value & 0xFF);
    if (width >= 16) e.ldx_imm((value >> 8) & 0xFF);
}

// RegisterNode
uint32_t RegisterNode::getValue(AssemblerParser*) const { return 0; }
bool RegisterNode::isConstant(AssemblerParser*) const { return false; }
bool RegisterNode::is16Bit(AssemblerParser*) const { return name.size() > 2; }
void RegisterNode::emit(M65Emitter& e, AssemblerParser* parser, int width, const std::string&) {
    if (!parser) return;
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

// FlagNode
uint32_t FlagNode::getValue(AssemblerParser*) const { return 0; }
bool FlagNode::isConstant(AssemblerParser*) const { return false; }
bool FlagNode::is16Bit(AssemblerParser*) const { return false; }
void FlagNode::emit(M65Emitter& e, AssemblerParser* parser, int width, const std::string&) {
    if (!parser) return;
    if (flag == 'C') { e.lda_imm(0); e.adc_imm(0); }
    else {
        int8_t branchOp = 0;
        switch (flag) {
            case 'Z': branchOp = 0xD0; break;
            case 'V': branchOp = 0x50; break;
            case 'N': branchOp = 0x10; break;
        }
        if (branchOp != 0) { e.lda_imm(0); e.bne(0x02); e.lda_imm(1); }
        else {
            uint8_t mask = 0;
            if (flag == 'I') mask = 0x04;
            else if (flag == 'D') mask = 0x08;
            else if (flag == 'B') mask = 0x10;
            if (mask != 0) { e.pha(); e.pla(); e.and_imm(mask); e.beq(0x02); e.lda_imm(1); }
            else e.lda_imm(0);
        }
    }
    if (width >= 16) e.ldx_imm(0);
}

// VariableNode
uint32_t VariableNode::getValue(AssemblerParser* parser) const {
    Symbol* sym = parser->resolveSymbol(name, scopePrefix);
    return sym ? sym->value : 0;
}
bool VariableNode::isConstant(AssemblerParser* parser) const {
    Symbol* sym = parser->resolveSymbol(name, scopePrefix);
    return sym ? !sym->isAddress : false;
}
bool VariableNode::is16Bit(AssemblerParser* parser) const {
    Symbol* sym = parser->resolveSymbol(name, scopePrefix);
    return sym ? sym->size > 1 : true;
}
void VariableNode::emit(M65Emitter& e, AssemblerParser* parser, int width, const std::string&) {
    if (!parser) return;
    Symbol* sym = parser->resolveSymbol(name, scopePrefix);
    if (!sym) return;
    if (!sym->isAddress) {
        e.lda_imm(sym->value & 0xFF);
        if (width >= 16) e.ldx_imm((sym->value >> 8) & 0xFF);
    } else {
        if (sym->isStackRelative) {
            e.lda_stack((uint8_t)sym->stackOffset);
            if (width >= 16) {
                e.lda_stack((uint8_t)(sym->stackOffset + 1));
                e.tax();
                e.lda_stack((uint8_t)sym->stackOffset);
            }
        } else {
            e.lda_abs(sym->value);
            if (width >= 16) e.ldx_abs(sym->value + 1);
        }
    }
}

// UnaryExpr
uint32_t UnaryExpr::getValue(AssemblerParser* parser) const {
    uint32_t val = operand ? operand->getValue(parser) : 0;
    if (op == "!") return val == 0 ? 1 : 0;
    if (op == "~") return ~val;
    if (op == "-") return -val;
    if (op == "<") return val & 0xFF;
    if (op == ">") return (val >> 8) & 0xFF;
    return 0;
}
bool UnaryExpr::isConstant(AssemblerParser* parser) const { return operand ? operand->isConstant(parser) : true; }
bool UnaryExpr::is16Bit(AssemblerParser* parser) const { return operand ? operand->is16Bit(parser) : false; }
void UnaryExpr::emit(M65Emitter& e, AssemblerParser* parser, int width, const std::string& target) {
    if (!parser || !operand) return;
    operand->emit(e, parser, width, target);
    if (op == "!") {
        e.bne(0x05);
        if (width >= 16) { e.txa(); e.bne(0x02); }
        e.lda_imm(1);
        e.bra(0x02);
        e.lda_imm(0);
        if (width >= 16) { e.tax(); e.ldx_imm(0); }
    } else if (op == "~") {
        e.eor_imm(0xFF);
        if (width >= 16) { e.pha(); e.txa(); e.eor_imm(0xFF); e.tax(); e.pla(); }
    } else if (op == "-") {
        e.neg_a();
        if (width >= 16) { e.pha(); e.txa(); e.eor_imm(0xFF); e.adc_imm(0); e.tax(); e.pla(); }
    } else if (op == "<") {
        if (width >= 16) e.ldx_imm(0);
    } else if (op == ">") {
        if (width >= 16) { e.txa(); e.ldx_imm(0); }
        else { e.txa(); }
    }
}

// DereferenceNode
uint32_t DereferenceNode::getValue(AssemblerParser*) const { return 0; }
bool DereferenceNode::isConstant(AssemblerParser*) const { return false; }
bool DereferenceNode::is16Bit(AssemblerParser*) const { return true; }
void DereferenceNode::emit(M65Emitter& e, AssemblerParser* parser, int width, const std::string&) {
    if (!parser || !address) return;
    address->emit(e, parser, 16, ".AX");
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

// BinaryExpr
uint32_t BinaryExpr::getValue(AssemblerParser* parser) const {
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
bool BinaryExpr::isConstant(AssemblerParser* parser) const {
    return (left ? left->isConstant(parser) : true) && (right ? right->isConstant(parser) : true);
}
bool BinaryExpr::is16Bit(AssemblerParser* parser) const {
    return getValue(parser) > 0xFF || (left && left->is16Bit(parser)) || (right && right->is16Bit(parser));
}
void BinaryExpr::emit(M65Emitter& e, AssemblerParser* parser, int width, const std::string& target) {
    if (!parser || !left || !right) return;
    int bytes = width / 8;
    if (bytes < 1) bytes = 1;
    if (bytes > 4) bytes = 4;
    if (op == "*" || op == "/" || op == "+" || op == "-") {
        left->emit(e, parser, width, ".A");
        uint8_t base = (op == "/") ? 0x60 : 0x70;
        e.sta_abs(0xD700 + base);
        if (bytes >= 2) e.stx_abs(0xD701 + base);
        right->emit(e, parser, width, ".A");
        e.sta_abs(0xD700 + base + 4);
        if (bytes >= 2) e.stx_abs(0xD701 + base + 4);
        if (op == "*") {
            for (int i = 0; i < bytes; ++i) {
                e.lda_abs(0xD778 + i);
                if (i == 1) e.tax();
            }
        } else if (op == "/") {
            e.bit_abs(0xD70F);
            e.bne(-5);
            for (int i = 0; i < bytes; ++i) {
                e.lda_abs(0xD768 + i);
                if (i == 1) e.tax();
            }
        } else if (op == "+") {
            for (int i = 0; i < bytes; ++i) {
                e.lda_abs(0xD77C + i);
                if (i == 1) e.tax();
            }
        } else if (op == "-") {
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
        left->emit(e, parser, width, ".A");
        e.push_ax();
        right->emit(e, parser, width, ".A");
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
    } else if (op == "<<" || op == ">>") {
        left->emit(e, parser, width, ".A");
        e.push_ax();
        right->emit(e, parser, width, ".A");
        e.sta_s(0);
        e.pop_ax();
        e.lda_s(0);
        e.beq((width >= 16) ? 0x07 : 0x05);
        e.dec_s(0);
        if (op == "<<") {
            e.asl_a();
            if (width >= 16) { e.rol_a(); e.txa(); e.rol_a(); e.tax(); }
        } else {
            if (width >= 16) { e.txa(); e.lsr_a(); e.tax(); e.ror_a(); }
            else e.lsr_a();
        }
        e.bra((width >= 16) ? -11 : -9);
    }
}

std::unique_ptr<ExprAST> parseExprAST(const std::vector<AssemblerToken>& tokens, int& idx, std::map<std::string, Symbol>& symbolTable, const std::string& scopePrefix) {
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
    };
    auto parseMultiplicative = [&]() -> std::unique_ptr<ExprAST> {
        auto left = parsePrimary();
        while (idx < (int)tokens.size() && (tokens[idx].type == AssemblerTokenType::STAR || tokens[idx].type == AssemblerTokenType::SLASH)) {
            std::string op = tokens[idx++].value;
            auto right = parsePrimary();
            left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
        }
        return left;
    };
    auto parseAdditive = [&]() -> std::unique_ptr<ExprAST> {
        auto left = parseMultiplicative();
        while (idx < (int)tokens.size() && (tokens[idx].type == AssemblerTokenType::PLUS || tokens[idx].type == AssemblerTokenType::MINUS)) {
            std::string op = tokens[idx++].value;
            auto right = parseMultiplicative();
            left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
        }
        return left;
    };
    auto parseShift = [&]() -> std::unique_ptr<ExprAST> {
        auto left = parseAdditive();
        while (idx < (int)tokens.size() && (tokens[idx].type == AssemblerTokenType::LSHIFT || tokens[idx].type == AssemblerTokenType::RSHIFT)) {
            std::string op = tokens[idx++].value;
            auto right = parseAdditive();
            left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
        }
        return left;
    };
    return parseShift();
}
