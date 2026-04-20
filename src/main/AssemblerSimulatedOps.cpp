#include "AssemblerSimulatedOps.hpp"
#include "AssemblerParser.hpp"
#include "M65Emitter.hpp"
#include "AssemblerExpression.hpp"
#include <algorithm>
#include <stdexcept>

static uint32_t parseNumericLiteral(const std::string& literal) {
    if (literal.empty()) throw std::runtime_error("Empty numeric literal");
    try {
        if (literal[0] == '$') return std::stoul(literal.substr(1), nullptr, 16);
        if (literal[0] == '%') return std::stoul(literal.substr(1), nullptr, 2);
        return std::stoul(literal);
    } catch (...) {
        throw std::runtime_error("Invalid numeric literal: " + literal);
    }
}

void AssemblerSimulatedOps::emitExpressionCode(AssemblerParser* parser, M65Emitter& e, const std::string& target, int tokenIndex, const std::string& scopePrefix) {
    int idx = tokenIndex;
    auto ast = parseExprAST(parser->tokens, idx, parser->symbolTable, scopePrefix);
    if (!ast) return;

    int width = 8;
    if (target == ".AX" || target == ".AY" || target == ".AZ") width = 16;
    else if (target == ".AXY" || target == ".AXZ") width = 24;
    else if (target == ".Q" || target == ".AXYZ") width = 32;
    else if (target[0] != '.') width = 16;

    if (ast->isConstant(parser)) {
        uint32_t val = ast->getValue(parser);
        e.lda_imm(val & 0xFF);
        if (width >= 16) e.ldx_imm((val >> 8) & 0xFF);
    } else {
        ast->emit(e, parser, width, target);
    }

    if (target[0] != '.') {
        Symbol* sym = parser->resolveSymbol(target, scopePrefix);
        uint32_t addr = sym ? sym->value : parseNumericLiteral(target);
        e.sta_abs(addr);
        if (width >= 16) {
            e.txa();
            e.sta_abs(addr + 1);
        }
    }
}

void AssemblerSimulatedOps::emitMulCode(AssemblerParser* parser, M65Emitter& e, int width, const std::string& dest, int tokenIndex, const std::string& scopePrefix) {
    int bytes = width / 8;
    if (bytes < 1) bytes = 1;
    if (bytes > 4) bytes = 4;
    int idx = tokenIndex;
    auto srcAst = parseExprAST(parser->tokens, idx, parser->symbolTable, scopePrefix);
    if (!srcAst) return;
    auto storeMath = [&](uint8_t base, int i, const std::string& src) {
        if (src == ".A") e.sta_abs(0xD700 + base + i);
        else if (src == ".X") { e.txa(); e.sta_abs(0xD700 + base + i); }
        else if (src == ".Y") { e.tya(); e.sta_abs(0xD700 + base + i); }
        else if (src == ".Z") { e.tza(); e.sta_abs(0xD700 + base + i); }
        else {
            Symbol* sym = parser->resolveSymbol(src, scopePrefix);
            uint32_t addr = (sym ? sym->value : parseNumericLiteral(src)) + i;
            e.lda_abs(addr); e.sta_abs(0xD700 + base + i);
        }
    };
    if (dest == ".A" || dest == ".AX" || dest == ".AXY" || dest == ".AXYZ" || dest == ".Q") {
        if (bytes >= 1) storeMath(0x70, 0, ".A");
        if (bytes >= 2) storeMath(0x70, 1, ".X");
        if (bytes >= 3) storeMath(0x70, 2, ".Y");
        if (bytes >= 4) storeMath(0x70, 3, ".Z");
    } else for (int i = 0; i < bytes; ++i) storeMath(0x70, i, dest);

    if (srcAst->isConstant(parser)) {
        uint32_t val = srcAst->getValue(parser);
        for (int i = 0; i < bytes; ++i) {
            e.lda_imm((val >> (i * 8)) & 0xFF);
            e.sta_abs(0xD774 + i);
        }
    } else {
        std::string srcName = parser->tokens[tokenIndex].value;
        if (parser->tokens[tokenIndex].type == AssemblerTokenType::REGISTER) srcName = "." + srcName;
        if (srcName == ".A" || srcName == ".AX" || srcName == ".AXY" || srcName == ".AXYZ" || srcName == ".Q") {
            if (bytes >= 1) storeMath(0x74, 0, ".A");
            if (bytes >= 2) storeMath(0x74, 1, ".X");
            if (bytes >= 3) storeMath(0x74, 2, ".Y");
            if (bytes >= 4) storeMath(0x74, 3, ".Z");
        } else for (int i = 0; i < bytes; ++i) storeMath(0x74, i, srcName);
    }
    for (int i = 0; i < bytes; ++i) {
        e.lda_abs(0xD778 + i);
        if (dest == ".A" || dest == ".AX" || dest == ".AXY" || dest == ".AXYZ" || dest == ".Q") {
            if (i == 1) e.tax(); else if (i == 2) e.tay(); else if (i == 3) e.taz();
        } else {
            Symbol* sym = parser->resolveSymbol(dest, scopePrefix);
            uint32_t addr = sym ? sym->value : parseNumericLiteral(dest);
            e.sta_abs(addr + i);
        }
    }
}

void AssemblerSimulatedOps::emitDivCode(AssemblerParser* parser, M65Emitter& e, int width, const std::string& dest, int tokenIndex, const std::string& scopePrefix) {
    int bytes = width / 8;
    if (bytes < 1) bytes = 1;
    if (bytes > 4) bytes = 4;
    int idx = tokenIndex;
    auto srcAst = parseExprAST(parser->tokens, idx, parser->symbolTable, scopePrefix);
    if (!srcAst) return;
    auto storeMath = [&](uint8_t base, int i, const std::string& src) {
        if (src == ".A") e.sta_abs(0xD700 + base + i);
        else if (src == ".X") { e.txa(); e.sta_abs(0xD700 + base + i); }
        else if (src == ".Y") { e.tya(); e.sta_abs(0xD700 + base + i); }
        else if (src == ".Z") { e.tza(); e.sta_abs(0xD700 + base + i); }
        else {
            Symbol* sym = parser->resolveSymbol(src, scopePrefix);
            uint32_t addr = (sym ? sym->value : parseNumericLiteral(src)) + i;
            e.lda_abs(addr); e.sta_abs(0xD700 + base + i);
        }
    };
    if (dest == ".A" || dest == ".AX" || dest == ".AXY" || dest == ".AXYZ" || dest == ".Q") {
        if (bytes >= 1) storeMath(0x60, 0, ".A");
        if (bytes >= 2) storeMath(0x60, 1, ".X");
        if (bytes >= 3) storeMath(0x60, 2, ".Y");
        if (bytes >= 4) storeMath(0x60, 3, ".Z");
    } else for (int i = 0; i < bytes; ++i) storeMath(0x60, i, dest);

    if (srcAst->isConstant(parser)) {
        uint32_t val = srcAst->getValue(parser);
        if (val == 0) throw std::runtime_error("Division by zero at line " + std::to_string(parser->tokens[tokenIndex].line));
        for (int i = 0; i < bytes; ++i) {
            e.lda_imm((val >> (i * 8)) & 0xFF);
            e.sta_abs(0xD764 + i);
        }
    } else {
        std::string srcName = parser->tokens[tokenIndex].value;
        if (parser->tokens[tokenIndex].type == AssemblerTokenType::REGISTER) srcName = "." + srcName;
        if (srcName == ".A" || srcName == ".AX" || srcName == ".AXY" || srcName == ".AXYZ" || srcName == ".Q") {
            if (bytes >= 1) storeMath(0x64, 0, ".A");
            if (bytes >= 2) storeMath(0x64, 1, ".X");
            if (bytes >= 3) storeMath(0x64, 2, ".Y");
            if (bytes >= 4) storeMath(0x64, 3, ".Z");
        } else for (int i = 0; i < bytes; ++i) storeMath(0x64, i, srcName);
    }
    e.bit_abs(0xD70F); e.bne(-5);
    for (int i = 0; i < bytes; ++i) {
        e.lda_abs(0xD768 + i);
        if (dest == ".A" || dest == ".AX" || dest == ".AXY" || dest == ".AXYZ" || dest == ".Q") {
            if (i == 1) e.tax(); else if (i == 2) e.tay(); else if (i == 3) e.taz();
        } else {
            Symbol* sym = parser->resolveSymbol(dest, scopePrefix);
            uint32_t addr = sym ? sym->value : parseNumericLiteral(dest);
            e.sta_abs(addr + i);
        }
    }
}

void AssemblerSimulatedOps::emitStackIncDecCode(AssemblerParser* parser, M65Emitter& e, bool isInc, int tokenIndex, const std::string& scopePrefix) {
    uint32_t offset = parser->evaluateExpressionAt(tokenIndex, scopePrefix);
    e.tsx();
    if (isInc) { e.inc_abs_x(0x0101 + offset); e.bne(0x03); e.inc_abs_x(0x0101 + offset + 1); }
    else { e.lda_abs_x(0x0101 + offset); e.bne(0x03); e.dec_abs_x(0x0101 + offset + 1); e.dec_abs_x(0x0101 + offset); }
}

void AssemblerSimulatedOps::emitStackIncDec8Code(AssemblerParser* parser, M65Emitter& e, bool isInc, int tokenIndex, const std::string& scopePrefix) {
    uint32_t offset = parser->evaluateExpressionAt(tokenIndex, scopePrefix);
    e.tsx();
    if (isInc) e.inc_abs_x(0x0101 + offset); else e.dec_abs_x(0x0101 + offset);
}

void AssemblerSimulatedOps::emitAddSub16Code(AssemblerParser* parser, M65Emitter& e, bool isAdd, const std::string& dest, int tokenIndex, const std::string& scopePrefix) {
    int idx = tokenIndex;
    auto srcAst = parseExprAST(parser->tokens, idx, parser->symbolTable, scopePrefix);
    if (!srcAst) return;
    std::string DEST = dest;
    if (!DEST.empty() && DEST[0] != '.') DEST = "." + DEST;
    std::transform(DEST.begin(), DEST.end(), DEST.begin(), ::toupper);
    if (DEST == ".AX") {
        if (isAdd) e.clc(); else e.sec();
        if (srcAst->isConstant(parser)) {
            uint32_t val = srcAst->getValue(parser);
            if (isAdd) e.adc_imm(val & 0xFF); else e.sbc_imm(val & 0xFF);
            e.pha(); e.txa();
            if (isAdd) e.adc_imm((val >> 8) & 0xFF); else e.sbc_imm((val >> 8) & 0xFF);
            e.tax(); e.pla();
        } else {
            std::string src = parser->tokens[tokenIndex].value;
            if (parser->tokens[tokenIndex].type == AssemblerTokenType::REGISTER) src = "." + src;
            else if (!src.empty() && src[0] != '.' && (src=="A"||src=="X"||src=="Y"||src=="Z"||src=="a"||src=="x"||src=="y"||src=="z")) src = "." + src;
            Symbol* sym = parser->resolveSymbol(src, scopePrefix);
            uint32_t addr = sym ? sym->value : parseNumericLiteral(src);
            if (isAdd) e.adc_abs(addr); else e.sbc_abs(addr);
            e.pha(); e.txa();
            if (isAdd) e.adc_abs(addr + 1); else e.sbc_abs(addr + 1);
            e.tax(); e.pla();
        }
    } else throw std::runtime_error("Simulated ADD.16/SUB.16 only supports .AX, found " + DEST);
}

void AssemblerSimulatedOps::emitBitwise16Code(AssemblerParser* parser, M65Emitter& e, const std::string& mnemonic, const std::string& dest, int tokenIndex, const std::string& scopePrefix) {
    int idx = tokenIndex;
    auto srcAst = parseExprAST(parser->tokens, idx, parser->symbolTable, scopePrefix);
    if (!srcAst) return;
    std::string DEST = dest;
    if (!DEST.empty() && DEST[0] != '.') DEST = "." + DEST;
    std::transform(DEST.begin(), DEST.end(), DEST.begin(), ::toupper);
    std::string M = mnemonic; std::transform(M.begin(), M.end(), M.begin(), ::toupper);
    if (DEST == ".AX") {
        if (srcAst->isConstant(parser)) {
            uint32_t val = srcAst->getValue(parser);
            if (M == "AND.16") e.and_imm(val & 0xFF); else if (M == "ORA.16") e.ora_imm(val & 0xFF); else if (M == "EOR.16") e.eor_imm(val & 0xFF);
            e.pha(); e.txa();
            if (M == "AND.16") e.and_imm((val >> 8) & 0xFF); else if (M == "ORA.16") e.ora_imm((val >> 8) & 0xFF); else if (M == "EOR.16") e.eor_imm((val >> 8) & 0xFF);
            e.tax(); e.pla();
        } else {
            std::string src = parser->tokens[tokenIndex].value;
            if (parser->tokens[tokenIndex].type == AssemblerTokenType::REGISTER) src = "." + src;
            else if (!src.empty() && src[0] != '.' && (src=="A"||src=="X"||src=="Y"||src=="Z"||src=="a"||src=="x"||src=="y"||src=="z")) src = "." + src;
            Symbol* sym = parser->resolveSymbol(src, scopePrefix);
            uint32_t addr = sym ? sym->value : parseNumericLiteral(src);
            if (M == "AND.16") e.and_abs(addr); else if (M == "ORA.16") e.ora_abs(addr); else if (M == "EOR.16") e.eor_abs(addr);
            e.pha(); e.txa();
            if (M == "AND.16") e.and_abs(addr + 1); else if (M == "ORA.16") e.ora_abs(addr + 1); else if (M == "EOR.16") e.eor_abs(addr + 1);
            e.tax(); e.pla();
        }
    } else throw std::runtime_error("Simulated bitwise 16-bit only supports .AX destination");
}

void AssemblerSimulatedOps::emitCPWCode(AssemblerParser* parser, M65Emitter& e, const std::string& src1, int tokenIndex, const std::string& scopePrefix) {
    int idx = tokenIndex;
    auto src2Ast = parseExprAST(parser->tokens, idx, parser->symbolTable, scopePrefix);
    if (!src2Ast) return;
    std::string SRC1 = src1;
    if (!SRC1.empty() && SRC1[0] != '.') SRC1 = "." + SRC1;
    std::transform(SRC1.begin(), SRC1.end(), SRC1.begin(), ::toupper);
    if (SRC1 == ".AX") {
        if (src2Ast->isConstant(parser)) {
            uint32_t val = src2Ast->getValue(parser);
            e.cmp_imm(val & 0xFF); e.bne(0x03); e.txa(); e.cmp_imm((val >> 8) & 0xFF);
        } else {
            std::string src2 = parser->tokens[tokenIndex].value;
            if (parser->tokens[tokenIndex].type == AssemblerTokenType::REGISTER) src2 = "." + src2;
            else if (!src2.empty() && src2[0] != '.' && (src2=="A"||src2=="X"||src2=="Y"||src2=="Z"||src2=="a"||src2=="x"||src2=="y"||src2=="z")) src2 = "." + src2;
            Symbol* sym = parser->resolveSymbol(src2, scopePrefix);
            uint32_t addr = sym ? sym->value : parseNumericLiteral(src2);
            e.cmp_abs(addr); e.bne(0x04); e.txa(); e.cmp_abs(addr + 1);
        }
    } else throw std::runtime_error("Simulated CPW only supports .AX as first operand");
}

void AssemblerSimulatedOps::emitLDWCode(AssemblerParser* parser, M65Emitter& e, const std::string& dest, int tokenIndex, const std::string& scopePrefix, bool forceStack) {
    int idx = tokenIndex;
    uint32_t offset = 0;
    bool isStack = forceStack ? (offset = parser->evaluateExpressionAt(tokenIndex, scopePrefix), true) : parser->isStackRelativeOperand(tokenIndex, offset, scopePrefix);
    std::string DEST = dest; if (!DEST.empty() && DEST[0] != '.') DEST = "." + DEST;
    std::transform(DEST.begin(), DEST.end(), DEST.begin(), ::toupper);
    if (DEST == ".AX" || DEST == ".AY" || DEST == ".AZ") {
        char reg2 = DEST[2];
        if (isStack) { e.lda_stack(offset); e.pha(); e.lda_stack(offset + 1); if (reg2 == 'X') e.tax(); else if (reg2 == 'Y') e.tya(); else if (reg2 == 'Z') e.tza(); e.pla(); }
        else {
            bool isImm = false; if (idx < (int)parser->tokens.size() && parser->tokens[idx].type == AssemblerTokenType::HASH) { isImm = true; idx++; }
            auto srcAst = parseExprAST(parser->tokens, idx, parser->symbolTable, scopePrefix);
            if (!srcAst) return;
            if (isImm || srcAst->isConstant(parser)) {
                uint32_t val = srcAst->getValue(parser); e.lda_imm(val & 0xFF); uint8_t val2 = (val >> 8) & 0xFF;
                if (reg2 == 'X') e.ldx_imm(val2); else if (reg2 == 'Y') e.ldy_imm(val2); else if (reg2 == 'Z') e.ldz_imm(val2);
            } else {
                std::string src = parser->tokens[tokenIndex].value; if (parser->tokens[tokenIndex].type == AssemblerTokenType::REGISTER) src = "." + src;
                else if (!src.empty() && src[0] != '.' && (src=="A"||src=="X"||src=="Y"||src=="Z"||src=="a"||src=="x"||src=="y"||src=="z")) src = "." + src;
                Symbol* sym = parser->resolveSymbol(src, scopePrefix); uint32_t addr = sym ? sym->value : parseNumericLiteral(src);
                e.lda_abs(addr); uint32_t addr2 = addr + 1; if (reg2 == 'X') e.ldx_abs(addr2); else if (reg2 == 'Y') e.ldy_abs(addr2); else if (reg2 == 'Z') e.ldz_abs(addr2);
            }
        }
    } else throw std::runtime_error("Simulated LDW only supports .AX, .AY, .AZ");
}

void AssemblerSimulatedOps::emitSTWCode(AssemblerParser* parser, M65Emitter& e, const std::string& src, int tokenIndex, const std::string& scopePrefix, bool forceStack) {
    uint32_t offset = 0;
    bool isStack = forceStack ? (offset = parser->evaluateExpressionAt(tokenIndex, scopePrefix), true) : parser->isStackRelativeOperand(tokenIndex, offset, scopePrefix);
    std::string SRC = src; if (!SRC.empty() && SRC[0] != '.' && SRC[0] != '#') SRC = "." + SRC;
    std::transform(SRC.begin(), SRC.end(), SRC.begin(), ::toupper);
    if (SRC[0] == '#') {
        int valIdx = tokenIndex - 1; while (valIdx >= 0 && parser->tokens[valIdx].type != AssemblerTokenType::HASH) valIdx--;
        if (valIdx >= 0) valIdx++; else valIdx = tokenIndex - 2;
        uint32_t val = parser->evaluateExpressionAt(valIdx, scopePrefix);
        uint8_t low = val & 0xFF, high = (val >> 8) & 0xFF;
        if (isStack) { e.lda_imm(low); e.sta_stack(offset); if (high == 0) e.stz_stack(offset + 1); else { e.lda_imm(high); e.sta_stack(offset + 1); } }
        else {
            std::string dest = parser->tokens[tokenIndex].value; if (parser->tokens[tokenIndex].type == AssemblerTokenType::REGISTER) dest = "." + dest;
            Symbol* sym = parser->resolveSymbol(dest, scopePrefix); uint32_t addr = sym ? sym->value : parseNumericLiteral(dest);
            e.lda_imm(low); e.sta_abs(addr); if (high == 0) e.stz_abs(addr + 1); else { e.lda_imm(high); e.sta_abs(addr + 1); }
        }
        return;
    }
    if (SRC == ".AX" || SRC == ".AY" || SRC == ".AZ") {
        char reg2 = SRC[2];
        if (isStack) { e.sta_stack(offset); e.pha(); if (reg2 == 'X') e.txa(); else if (reg2 == 'Y') e.tya(); else if (reg2 == 'Z') e.tza(); e.sta_stack(offset + 1); e.pla(); }
        else {
            std::string dest = parser->tokens[tokenIndex].value; if (parser->tokens[tokenIndex].type == AssemblerTokenType::REGISTER) dest = "." + dest;
            else if (!dest.empty() && dest[0] != '.' && (dest=="A"||dest=="X"||dest=="Y"||dest=="Z"||dest=="a"||dest=="x"||dest=="y"||dest=="z")) dest = "." + dest;
            Symbol* sym = parser->resolveSymbol(dest, scopePrefix); uint32_t addr = sym ? sym->value : parseNumericLiteral(dest);
            e.sta_abs(addr); uint32_t addr2 = addr + 1; if (reg2 == 'X') e.stx_abs(addr2); else if (reg2 == 'Y') e.sty_abs(addr2); else if (reg2 == 'Z') e.stz_abs(addr2);
        }
    } else throw std::runtime_error("Simulated STW only supports .AX, .AY, .AZ");
}

void AssemblerSimulatedOps::emitSwapCode(AssemblerParser* parser, M65Emitter& e, const std::string& r1, int tokenIndex, const std::string&) {
    std::string r2 = parser->tokens[tokenIndex].value; if (!r2.empty() && r2[0] != '.') r2 = "." + r2;
    std::transform(r2.begin(), r2.end(), r2.begin(), ::toupper);
    std::string R1 = r1; if (!R1.empty() && R1[0] != '.') R1 = "." + R1;
    std::transform(R1.begin(), R1.end(), R1.begin(), ::toupper);
    if (R1 == ".A" || r2 == ".A") {
        std::string other = (R1 == ".A") ? r2 : R1;
        if (other == ".X") { e.pha(); e.txa(); e.plx(); } else if (other == ".Y") { e.pha(); e.tya(); e.ply(); } else if (other == ".Z") { e.pha(); e.tza(); e.plz(); }
        else throw std::runtime_error("SWAP only supports A, X, Y, Z");
    } else {
        if ((R1 == ".X" && r2 == ".Y") || (R1 == ".Y" && r2 == ".X")) { e.phx(); e.tya(); e.tax(); e.ply(); }
        else if ((R1 == ".X" && r2 == ".Z") || (R1 == ".Z" && r2 == ".X")) { e.phx(); e.tza(); e.tax(); e.plz(); }
        else if ((R1 == ".Y" && r2 == ".Z") || (R1 == ".Z" && r2 == ".Y")) { e.phy(); e.tza(); e.tay(); e.plz(); }
        else throw std::runtime_error("SWAP only supports A, X, Y, Z");
    }
}

void AssemblerSimulatedOps::emitZeroCode(AssemblerParser* parser, M65Emitter& e, int tokenIndex, const std::string&) {
    int idx = tokenIndex;
    while (idx < (int)parser->tokens.size()) {
        std::string reg = parser->tokens[idx].value; std::transform(reg.begin(), reg.end(), reg.begin(), ::toupper);
        if (reg == "A") e.cla(); else if (reg == "X") e.clx(); else if (reg == "Y") e.cly(); else if (reg == "Z") e.clz();
        else throw std::runtime_error("ZERO only supports A, X, Y, Z");
        if (idx + 1 < (int)parser->tokens.size() && parser->tokens[idx+1].type == AssemblerTokenType::COMMA) idx += 2; else break;
    }
}

void AssemblerSimulatedOps::emitNegNot16Code(AssemblerParser* parser, M65Emitter& e, bool isNeg, const std::string& operand, int tokenIndex, const std::string& scopePrefix) {
    std::string OP = operand; if (!OP.empty() && OP[0] != '.') OP = "." + OP;
    std::transform(OP.begin(), OP.end(), OP.begin(), ::toupper);
    if (OP == ".AX" || OP == "") { if (isNeg) e.neg_16(); else e.not_16(); return; }
    uint32_t offset = 0;
    if (parser->isStackRelativeOperand(tokenIndex, offset, scopePrefix)) {
        e.tsx();
        if (isNeg) { e.lda_abs_x(0x0101 + offset); e.eor_imm(0xFF); e.clc(); e.adc_imm(1); e.sta_abs_x(0x0101 + offset); e.lda_abs_x(0x0102 + offset); e.eor_imm(0xFF); e.adc_imm(0); e.sta_abs_x(0x0102 + offset); }
        else { e.lda_abs_x(0x0101 + offset); e.eor_imm(0xFF); e.sta_abs_x(0x0101 + offset); e.lda_abs_x(0x0102 + offset); e.eor_imm(0xFF); e.sta_abs_x(0x0102 + offset); }
    } else {
        Symbol* sym = parser->resolveSymbol(operand, scopePrefix); uint32_t addr = sym ? sym->value : parseNumericLiteral(operand);
        if (isNeg) { e.lda_abs(addr); e.eor_imm(0xFF); e.clc(); e.adc_imm(1); e.sta_abs(addr); e.lda_abs(addr + 1); e.eor_imm(0xFF); e.adc_imm(0); e.sta_abs(addr + 1); }
        else { e.lda_abs(addr); e.eor_imm(0xFF); e.sta_abs(addr); e.lda_abs(addr + 1); e.eor_imm(0xFF); e.sta_abs(addr + 1); }
    }
}

void AssemblerSimulatedOps::emitChkZeroCode(AssemblerParser* parser, M65Emitter& e, bool is16, bool isInverse, int, const std::string&) {
    if (is16) { e.cmp_imm(0); e.bne(0x03); e.txa(); } else e.cmp_imm(0);
    if (isInverse) { e.bne(0x03); e.lda_imm(0); e.bra(0x02); e.lda_imm(1); }
    else { e.beq(0x03); e.lda_imm(0); e.bra(0x02); e.lda_imm(1); }
}

void AssemblerSimulatedOps::emitBranch16Code(AssemblerParser* parser, M65Emitter& e, int tokenIndex, const std::string&) {
    int idx = tokenIndex; if (idx >= (int)parser->tokens.size()) return;
    std::string condition = parser->tokens[idx++].value; std::transform(condition.begin(), condition.end(), condition.begin(), ::toupper);
    if (condition == "BEQ") e.beq(0x00); else if (condition == "BNE") e.bne(0x00);
}

void AssemblerSimulatedOps::emitSelectCode(AssemblerParser* parser, M65Emitter& e, int tokenIndex, const std::string& scopePrefix) {
    int idx = tokenIndex; if (idx >= (int)parser->tokens.size()) return;
    idx++; // Skip reg
    if (idx < (int)parser->tokens.size() && parser->tokens[idx].type == AssemblerTokenType::COMMA) idx++;
    auto val1Ast = parseExprAST(parser->tokens, idx, parser->symbolTable, scopePrefix);
    if (idx < (int)parser->tokens.size() && parser->tokens[idx].type == AssemblerTokenType::COMMA) idx++;
    auto val2Ast = parseExprAST(parser->tokens, idx, parser->symbolTable, scopePrefix);
    e.bne(0x03); e.lda_imm(val2Ast->getValue(parser)); e.bra(0x02); e.lda_imm(val1Ast->getValue(parser));
}

void AssemblerSimulatedOps::emitPtrStackCode(AssemblerParser* parser, M65Emitter& e, int tokenIndex, const std::string& scopePrefix) {
    uint32_t offset = parser->evaluateExpressionAt(tokenIndex, scopePrefix);
    e.tsx(); e.txa(); e.clc(); e.adc_imm(0x0101 + offset); e.pha(); e.lda_imm(0); e.adc_imm(0); e.tax(); e.pla();
}

void AssemblerSimulatedOps::emitPtrDerefCode(AssemblerParser* parser, M65Emitter& e, int tokenIndex, const std::string& scopePrefix) {
    std::string src = parser->tokens[tokenIndex].value; if (parser->tokens[tokenIndex].type == AssemblerTokenType::REGISTER) src = "." + src;
    Symbol* sym = parser->resolveSymbol(src, scopePrefix); uint32_t addr = sym ? sym->value : parseNumericLiteral(src);
    e.ldy_imm(0); e.lda_ind_z(addr, false); e.pha(); e.ldy_imm(1); e.lda_ind_z(addr, false); e.tax(); e.pla();
}

void AssemblerSimulatedOps::emitFillCode(AssemblerParser* parser, M65Emitter& e, int tokenIndex, const std::string& scopePrefix, bool forceStack) {
    int idx = tokenIndex; if (idx < 0 || idx >= (int)parser->tokens.size()) return;
    std::string destReg; bool destIsRegister = false, destIsIndirect = false; uint32_t destVal = 0; bool destIsStack = forceStack;
    if (destIsStack) {
        if (parser->tokens[idx].type == AssemblerTokenType::HASH) idx++;
        if (parser->tokens[idx].type == AssemblerTokenType::REGISTER) { destReg = "." + parser->tokens[idx++].value; destIsRegister = true; }
        else { auto ast = parseExprAST(parser->tokens, idx, parser->symbolTable, scopePrefix); if (ast) destVal = ast->getValue(parser); }
    } else {
        if (parser->tokens[idx].type == AssemblerTokenType::REGISTER) { destReg = "." + parser->tokens[idx++].value; destIsRegister = true; }
        else if (parser->tokens[idx].type == AssemblerTokenType::OPEN_PAREN) { idx++; destReg = parser->tokens[idx++].value; if (parser->tokens[idx-1].type == AssemblerTokenType::REGISTER) destReg = "." + destReg; destIsIndirect = true; if (idx < (int)parser->tokens.size() && parser->tokens[idx].type == AssemblerTokenType::CLOSE_BRACKET) idx++; }
        else {
            uint32_t offset = 0;
            if (parser->isStackRelativeOperand(idx, offset, scopePrefix)) { destIsStack = true; destVal = offset; parseExprAST(parser->tokens, idx, parser->symbolTable, scopePrefix); if (idx < (int)parser->tokens.size() && parser->tokens[idx].type == AssemblerTokenType::COMMA) idx++; if (idx < (int)parser->tokens.size() && (parser->tokens[idx].value == "s" || parser->tokens[idx].value == "S")) idx++; }
            else { auto ast = parseExprAST(parser->tokens, idx, parser->symbolTable, scopePrefix); if (ast) destVal = ast->getValue(parser); }
        }
    }
    if (idx < (int)parser->tokens.size() && parser->tokens[idx].type == AssemblerTokenType::COMMA) idx++;
    std::string lenReg; bool lenIsRegister = false; uint32_t lenVal = 0;
    if (idx < (int)parser->tokens.size()) {
        if (parser->tokens[idx].type == AssemblerTokenType::HASH) idx++;
        if (parser->tokens[idx].type == AssemblerTokenType::REGISTER) { lenReg = "." + parser->tokens[idx++].value; lenIsRegister = true; }
        else { auto ast = parseExprAST(parser->tokens, idx, parser->symbolTable, scopePrefix); if (ast) lenVal = ast->getValue(parser); }
    }
    e.pha(); e.phw_imm(0); e.lda_imm(0); e.pha();
    if (destIsStack) { e.tsx(); e.txa(); e.clc(); if (destIsRegister) { if (destReg == ".X") {} else if (destReg == ".Y") { e.tya(); e.tax(); } else if (destReg == ".Z") { e.tza(); e.tax(); } e.txa(); } else e.adc_imm(destVal); e.pha(); e.lda_imm(1); e.adc_imm(0); e.pha(); }
    else if (destIsRegister) { if (destReg == ".AX") { e.phx(); e.pha(); } else if (destReg == ".AY") { e.phy(); e.pha(); } else if (destReg == ".AZ") { e.phz(); e.pha(); } else if (destReg == ".XY") { e.phy(); e.phx(); } }
    else if (destIsIndirect) { Symbol* sym = parser->resolveSymbol(destReg, scopePrefix); uint32_t addr = sym ? sym->value : parseNumericLiteral(destReg); e.lda_zp(addr + 1); e.pha(); e.lda_zp(addr); e.pha(); }
    else { e.lda_imm(destVal >> 8); e.pha(); e.lda_imm(destVal & 0xFF); e.pha(); }
    e.lda_imm(0); e.pha(); e.lda_imm(1); e.pha(); e.tsx(); e.txa(); e.clc(); e.adc_imm(8); e.pha();
    if (lenIsRegister) { if (lenReg == ".XY") { e.phy(); e.phx(); } else { e.lda_imm(0); e.pha(); if (lenReg == ".X") e.phx(); else if (lenReg == ".Y") e.phy(); else if (lenReg == ".Z") e.phz(); else { e.tsx(); e.txa(); e.clc(); e.adc_imm(11); e.tax(); e.lda_abs_x(0x0100); e.pha(); } } }
    else { e.lda_imm(lenVal >> 8); e.pha(); e.lda_imm(lenVal & 0xFF); e.pha(); }
    e.lda_imm(0x03); e.pha(); e.tsx(); e.txa(); e.clc(); e.adc_imm(1); e.sta_abs(0xD701); e.lda_imm(1); e.sta_abs(0xD702); e.stz_abs(0xD703); e.stz_abs(0xD700);
    e.tsx(); e.txa(); e.clc(); e.adc_imm(12); e.tax(); e.txs(); e.pla();
}

void AssemblerSimulatedOps::emitMoveCode(AssemblerParser* parser, M65Emitter& e, int tokenIndex, const std::string& scopePrefix, bool forceStack) {
    int idx = tokenIndex; if (idx < 0 || idx >= (int)parser->tokens.size()) return;
    struct Operand { bool isAZ = false, isAbsolute = false, isStack = false; uint32_t value = 0; };
    auto parseMoveOp = [&](int& curIdx) -> Operand {
        Operand op; if (parser->tokens[curIdx].type == AssemblerTokenType::REGISTER && parser->tokens[curIdx].value == "AZ") { op.isAZ = true; curIdx++; }
        else { uint32_t offset = 0; if (parser->isStackRelativeOperand(curIdx, offset, scopePrefix)) { op.isStack = true; op.value = offset; parseExprAST(parser->tokens, curIdx, parser->symbolTable, scopePrefix); if (curIdx < (int)parser->tokens.size() && parser->tokens[curIdx].type == AssemblerTokenType::COMMA) curIdx++; if (curIdx < (int)parser->tokens.size() && (parser->tokens[curIdx].value == "s" || parser->tokens[curIdx].value == "S")) curIdx++; }
        else { auto ast = parseExprAST(parser->tokens, curIdx, parser->symbolTable, scopePrefix); if (ast) { op.isAbsolute = true; op.value = ast->getValue(parser); } } } return op;
    };
    Operand src = parseMoveOp(idx); if (idx < (int)parser->tokens.size() && parser->tokens[idx].type == AssemblerTokenType::COMMA) idx++;
    Operand dest = parseMoveOp(idx);
    if (forceStack && !src.isStack && !dest.isStack) { if (src.isAbsolute && !dest.isAbsolute && !dest.isAZ) src.isStack = true; else dest.isStack = true; }
    e.pha(); e.phw_imm(0); e.lda_imm(0); e.pha();
    if (dest.isAZ) { e.phz(); e.pha(); } else if (dest.isStack) { e.tsx(); e.txa(); e.clc(); e.adc_imm(dest.value); e.pha(); e.lda_imm(1); e.adc_imm(0); e.pha(); }
    else { e.lda_imm(dest.value >> 8); e.pha(); e.lda_imm(dest.value & 0xFF); e.pha(); }
    e.lda_imm(0); e.pha();
    if (src.isAZ) { e.phz(); e.pha(); } else if (src.isStack) { e.tsx(); e.txa(); e.clc(); e.adc_imm(src.value); e.pha(); e.lda_imm(1); e.adc_imm(0); e.pha(); }
    else { e.lda_imm(src.value >> 8); e.pha(); e.lda_imm(src.value & 0xFF); e.pha(); }
    e.phy(); e.phx(); e.lda_imm(0x00); e.pha();
    e.tsx(); e.txa(); e.clc(); e.adc_imm(1); e.sta_abs(0xD701); e.lda_imm(1); e.sta_abs(0xD702); e.stz_abs(0xD703); e.stz_abs(0xD700);
    e.tsx(); e.txa(); e.clc(); e.adc_imm(12); e.tax(); e.txs(); e.pla();
}

void AssemblerSimulatedOps::emitFlatMemoryCode(AssemblerParser* parser, M65Emitter& e, const std::string& mnemonic, int tokenIndex, const std::string& scopePrefix) {
    std::string op = parser->tokens[tokenIndex].value;
    Symbol* sym = parser->resolveSymbol(op, scopePrefix); uint32_t addr = sym ? sym->value : parseNumericLiteral(op);
    e.eom();
    if (mnemonic == "LDW.F") { e.lda_abs(addr); e.eom(); e.ldx_abs(addr + 1); }
    else if (mnemonic == "STW.F") { e.sta_abs(addr); e.eom(); e.stx_abs(addr + 1); }
    else if (mnemonic == "INC.F") e.inc_abs(addr); else if (mnemonic == "DEC.F") e.dec_abs(addr);
}

void AssemblerSimulatedOps::emitPHWStackCode(AssemblerParser* parser, M65Emitter& e, int tokenIndex, const std::string& scopePrefix) {
    uint32_t offset = parser->evaluateExpressionAt(tokenIndex, scopePrefix);
    e.tsx(); e.lda_abs_x(0x0102 + offset); e.pha(); e.lda_abs_x(0x0101 + offset); e.pha();
}

void AssemblerSimulatedOps::emitASWCode(AssemblerParser* parser, M65Emitter& e, const std::string& dest, int /*tokenIndex*/, const std::string& scopePrefix) {
    std::string DEST = dest; if (!DEST.empty() && DEST[0] != '.') DEST = "." + DEST;
    std::transform(DEST.begin(), DEST.end(), DEST.begin(), ::toupper);
    if (DEST == ".AX") {
        e.asl_a(); e.txa(); e.rol_a(); e.tax();
    } else {
        // Load into AX, perform shift, store back
        Symbol* sym = parser->resolveSymbol(dest, scopePrefix);
        if (!sym) throw std::runtime_error("Undefined symbol for ASW: " + dest);
        uint32_t addr = sym->value;
        e.lda_abs(addr); e.txa(); e.ldx_abs(addr + 1);
        e.asl_a(); e.txa(); e.rol_a(); e.tax();
        e.sta_abs(addr); e.stx_abs(addr + 1);
    }
}

void AssemblerSimulatedOps::emitROWCode(AssemblerParser* parser, M65Emitter& e, const std::string& dest, int /*tokenIndex*/, const std::string& scopePrefix) {
    std::string DEST = dest; if (!DEST.empty() && DEST[0] != '.') DEST = "." + DEST;
    std::transform(DEST.begin(), DEST.end(), DEST.begin(), ::toupper);
    if (DEST == ".AX") {
        e.ror_a(); e.txa(); e.ror_a(); e.tax();
    } else {
        // Load into AX, perform rotation, store back
        Symbol* sym = parser->resolveSymbol(dest, scopePrefix);
        if (!sym) throw std::runtime_error("Undefined symbol for ROW: " + dest);
        uint32_t addr = sym->value;
        e.lda_abs(addr); e.txa(); e.ldx_abs(addr + 1);
        e.ror_a(); e.txa(); e.ror_a(); e.tax();
        e.sta_abs(addr); e.stx_abs(addr + 1);
    }
}

void AssemblerSimulatedOps::emitASR16Code(AssemblerParser* parser, M65Emitter& e, const std::string& dest, int /*tokenIndex*/, const std::string& scopePrefix) {
    std::string DEST = dest; if (!DEST.empty() && DEST[0] != '.') DEST = "." + DEST;
    std::transform(DEST.begin(), DEST.end(), DEST.begin(), ::toupper);
    if (DEST == ".AX") {
        e.clc(); // Clear carry for logical shift
        e.ror_a(); e.txa(); e.ror_a(); e.tax();
    } else {
        // Load into AX, perform shift, store back
        Symbol* sym = parser->resolveSymbol(dest, scopePrefix);
        if (!sym) throw std::runtime_error("Undefined symbol for ASR.16: " + dest);
        uint32_t addr = sym->value;
        e.lda_abs(addr); e.txa(); e.ldx_abs(addr + 1);
        e.clc(); // Clear carry for logical shift
        e.ror_a(); e.txa(); e.ror_a(); e.tax();
        e.sta_abs(addr); e.stx_abs(addr + 1);
    }
}
