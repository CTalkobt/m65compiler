#include "AssemblerParser.hpp"
#include "AssemblerExpression.hpp"
#include "AssemblerOptimizer.hpp"
#include "AssemblerSimulatedOps.hpp"
#include "AssemblerGenerator.hpp"
#include "M65Emitter.hpp"
#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <memory>
#include <cmath>

// --- Support Functions ---

static uint32_t parseNumericLiteral(const std::string& literal) {
    if (literal.empty()) throw std::runtime_error("Empty numeric literal");
    try {
        if (literal[0] == '$') return std::stoul(literal.substr(1), nullptr, 16);
        if (literal[0] == '%') return std::stoul(literal.substr(1), nullptr, 2);
        return std::stoul(literal);
    } catch (...) {
        throw std::runtime_error("Invalid numeric literal: '" + literal + "'");
    }
}

// --- AssemblerParser Implementation ---

AssemblerParser::AssemblerParser(const std::vector<AssemblerToken>& tokens) : tokens(tokens), pos(0), pc(0) {
    switchSegment("default");
}

AssemblerParser::AssemblerParser(const std::vector<AssemblerToken>& tokens, const std::map<std::string, uint32_t>& predefinedSymbols) : tokens(tokens), pos(0), pc(0), nextScopeId(0) {
    for (const auto& [name, val] : predefinedSymbols) symbolTable[name] = {val, false, 2, false, 0, false, 0};
    switchSegment("default");
}

void AssemblerParser::switchSegment(const std::string& name) {
    if (currentSegment) {
        currentSegment->pc = pc;
    }
    if (segments.find(name) == segments.end()) {
        auto seg = std::make_shared<Segment>();
        seg->name = name;
        seg->pc = 0;
        seg->hasOrg = false;
        segments[name] = seg;
        segmentOrder.push_back(seg);
    }
    currentSegment = segments[name];
    pc = currentSegment->pc;
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
        if (current.size() < 2) { current = ""; continue; }
        size_t p = current.find_last_of(':', current.size() - 2);
        if (p == std::string::npos) current = "";
        else current = current.substr(0, p + 1);
    }
    return nullptr;
}

uint32_t AssemblerParser::getZPStart() const {
    if (symbolTable.count("cc45.zeroPageStart")) {
        return symbolTable.at("cc45.zeroPageStart").value;
    }
    return 0x02;
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

bool AssemblerParser::optimize() { return AssemblerOptimizer::optimize(this); }

void AssemblerParser::emitExpressionCode(std::vector<uint8_t>& binary, const std::string& target, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    AssemblerSimulatedOps::emitExpressionCode(this, e, target, tokenIndex, scopePrefix);
}

void AssemblerParser::emitMulCode(std::vector<uint8_t>& binary, int width, const std::string& dest, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    AssemblerSimulatedOps::emitMulCode(this, e, width, dest, tokenIndex, scopePrefix);
}

void AssemblerParser::emitDivCode(std::vector<uint8_t>& binary, int width, const std::string& dest, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    AssemblerSimulatedOps::emitDivCode(this, e, width, dest, tokenIndex, scopePrefix);
}

void AssemblerParser::emitStackIncDecCode(std::vector<uint8_t>& binary, bool isInc, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    AssemblerSimulatedOps::emitStackIncDecCode(this, e, isInc, tokenIndex, scopePrefix);
}

void AssemblerParser::emitStackIncDec8Code(std::vector<uint8_t>& binary, bool isInc, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    AssemblerSimulatedOps::emitStackIncDec8Code(this, e, isInc, tokenIndex, scopePrefix);
}

void AssemblerParser::emitAddSub16Code(std::vector<uint8_t>& binary, bool isAdd, const std::string& dest, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    AssemblerSimulatedOps::emitAddSub16Code(this, e, isAdd, dest, tokenIndex, scopePrefix);
}

void AssemblerParser::emitBitwise16Code(std::vector<uint8_t>& binary, const std::string& mnemonic, const std::string& dest, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    AssemblerSimulatedOps::emitBitwise16Code(this, e, mnemonic, dest, tokenIndex, scopePrefix);
}

void AssemblerParser::emitCMP16Code(std::vector<uint8_t>& binary, const std::string& src1, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    AssemblerSimulatedOps::emitCMP16Code(this, e, src1, tokenIndex, scopePrefix);
}

void AssemblerParser::emitCMP_S16Code(std::vector<uint8_t>& binary, const std::string& src1, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    AssemblerSimulatedOps::emitCMP_S16Code(this, e, src1, tokenIndex, scopePrefix);
}

void AssemblerParser::emitLDWCode(std::vector<uint8_t>& binary, const std::string& dest, int tokenIndex, const std::string& scopePrefix, bool forceStack) {
    M65Emitter e(binary, getZPStart());
    AssemblerSimulatedOps::emitLDWCode(this, e, dest, tokenIndex, scopePrefix, forceStack);
}

void AssemblerParser::emitSTWCode(std::vector<uint8_t>& binary, const std::string& src, int tokenIndex, const std::string& scopePrefix, bool forceStack) {
    M65Emitter e(binary, getZPStart());
    AssemblerSimulatedOps::emitSTWCode(this, e, src, tokenIndex, scopePrefix, forceStack);
}

void AssemblerParser::emitLDX_StackCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    AssemblerSimulatedOps::emitLDX_StackCode(this, e, tokenIndex, scopePrefix);
}

void AssemblerParser::emitLDY_StackCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    AssemblerSimulatedOps::emitLDY_StackCode(this, e, tokenIndex, scopePrefix);
}

void AssemblerParser::emitLDZ_StackCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    AssemblerSimulatedOps::emitLDZ_StackCode(this, e, tokenIndex, scopePrefix);
}

void AssemblerParser::emitSTX_StackCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    AssemblerSimulatedOps::emitSTX_StackCode(this, e, tokenIndex, scopePrefix);
}

void AssemblerParser::emitSTY_StackCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    AssemblerSimulatedOps::emitSTY_StackCode(this, e, tokenIndex, scopePrefix);
}

void AssemblerParser::emitSTZ_StackCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    AssemblerSimulatedOps::emitSTZ_StackCode(this, e, tokenIndex, scopePrefix);
}

void AssemblerParser::emitSwapCode(std::vector<uint8_t>& binary, const std::string& r1, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    AssemblerSimulatedOps::emitSwapCode(this, e, r1, tokenIndex, scopePrefix);
}

void AssemblerParser::emitZeroCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    AssemblerSimulatedOps::emitZeroCode(this, e, tokenIndex, scopePrefix);
}

void AssemblerParser::emitNegNot16Code(std::vector<uint8_t>& binary, bool isNeg, const std::string& operand, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    AssemblerSimulatedOps::emitNegNot16Code(this, e, isNeg, operand, tokenIndex, scopePrefix);
}

void AssemblerParser::emitABS16Code(std::vector<uint8_t>& binary, const std::string& dest, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    AssemblerSimulatedOps::emitABS16Code(this, e, dest, tokenIndex, scopePrefix);
}

void AssemblerParser::emitChkZeroCode(std::vector<uint8_t>& binary, bool is16, bool isInverse, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    AssemblerSimulatedOps::emitChkZeroCode(this, e, is16, isInverse, tokenIndex, scopePrefix);
}

void AssemblerParser::emitBranch16Code(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    AssemblerSimulatedOps::emitBranch16Code(this, e, tokenIndex, scopePrefix);
}

void AssemblerParser::emitSelectCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    AssemblerSimulatedOps::emitSelectCode(this, e, tokenIndex, scopePrefix);
}

void AssemblerParser::emitPtrStackCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    AssemblerSimulatedOps::emitPtrStackCode(this, e, tokenIndex, scopePrefix);
}

void AssemblerParser::emitPtrDerefCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    AssemblerSimulatedOps::emitPtrDerefCode(this, e, tokenIndex, scopePrefix);
}

void AssemblerParser::emitFillCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix, bool forceStack) {
    M65Emitter e(binary, getZPStart());
    AssemblerSimulatedOps::emitFillCode(this, e, tokenIndex, scopePrefix, forceStack);
}

void AssemblerParser::emitMoveCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix, bool forceStack) {
    M65Emitter e(binary, getZPStart());
    AssemblerSimulatedOps::emitMoveCode(this, e, tokenIndex, scopePrefix, forceStack);
}

void AssemblerParser::emitFlatMemoryCode(std::vector<uint8_t>& binary, const std::string& mnemonic, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    AssemblerSimulatedOps::emitFlatMemoryCode(this, e, mnemonic, tokenIndex, scopePrefix);
}

void AssemblerParser::emitPHWStackCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    AssemblerSimulatedOps::emitPHWStackCode(this, e, tokenIndex, scopePrefix);
}

void AssemblerParser::pass1() {
    segments.clear();
    segmentOrder.clear();
    segmentStack.clear();
    switchSegment("default");
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
    };

    while (pos < tokens.size()) {
        while (match(AssemblerTokenType::NEWLINE));
        if (peek().type == AssemblerTokenType::END_OF_FILE) break;

        auto stmt = std::make_unique<Statement>();
        stmt->address = pc;
        stmt->line = peek().line;
        stmt->scopePrefix = currentScopePrefix();
        stmt->segmentName = currentSegment->name;

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
            segmentStack.push_back("");
            stmt->size = 0;
            statements.push_back(std::move(stmt));
            continue;
        }
        else if (match(AssemblerTokenType::CLOSE_CURLY)) {
            if (!scopeStack.empty()) scopeStack.pop_back();
            if (!segmentStack.empty()) {
                std::string prevSeg = segmentStack.back();
                segmentStack.pop_back();
                if (!prevSeg.empty()) switchSegment(prevSeg);
            }
            stmt->size = 0;
            statements.push_back(std::move(stmt));
            continue;
        }
        else if (match(AssemblerTokenType::DIRECTIVE)) {
            stmt->type = Statement::DIRECTIVE;
            stmt->dir.name = tokens[pos-1].value;

            if (stmt->dir.name == "segment" || stmt->dir.name == "code" || stmt->dir.name == "text" || stmt->dir.name == "data" || stmt->dir.name == "bss") {
                if (stmt->dir.name != "text" || peek().type != AssemblerTokenType::STRING_LITERAL) {
                    std::string newSeg;
                    if (stmt->dir.name == "segment") {
                        if (peek().type == AssemblerTokenType::STRING_LITERAL) newSeg = advance().value;
                        else newSeg = expect(AssemblerTokenType::IDENTIFIER, "Expected segment name").value;
                    } else if (stmt->dir.name == "text") newSeg = "code";
                    else newSeg = stmt->dir.name;
                    std::string oldSeg = currentSegment->name;
                    switchSegment(newSeg);
                    if (match(AssemblerTokenType::OPEN_CURLY)) {
                        scopeStack.push_back("_S" + std::to_string(nextScopeId++));
                        segmentStack.push_back(oldSeg);
                    }
                    stmt->size = 0;
                    stmt->address = pc;
                    stmt->segmentName = currentSegment->name;
                    statements.push_back(std::move(stmt));
                    continue;
                }
            }

            if (stmt->dir.name == "segmentOrder") {
                while (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) {
                    if (peek().type == AssemblerTokenType::COMMA) { advance(); continue; }
                    requestedSegmentOrder.push_back(advance().value);
                }
                stmt->size = 0;
                statements.push_back(std::move(stmt));
                continue;
            }

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
                stmt->size = 13;
            }
            else {
                stmt->dir.tokenIndex = (int)pos;
                while (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) {
                    if (peek().type == AssemblerTokenType::COMMA) { advance(); continue; }
                    stmt->dir.arguments.push_back(advance().value);
                }
                if (stmt->dir.name == "org") {
                    if (!stmt->dir.arguments.empty()) pc = parseNumericLiteral(stmt->dir.arguments[0]);
                    stmt->address = pc;
                    stmt->size = 0;
                }
                else if (stmt->dir.name == "cpu") stmt->size = 0;
                else stmt->size = calculateDirectiveSize(stmt->dir, pc);
            }
        }
        else if (peek().type == AssemblerTokenType::STAR && pos + 1 < tokens.size() && tokens[pos+1].type == AssemblerTokenType::EQUALS) {
            advance(); advance();
            stmt->type = Statement::DIRECTIVE;
            stmt->dir.name = "org";
            stmt->dir.arguments.push_back(advance().value);
            if (!stmt->dir.arguments.empty()) pc = parseNumericLiteral(stmt->dir.arguments[0]);
            stmt->address = pc;
            stmt->size = 0;
        }
        else if (match(AssemblerTokenType::INSTRUCTION)) {
            stmt->type = Statement::INSTRUCTION;
            std::string fullMnemonic = tokens[pos-1].value;
            std::transform(fullMnemonic.begin(), fullMnemonic.end(), fullMnemonic.begin(), ::tolower);
            stmt->instr.mnemonic = fullMnemonic;

            if (fullMnemonic == "add.16") stmt->type = Statement::ADD16;
            else if (fullMnemonic == "sub.16") stmt->type = Statement::SUB16;
            else if (fullMnemonic == "add.s16") stmt->type = Statement::ADDS16;
            else if (fullMnemonic == "sub.s16") stmt->type = Statement::SUBS16;
            else if (fullMnemonic == "and.16") stmt->type = Statement::AND16;
            else if (fullMnemonic == "ora.16") stmt->type = Statement::ORA16;
            else if (fullMnemonic == "eor.16") stmt->type = Statement::EOR16;
            else if (fullMnemonic == "cmp.16" || fullMnemonic == "cpw") stmt->type = Statement::CMP16;
            else if (fullMnemonic == "cmp.s16") stmt->type = Statement::CMP_S16;
            else if (fullMnemonic == "ldw" || fullMnemonic == "ldw.sp") stmt->type = Statement::LDW;
            else if (fullMnemonic == "stw" || fullMnemonic == "stw.sp") stmt->type = Statement::STW;
            else if (fullMnemonic == "fill" || fullMnemonic == "fill.sp") stmt->type = Statement::FILL;
            else if (fullMnemonic == "move" || fullMnemonic == "move.sp") stmt->type = Statement::COPY;
            else if (fullMnemonic == "swap") stmt->type = Statement::SWAP;
            else if (fullMnemonic == "neg.16") stmt->type = Statement::NEG16;
            else if (fullMnemonic == "not.16") stmt->type = Statement::NOT16;
            else if (fullMnemonic == "neg.s16") stmt->type = Statement::NEG_S16;
            else if (fullMnemonic == "abs.16") stmt->type = Statement::ABS16;
            else if (fullMnemonic == "abs.s16") stmt->type = Statement::ABS_S16;
            else if (fullMnemonic == "chkzero.8") stmt->type = Statement::CHKZERO8;
            else if (fullMnemonic == "chkzero.16") stmt->type = Statement::CHKZERO16;
            else if (fullMnemonic == "chknonzero.8") stmt->type = Statement::CHKNONZERO8;
            else if (fullMnemonic == "chknonzero.16") stmt->type = Statement::CHKNONZERO16;
            else if (fullMnemonic == "branch.16") stmt->type = Statement::BRANCH16;
            else if (fullMnemonic == "select") stmt->type = Statement::SELECT;
            else if (fullMnemonic == "ptrstack") stmt->type = Statement::PTRSTACK;
            else if (fullMnemonic == "ptrderef") stmt->type = Statement::PTRDEREF;
            else if (fullMnemonic == "ldw.f") stmt->type = Statement::LDWF;
            else if (fullMnemonic == "stw.f") stmt->type = Statement::STWF;
            else if (fullMnemonic == "inc.f") stmt->type = Statement::INCF;
            else if (fullMnemonic == "dec.f") stmt->type = Statement::DECF;
            else if (fullMnemonic == "asw") stmt->type = Statement::ASW;
            else if (fullMnemonic == "row") stmt->type = Statement::ROW;
            else if (fullMnemonic == "lsl.16") stmt->type = Statement::LSL16;
            else if (fullMnemonic == "lsr.16") stmt->type = Statement::LSR16;
            else if (fullMnemonic == "rol.16") stmt->type = Statement::ROL16;
            else if (fullMnemonic == "ror.16") stmt->type = Statement::ROR16;
            else if (fullMnemonic == "asr.16") stmt->type = Statement::ASR16;
            else if (fullMnemonic == "lsl.s16") stmt->type = Statement::LSL_S16;
            else if (fullMnemonic == "lsr.s16") stmt->type = Statement::LSR_S16;
            else if (fullMnemonic == "rol.s16") stmt->type = Statement::ROL_S16;
            else if (fullMnemonic == "ror.s16") stmt->type = Statement::ROR_S16;
            else if (fullMnemonic == "asr.s16") stmt->type = Statement::ASR_S16;
            else if (fullMnemonic == "sxt.8") {
                stmt->type = Statement::SXT8;
                if (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) {
                     // Optionally consume 'a' if present? C compilers sometimes use 'sxt.8 a' or similar. 
                     // For now just keep it simple.
                }
            }
            else if (fullMnemonic == "push") stmt->type = Statement::PUSH;
            else if (fullMnemonic == "pop") stmt->type = Statement::POP;

            if (stmt->instr.mnemonic == "expr") {
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
            else if (stmt->instr.mnemonic.substr(0, 3) == "mul" || stmt->instr.mnemonic.substr(0, 3) == "div") {
                stmt->type = (stmt->instr.mnemonic.substr(0, 3) == "mul") ? Statement::MUL : Statement::DIV;
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
            else if (stmt->instr.mnemonic == "ldax" || stmt->instr.mnemonic == "lday" || stmt->instr.mnemonic == "ldaz" ||
                     stmt->instr.mnemonic == "stax" || stmt->instr.mnemonic == "stay" || stmt->instr.mnemonic == "staz") {
                std::string m = stmt->instr.mnemonic;
                std::string reg = "." + m.substr(2);
                bool isLoad = (m.substr(0, 2) == "ld");
                stmt->type = isLoad ? Statement::LDW : Statement::STW;
                stmt->instr.operand = reg;
                stmt->exprTokenIndex = (int)pos;
                while (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) advance();
                std::vector<uint8_t> d;
                if (isLoad) emitLDWCode(d, reg, stmt->exprTokenIndex, stmt->scopePrefix);
                else emitSTWCode(d, reg, stmt->exprTokenIndex, stmt->scopePrefix);
                stmt->size = d.size();
            }
            else if (stmt->isSimulatedOp()) {
                if (stmt->type == Statement::SXT8) {
                    // No operand
                } else {
                    stmt->instr.operandTokenIndex = (int)pos;
                    stmt->exprTokenIndex = (int)pos; // Initialize exprTokenIndex too
                    if (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) {
                        if (peek().type == AssemblerTokenType::REGISTER) {
                            stmt->instr.operand = "." + advance().value;
                        } else {
                            if (peek().type == AssemblerTokenType::HASH) {
                                std::string immStr = advance().value;
                                stmt->instr.operandTokenIndex = (int)pos - 1; // HASH
                                stmt->exprTokenIndex = (int)pos;
                                std::string valPart;
                                int tempPos = (int)pos;
                                while(tempPos < (int)tokens.size() && tokens[tempPos].type != AssemblerTokenType::NEWLINE && tokens[tempPos].type != AssemblerTokenType::END_OF_FILE && tokens[tempPos].type != AssemblerTokenType::COMMA) {
                                    valPart += tokens[tempPos].value;
                                    tempPos++;
                                }
                                stmt->instr.operand = immStr + valPart;
                                while(peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE && peek().type != AssemblerTokenType::COMMA) advance();
                            } else {
                                int idx = (int)pos;
                                auto ast = parseExprAST(tokens, idx, symbolTable, stmt->scopePrefix);
                                if (ast) {
                                    if (auto* v = dynamic_cast<VariableNode*>(ast.get())) stmt->instr.operand = v->name;
                                    else stmt->instr.operand = "EXPR";
                                }
                                pos = idx;
                            }
                        }
                        if (match(AssemblerTokenType::COMMA)) {
                            stmt->exprTokenIndex = (int)pos;
                            while (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) advance();
                        }
                    }
                }
                std::vector<uint8_t> d;
                if (stmt->type == Statement::ADD16 || stmt->type == Statement::SUB16 || stmt->type == Statement::ADDS16 || stmt->type == Statement::SUBS16) emitAddSub16Code(d, stmt->type == Statement::ADD16 || stmt->type == Statement::ADDS16, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::AND16 || stmt->type == Statement::ORA16 || stmt->type == Statement::EOR16) emitBitwise16Code(d, stmt->instr.mnemonic, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::CMP16) emitCMP16Code(d, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::CMP_S16) emitCMP_S16Code(d, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::LDW) emitLDWCode(d, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix, stmt->instr.mnemonic == "ldw.sp");
                else if (stmt->type == Statement::STW) emitSTWCode(d, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix, stmt->instr.mnemonic == "stw.sp");
                else if (stmt->type == Statement::FILL) emitFillCode(d, stmt->instr.operandTokenIndex, stmt->scopePrefix, stmt->instr.mnemonic == "fill.sp");
                else if (stmt->type == Statement::COPY) emitMoveCode(d, stmt->instr.operandTokenIndex, stmt->scopePrefix, stmt->instr.mnemonic == "move.sp");
                else if (stmt->type == Statement::NEG16 || stmt->type == Statement::NOT16 || stmt->type == Statement::NEG_S16) emitNegNot16Code(d, stmt->type == Statement::NEG16 || stmt->type == Statement::NEG_S16, stmt->instr.operand, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::ABS16 || stmt->type == Statement::ABS_S16) emitABS16Code(d, stmt->instr.operand, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::CHKZERO8) emitChkZeroCode(d, false, false, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::CHKZERO16) emitChkZeroCode(d, true, false, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::CHKNONZERO8) emitChkZeroCode(d, false, true, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::CHKNONZERO16) emitChkZeroCode(d, true, true, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::BRANCH16) emitBranch16Code(d, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::SELECT) emitSelectCode(d, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::PHW_STACK) emitPHWStackCode(d, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::LDX_STACK) emitLDX_StackCode(d, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::LDY_STACK) emitLDY_StackCode(d, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::LDZ_STACK) emitLDZ_StackCode(d, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::STX_STACK) emitSTX_StackCode(d, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::STY_STACK) emitSTY_StackCode(d, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::STZ_STACK) emitSTZ_StackCode(d, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::PTRDEREF) emitPtrDerefCode(d, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::LDWF || stmt->type == Statement::STWF || stmt->type == Statement::INCF || stmt->type == Statement::DECF) emitFlatMemoryCode(d, stmt->instr.mnemonic, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::ASW) emitASWCode(d, stmt->instr.operand, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::ROW) emitROWCode(d, stmt->instr.operand, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::LSL16 || stmt->type == Statement::LSL_S16) emitLSL16Code(d, stmt->instr.operand, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::LSR16 || stmt->type == Statement::LSR_S16) emitLSR16Code(d, stmt->instr.operand, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::ROL16 || stmt->type == Statement::ROL_S16) emitROL16Code(d, stmt->instr.operand, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::ROR16 || stmt->type == Statement::ROR_S16) emitROR16Code(d, stmt->instr.operand, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::ASR16 || stmt->type == Statement::ASR_S16) emitASR16Code(d, stmt->instr.operand, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::SXT8) emitSXT8Code(d, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::PUSH || stmt->type == Statement::POP) emitPushPopCode(d, stmt->type == Statement::PUSH, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                if (stmt->type != Statement::INSTRUCTION) stmt->size = d.size();
            }
            else {
                size_t dotPos = fullMnemonic.find('.');
                if (dotPos != std::string::npos) {
                    stmt->instr.mnemonic = fullMnemonic.substr(0, dotPos);
                    std::string suffix = fullMnemonic.substr(dotPos + 1);
                    stmt->instr.forceMode = true;
                    if (suffix == "imm") {
                        if (stmt->instr.mnemonic == "phw") stmt->instr.mode = AddressingMode::IMMEDIATE16;
                        else stmt->instr.mode = AddressingMode::IMMEDIATE;
                    }
                    else if (suffix == "zp") stmt->instr.mode = AddressingMode::BASE_PAGE;
                    else if (suffix == "abs") stmt->instr.mode = AddressingMode::ABSOLUTE;
                    else if (suffix == "ind") stmt->instr.mode = AddressingMode::INDIRECT;
                    else if (suffix == "xind") stmt->instr.mode = AddressingMode::BASE_PAGE_X_INDIRECT;
                    else if (suffix == "indy") stmt->instr.mode = AddressingMode::BASE_PAGE_INDIRECT_Y;
                    else if (suffix == "indz") stmt->instr.mode = AddressingMode::BASE_PAGE_INDIRECT_Z;
                    else if (suffix == "accum" || suffix == "a") stmt->instr.mode = AddressingMode::ACCUMULATOR;
                    else if (suffix == "s") stmt->instr.mode = AddressingMode::STACK_RELATIVE;
                    else { stmt->instr.mnemonic = fullMnemonic; stmt->instr.forceMode = false; }
                }
                if (stmt->instr.mnemonic == "nop") throw std::runtime_error("nop disallowed");
                if (stmt->instr.mnemonic == "proc") {
                    std::string pN = advance().value;
                    stmt->label = pN;
                    symbolTable[pN] = {pc, true, 2, false, 0, false, 0};
                    auto ctx = std::make_shared<ProcContext>();
                    ctx->name = pN; ctx->totalParamSize = 0;
                    std::vector<std::pair<std::string, int>> args;
                    while (match(AssemblerTokenType::COMMA)) {
                        bool isB = false;
                        if (peek().type == AssemblerTokenType::IDENTIFIER && (peek().value == "B" || peek().value == "W")) {
                            isB = (advance().value == "B"); match(AssemblerTokenType::HASH);
                        }
                        args.push_back({advance().value, isB ? 1 : 2});
                        ctx->totalParamSize += args.back().second;
                    }
                    scopeStack.push_back(pN); stmt->scopePrefix = currentScopePrefix();
                    int cO = 2;
                    for (int i = (int)args.size() - 1; i >= 0; --i) {
                        std::string scA = stmt->scopePrefix + args[i].first;
                        std::string scAN = stmt->scopePrefix + "ARG" + std::to_string(i + 1);
                        ctx->localArgs[args[i].first] = cO; ctx->localArgs["ARG" + std::to_string(i + 1)] = cO;
                        symbolTable[scA] = {(uint32_t)cO, false, args[i].second, true, (uint32_t)cO, false, cO};
                        symbolTable[scAN] = {(uint32_t)cO, false, args[i].second, true, (uint32_t)cO, false, cO};
                        cO += args[i].second;
                    }
                    procedures[pc] = ctx; pass1ProcStack.push_back(currentProc);
                    currentProc = ctx; stmt->procCtx = ctx; stmt->size = 0;
                }
                else if (stmt->instr.mnemonic == "endproc") {
                    if (currentProc) {
                        stmt->instr.procParamSize = currentProc->totalParamSize;
                        currentProc = pass1ProcStack.empty() ? nullptr : pass1ProcStack.back();
                        if (!pass1ProcStack.empty()) pass1ProcStack.pop_back();
                    }
                    if (!scopeStack.empty()) scopeStack.pop_back();
                    stmt->size = 2;
                }
                else if (stmt->instr.mnemonic == "call") {
                    stmt->instr.operand = advance().value;
                    while (match(AssemblerTokenType::COMMA)) {
                        if (match(AssemblerTokenType::HASH)) stmt->instr.callArgs.push_back(std::string("#") + advance().value);
                        else if (peek().type == AssemblerTokenType::IDENTIFIER && (peek().value == "B" || peek().value == "W")) {
                            std::string p = advance().value; match(AssemblerTokenType::HASH);
                            stmt->instr.callArgs.push_back(p + "#" + advance().value);
                        }
                        else stmt->instr.callArgs.push_back(advance().value);
                    }
                    stmt->size = calculateInstructionSize(stmt->instr, pc, stmt->scopePrefix);
                }
                else if (stmt->instr.mnemonic == "zero") {
                    stmt->type = Statement::ZERO; stmt->instr.operandTokenIndex = (int)pos;
                    while (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) advance();
                    std::vector<uint8_t> d; emitZeroCode(d, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                    stmt->size = d.size();
                }
                else {
                    if (match(AssemblerTokenType::HASH)) {
                        stmt->instr.operandTokenIndex = (int)pos;
                        std::string o; while (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE && peek().type != AssemblerTokenType::COMMA) o += advance().value;
                        stmt->instr.operand = o;
                        if (!stmt->instr.forceMode) {
                            if (stmt->instr.mnemonic == "phw") stmt->instr.mode = AddressingMode::IMMEDIATE16;
                            else stmt->instr.mode = AddressingMode::IMMEDIATE;
                        }
                    }
                    else if (match(AssemblerTokenType::OPEN_PAREN)) {
                        stmt->instr.operandTokenIndex = (int)pos;
                        std::string o; while (peek().type != AssemblerTokenType::CLOSE_PAREN && peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE && peek().type != AssemblerTokenType::COMMA) o += advance().value;
                        stmt->instr.operand = o;
                        if (match(AssemblerTokenType::COMMA)) {
                            std::string r = advance().value;
                            if (r == "X" || r == "x") {
                                expect(AssemblerTokenType::CLOSE_PAREN, "Expected )");
                                if (!stmt->instr.forceMode) {
                                    try {
                                        uint32_t val = parseNumericLiteral(stmt->instr.operand);
                                        if (val > 0xFF || stmt->instr.mnemonic == "jsr" || stmt->instr.mnemonic == "jmp" || stmt->instr.mnemonic == "phw") stmt->instr.mode = AddressingMode::ABSOLUTE_X_INDIRECT;
                                        else stmt->instr.mode = AddressingMode::BASE_PAGE_X_INDIRECT;
                                    } catch(...) { stmt->instr.mode = AddressingMode::ABSOLUTE_X_INDIRECT; }
                                }
                            } else if (r == "SP" || r == "sp") {
                                expect(AssemblerTokenType::CLOSE_PAREN, "Expected )"); expect(AssemblerTokenType::COMMA, "Expected ,"); advance();
                                if (!stmt->instr.forceMode) stmt->instr.mode = AddressingMode::STACK_RELATIVE;
                            }
                        } else {
                            expect(AssemblerTokenType::CLOSE_PAREN, "Expected )");
                            if (match(AssemblerTokenType::COMMA)) {
                                std::string r = advance().value;
                                if (!stmt->instr.forceMode) {
                                    if (r == "Y" || r == "y") stmt->instr.mode = AddressingMode::BASE_PAGE_INDIRECT_Y;
                                    else if (r == "Z" || r == "z") stmt->instr.mode = AddressingMode::BASE_PAGE_INDIRECT_Z;
                                }
                            } else {
                                if (!stmt->instr.forceMode) {
                                    try {
                                        uint32_t val = parseNumericLiteral(stmt->instr.operand);
                                        if (val > 0xFF || stmt->instr.mnemonic == "jsr" || stmt->instr.mnemonic == "jmp" || stmt->instr.mnemonic == "phw") stmt->instr.mode = AddressingMode::ABSOLUTE_INDIRECT;
                                        else stmt->instr.mode = AddressingMode::INDIRECT;
                                    } catch(...) { stmt->instr.mode = AddressingMode::ABSOLUTE_INDIRECT; }
                                }
                            }
                        }
                    }
                    else if (match(AssemblerTokenType::OPEN_BRACKET)) {
                        stmt->instr.operandTokenIndex = (int)pos;
                        std::string o; while (peek().type != AssemblerTokenType::CLOSE_BRACKET && peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE && peek().type != AssemblerTokenType::COMMA) o += advance().value;
                        stmt->instr.operand = o; expect(AssemblerTokenType::CLOSE_BRACKET, "Expected ]");
                        if (match(AssemblerTokenType::COMMA)) {
                            advance(); if (!stmt->instr.forceMode) stmt->instr.mode = AddressingMode::FLAT_INDIRECT_Z;
                        }
                    }
                    else if ((peek().type == AssemblerTokenType::REGISTER || peek().type == AssemblerTokenType::IDENTIFIER) && (peek().value == "A" || peek().value == "a") && (pos + 1 >= tokens.size() || tokens[pos+1].type == AssemblerTokenType::NEWLINE || tokens[pos+1].type == AssemblerTokenType::END_OF_FILE)) {
                        advance(); if (!stmt->instr.forceMode) stmt->instr.mode = AddressingMode::ACCUMULATOR;
                    }
                    else {
                        stmt->instr.operandTokenIndex = (int)pos;
                        std::string o; while (peek().type != AssemblerTokenType::COMMA && peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) o += advance().value;
                        stmt->instr.operand = o;
                        if (peek().type == AssemblerTokenType::COMMA) {
                            advance(); std::string r = advance().value;
                            if (!stmt->instr.forceMode) {
                                if (r == "X" || r == "x") stmt->instr.mode = AddressingMode::ABSOLUTE_X;
                                else if (r == "Y" || r == "y") stmt->instr.mode = AddressingMode::ABSOLUTE_Y;
                                else if (r == "S" || r == "s" || r == "SP" || r == "sp") stmt->instr.mode = AddressingMode::STACK_RELATIVE;
                                else { stmt->instr.bitBranchTarget = r; stmt->instr.mode = AddressingMode::BASE_PAGE_RELATIVE; }
                            }
                        } else if (!stmt->instr.forceMode && !o.empty()) {
                            try {
                                uint32_t val = parseNumericLiteral(o);
                                if (val > 0xFF || stmt->instr.mnemonic == "jsr" || stmt->instr.mnemonic == "jmp" || stmt->instr.mnemonic == "phw") stmt->instr.mode = AddressingMode::ABSOLUTE;
                                else stmt->instr.mode = AddressingMode::BASE_PAGE;
                            } catch(...) { stmt->instr.mode = AddressingMode::ABSOLUTE; }
                        }
                    }
                    if ((stmt->instr.mnemonic == "inw" || stmt->instr.mnemonic == "dew") && stmt->instr.mode == AddressingMode::STACK_RELATIVE) {
                        stmt->type = (stmt->instr.mnemonic == "inw") ? Statement::STACK_INC : Statement::STACK_DEC;
                        stmt->size = (stmt->type == Statement::STACK_INC) ? 9 : 12;
                    } else if ((stmt->instr.mnemonic == "inc" || stmt->instr.mnemonic == "dec") && stmt->instr.mode == AddressingMode::STACK_RELATIVE) {
                        stmt->type = (stmt->instr.mnemonic == "inc") ? Statement::STACK_INC8 : Statement::STACK_DEC8;
                        stmt->size = 5;
                    } else if (stmt->instr.mode == AddressingMode::STACK_RELATIVE) {
                        if (stmt->instr.mnemonic == "ldx") stmt->type = Statement::LDX_STACK;
                        else if (stmt->instr.mnemonic == "ldy") stmt->type = Statement::LDY_STACK;
                        else if (stmt->instr.mnemonic == "ldz") stmt->type = Statement::LDZ_STACK;
                        else if (stmt->instr.mnemonic == "stx") stmt->type = Statement::STX_STACK;
                        else if (stmt->instr.mnemonic == "sty") stmt->type = Statement::STY_STACK;
                        else if (stmt->instr.mnemonic == "stz") stmt->type = Statement::STZ_STACK;
                    }

                    if (stmt->type != Statement::INSTRUCTION) {
                        // Already handled
                    } else if (stmt->instr.mnemonic == "phw" && stmt->instr.mode == AddressingMode::STACK_RELATIVE) {
                        stmt->type = Statement::PHW_STACK;
                        std::vector<uint8_t> d; emitPHWStackCode(d, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                        stmt->size = d.size();
                    } else {
                        stmt->size = calculateInstructionSize(stmt->instr, pc, stmt->scopePrefix);
                    }
                }
            }
        }
        else if (stmt->label.empty()) { advance(); continue; }
        pc += stmt->size;
        statements.push_back(std::move(stmt));
    }
}


void AssemblerParser::emitASWCode(std::vector<uint8_t>& binary, const std::string& dest, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    AssemblerSimulatedOps::emitASWCode(this, e, dest, tokenIndex, scopePrefix);
}

void AssemblerParser::emitROWCode(std::vector<uint8_t>& binary, const std::string& dest, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    AssemblerSimulatedOps::emitROWCode(this, e, dest, tokenIndex, scopePrefix);
}

void AssemblerParser::emitLSL16Code(std::vector<uint8_t>& binary, const std::string& dest, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    AssemblerSimulatedOps::emitLSL16Code(this, e, dest, tokenIndex, scopePrefix);
}

void AssemblerParser::emitLSR16Code(std::vector<uint8_t>& binary, const std::string& dest, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    AssemblerSimulatedOps::emitLSR16Code(this, e, dest, tokenIndex, scopePrefix);
}

void AssemblerParser::emitROL16Code(std::vector<uint8_t>& binary, const std::string& dest, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    AssemblerSimulatedOps::emitROL16Code(this, e, dest, tokenIndex, scopePrefix);
}

void AssemblerParser::emitROR16Code(std::vector<uint8_t>& binary, const std::string& dest, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    AssemblerSimulatedOps::emitROR16Code(this, e, dest, tokenIndex, scopePrefix);
}

void AssemblerParser::emitASR16Code(std::vector<uint8_t>& binary, const std::string& dest, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    AssemblerSimulatedOps::emitASR16Code(this, e, dest, tokenIndex, scopePrefix);
}

void AssemblerParser::emitSXT8Code(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary, getZPStart());
    AssemblerSimulatedOps::emitSXT8Code(this, e, tokenIndex, scopePrefix);
}

void AssemblerParser::emitPushPopCode(std::vector<uint8_t>& binary, bool isPush, int tokenIndex, const std::string& scopePrefix) {
    M65Emitter e(binary);
    std::string reg = tokens[tokenIndex].value;
    if (tokens[tokenIndex].type == AssemblerTokenType::REGISTER) reg = "." + reg;
    AssemblerSimulatedOps::emitPushPopCode(this, e, isPush, reg, tokenIndex, scopePrefix);
}

int AssemblerParser::calculateInstructionSize(const Instruction& instr, uint32_t currentAddr, const std::string& scopePrefix) {
    if (instr.mnemonic == "proc") return 0;
    if (instr.mnemonic == "endproc") return (instr.procParamSize == 0) ? 1 : 2;
    if (instr.mnemonic == "push" || instr.mnemonic == "pop") {
        return AssemblerSimulatedOps::getPushPopSize(this, instr.mnemonic == "push", instr.operand, instr.operandTokenIndex, scopePrefix);
    }

    AddressingMode resolvedMode = instr.mode;
    
    // Auto-promote BASE_PAGE to ABSOLUTE if it doesn't fit in 8 bits.
    // Or promote identifiers if they don't have a fixed mode.
    if (!instr.forceMode && instr.operandTokenIndex != -1 && (instr.mode == AddressingMode::BASE_PAGE || instr.mode == AddressingMode::ABSOLUTE || 
        instr.mode == AddressingMode::BASE_PAGE_X || instr.mode == AddressingMode::ABSOLUTE_X ||
        instr.mode == AddressingMode::BASE_PAGE_Y || instr.mode == AddressingMode::ABSOLUTE_Y)) {
        
        try {
            uint32_t val = evaluateExpressionAt(instr.operandTokenIndex, scopePrefix);
            bool fitsIn8 = (val <= 0xFF);
            bool forceAbs = (instr.mnemonic == "jsr" || instr.mnemonic == "jmp");

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
    bool isQuad = (instr.mnemonic.size() > 1 && instr.mnemonic.back() == 'q' && instr.mnemonic != "ldq" && instr.mnemonic != "stq" && instr.mnemonic != "beq" && instr.mnemonic != "bne" && instr.mnemonic != "bra" && instr.mnemonic != "bsr");

    if (isQuad) size += 2;
    if (resolvedMode == AddressingMode::FLAT_INDIRECT_Z) size += 1;

    if (instr.mnemonic == "beq" || instr.mnemonic == "bne" || instr.mnemonic == "bra" || instr.mnemonic == "bcc" || instr.mnemonic == "bcs" || instr.mnemonic == "bpl" || instr.mnemonic == "bmi" || instr.mnemonic == "bvc" || instr.mnemonic == "bvs") {
        Symbol* sym = resolveSymbol(instr.operand, scopePrefix);
        if (sym) {
            uint32_t target = sym->value;
            int32_t diff = (int32_t)target - (int32_t)(currentAddr + 2);
            if (diff >= -128 && diff <= 127) return 2;
        }
        return 3;
    }

    if (instr.mnemonic == "bsr") return 3;

    if (instr.mnemonic == "call") {
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

    if (instr.mnemonic == "rtn") return 2;

    switch (resolvedMode) {
        case AddressingMode::IMPLIED: case AddressingMode::ACCUMULATOR: return size;
        case AddressingMode::IMMEDIATE: case AddressingMode::BASE_PAGE: case AddressingMode::BASE_PAGE_X: case AddressingMode::BASE_PAGE_Y: case AddressingMode::INDIRECT: case AddressingMode::BASE_PAGE_X_INDIRECT: case AddressingMode::BASE_PAGE_INDIRECT_Y: case AddressingMode::BASE_PAGE_INDIRECT_Z: case AddressingMode::STACK_RELATIVE: return size + 1;
        case AddressingMode::ABSOLUTE: case AddressingMode::ABSOLUTE_X: case AddressingMode::ABSOLUTE_Y: case AddressingMode::ABSOLUTE_INDIRECT: case AddressingMode::ABSOLUTE_X_INDIRECT: case AddressingMode::IMMEDIATE16: return size + 2;
        case AddressingMode::BASE_PAGE_RELATIVE: return size + 2;
        case AddressingMode::FLAT_INDIRECT_Z: return size + 1;
        default: return size + 2;
    }
}

int AssemblerParser::calculateDirectiveSize(const Directive& dir, uint32_t currentAddr) {
    if (dir.name == "byte") return (int)dir.arguments.size();
    if (dir.name == "word") return (int)dir.arguments.size() * 2;
    if (dir.name == "dword" || dir.name == "long") return (int)dir.arguments.size() * 4;
    if (dir.name == "float") return (int)dir.arguments.size() * 5;
    if (dir.name == "text" || dir.name == "ascii") {
        if (dir.arguments.empty()) return 0;
        return (int)dir.arguments[0].size();
    }
    if (dir.name == "res") {
        if (dir.arguments.empty()) return 0;
        return (int)evaluateExpressionAt(dir.tokenIndex, "");
    }
    if (dir.name == "align" || dir.name == "balign") {
        if (dir.arguments.empty()) return 0;
        uint32_t align = parseNumericLiteral(dir.arguments[0]);
        if (align == 0) return 0;
        return (align - (currentAddr % align)) % align;
    }
    return 0;
}

std::vector<uint8_t> AssemblerParser::pass2() {
    bool overallChanged;
    do {
        overallChanged = false;
        bool optimizerMadeChanges = optimize();
        if (optimizerMadeChanges) overallChanged = true;
        std::deque<std::unique_ptr<Statement>> newStatements;
        for (auto& s : statements) {
            if (!s->deleted) newStatements.push_back(std::move(s));
            else overallChanged = true;
        }
        statements = std::move(newStatements);
        bool addressRecalculationMadeChanges = false;
        std::map<std::string, uint32_t> pass2PCs;
        for (auto const& [name, seg] : segments) pass2PCs[name] = 0;
        std::string activeSegment = "default"; uint32_t cP = 0; bool isDeadCode = false;
        for (auto& s : statements) {
            if (s->type == Statement::DIRECTIVE && (s->dir.name == "segment" || s->dir.name == "code" || s->dir.name == "text" || s->dir.name == "data" || s->dir.name == "bss")) {
                pass2PCs[activeSegment] = cP; activeSegment = s->segmentName; cP = pass2PCs[activeSegment];
            }
            if (s->type == Statement::DIRECTIVE && s->dir.name == "org") {
                if (!s->dir.arguments.empty()) {
                    cP = parseNumericLiteral(s->dir.arguments[0]);
                    if (segments[activeSegment]->startAddress == 0xFFFFFFFF) segments[activeSegment]->startAddress = cP;
                }
            }
            if (segments[activeSegment]->startAddress == 0xFFFFFFFF && (s->size > 0 || s->type == Statement::INSTRUCTION)) segments[activeSegment]->startAddress = cP;
            pc = cP; s->address = cP;
            if (!s->label.empty()) {
                isDeadCode = false;
                if (symbolTable[s->label].value != cP) { symbolTable[s->label].value = cP; addressRecalculationMadeChanges = true; }
            }
            if (s->type == Statement::DIRECTIVE && s->dir.name == "org") {
                if (!s->dir.arguments.empty()) cP = parseNumericLiteral(s->dir.arguments[0]);
                s->address = cP;
            }
            int oS = s->size;
            if (s->type == Statement::INSTRUCTION && s->instr.mnemonic == "proc") s->size = 0;
            else if (s->type == Statement::INSTRUCTION && s->instr.mnemonic == "endproc") {
                if (isDeadCode) s->size = 0; else s->size = (s->instr.procParamSize == 0) ? 1 : 2;
                isDeadCode = false;
            } else if (isDeadCode && s->type != Statement::DIRECTIVE && s->type != Statement::BASIC_UPSTART) s->size = 0;
            else {
                if (s->type == Statement::INSTRUCTION) s->size = calculateInstructionSize(s->instr, cP, s->scopePrefix);
                else if (s->isSimulatedOp()) {
                    std::vector<uint8_t> d;
                    if (s->type == Statement::ADD16 || s->type == Statement::SUB16 || s->type == Statement::ADDS16 || s->type == Statement::SUBS16) emitAddSub16Code(d, s->type == Statement::ADD16 || s->type == Statement::ADDS16, s->instr.operand, s->exprTokenIndex, s->scopePrefix);
                    else if (s->type == Statement::AND16 || s->type == Statement::ORA16 || s->type == Statement::EOR16) emitBitwise16Code(d, s->instr.mnemonic, s->instr.operand, s->exprTokenIndex, s->scopePrefix);
                    else if (s->type == Statement::CMP16) emitCMP16Code(d, s->instr.operand, s->exprTokenIndex, s->scopePrefix);
                    else if (s->type == Statement::CMP_S16) emitCMP_S16Code(d, s->instr.operand, s->exprTokenIndex, s->scopePrefix);
                    else if (s->type == Statement::LDW) emitLDWCode(d, s->instr.operand, s->exprTokenIndex, s->scopePrefix, s->instr.mnemonic == "ldw.sp");
                    else if (s->type == Statement::STW) emitSTWCode(d, s->instr.operand, s->exprTokenIndex, s->scopePrefix, s->instr.mnemonic == "stw.sp");
                    else if (s->type == Statement::FILL) emitFillCode(d, s->instr.operandTokenIndex, s->scopePrefix, s->instr.mnemonic == "fill.sp");
                    else if (s->type == Statement::COPY) emitMoveCode(d, s->instr.operandTokenIndex, s->scopePrefix, s->instr.mnemonic == "move.sp");
                    else if (s->type == Statement::NEG16 || s->type == Statement::NOT16 || s->type == Statement::NEG_S16) emitNegNot16Code(d, s->type == Statement::NEG16 || s->type == Statement::NEG_S16, s->instr.operand, s->instr.operandTokenIndex, s->scopePrefix);
                    else if (s->type == Statement::ABS16 || s->type == Statement::ABS_S16) emitABS16Code(d, s->instr.operand, s->instr.operandTokenIndex, s->scopePrefix);
                    else if (s->type == Statement::CHKZERO8) emitChkZeroCode(d, false, false, s->instr.operandTokenIndex, s->scopePrefix);
                    else if (s->type == Statement::CHKZERO16) emitChkZeroCode(d, true, false, s->instr.operandTokenIndex, s->scopePrefix);
                    else if (s->type == Statement::CHKNONZERO8) emitChkZeroCode(d, false, true, s->instr.operandTokenIndex, s->scopePrefix);
                    else if (s->type == Statement::CHKNONZERO16) emitChkZeroCode(d, true, true, s->instr.operandTokenIndex, s->scopePrefix);
                    else if (s->type == Statement::BRANCH16) emitBranch16Code(d, s->instr.operandTokenIndex, s->scopePrefix);
                    else if (s->type == Statement::SELECT) emitSelectCode(d, s->instr.operandTokenIndex, s->scopePrefix);
                    else if (s->type == Statement::PHW_STACK) emitPHWStackCode(d, s->instr.operandTokenIndex, s->scopePrefix);
                    else if (s->type == Statement::LDX_STACK) emitLDX_StackCode(d, s->instr.operandTokenIndex, s->scopePrefix);
                    else if (s->type == Statement::LDY_STACK) emitLDY_StackCode(d, s->instr.operandTokenIndex, s->scopePrefix);
                    else if (s->type == Statement::LDZ_STACK) emitLDZ_StackCode(d, s->instr.operandTokenIndex, s->scopePrefix);
                    else if (s->type == Statement::STX_STACK) emitSTX_StackCode(d, s->instr.operandTokenIndex, s->scopePrefix);
                    else if (s->type == Statement::STY_STACK) emitSTY_StackCode(d, s->instr.operandTokenIndex, s->scopePrefix);
                    else if (s->type == Statement::STZ_STACK) emitSTZ_StackCode(d, s->instr.operandTokenIndex, s->scopePrefix);
                    else if (s->type == Statement::PTRDEREF) emitPtrDerefCode(d, s->instr.operandTokenIndex, s->scopePrefix);
                    else if (s->type == Statement::LDWF || s->type == Statement::STWF || s->type == Statement::INCF || s->type == Statement::DECF) emitFlatMemoryCode(d, s->instr.mnemonic, s->instr.operandTokenIndex, s->scopePrefix);
                    else if (s->type == Statement::ASW) emitASWCode(d, s->instr.operand, s->instr.operandTokenIndex, s->scopePrefix);
                    else if (s->type == Statement::ROW) emitROWCode(d, s->instr.operand, s->instr.operandTokenIndex, s->scopePrefix);
                    else if (s->type == Statement::LSL16 || s->type == Statement::LSL_S16) emitLSL16Code(d, s->instr.operand, s->instr.operandTokenIndex, s->scopePrefix);
                    else if (s->type == Statement::LSR16 || s->type == Statement::LSR_S16) emitLSR16Code(d, s->instr.operand, s->instr.operandTokenIndex, s->scopePrefix);
                    else if (s->type == Statement::ROL16 || s->type == Statement::ROL_S16) emitROL16Code(d, s->instr.operand, s->instr.operandTokenIndex, s->scopePrefix);
                    else if (s->type == Statement::ROR16 || s->type == Statement::ROR_S16) emitROR16Code(d, s->instr.operand, s->instr.operandTokenIndex, s->scopePrefix);
                    else if (s->type == Statement::ASR16 || s->type == Statement::ASR_S16) emitASR16Code(d, s->instr.operand, s->instr.operandTokenIndex, s->scopePrefix);
                    else if (s->type == Statement::SXT8) emitSXT8Code(d, s->instr.operandTokenIndex, s->scopePrefix);
                    else if (s->type == Statement::PUSH || s->type == Statement::POP) emitPushPopCode(d, s->type == Statement::PUSH, s->instr.operandTokenIndex, s->scopePrefix);
                    else if (s->type == Statement::EXPR) emitExpressionCode(d, s->exprTarget, s->exprTokenIndex, s->scopePrefix);
                    else if (s->type == Statement::MUL || s->type == Statement::DIV) {
                        if (s->type == Statement::MUL) emitMulCode(d, s->mulWidth, s->instr.operand, s->exprTokenIndex, s->scopePrefix);
                        else emitDivCode(d, s->mulWidth, s->instr.operand, s->exprTokenIndex, s->scopePrefix);
                    }
                    else if (s->type == Statement::STACK_INC || s->type == Statement::STACK_DEC) emitStackIncDecCode(d, s->type == Statement::STACK_INC, s->instr.operandTokenIndex, s->scopePrefix);
                    else if (s->type == Statement::STACK_INC8 || s->type == Statement::STACK_DEC8) emitStackIncDec8Code(d, s->type == Statement::STACK_INC8, s->instr.operandTokenIndex, s->scopePrefix);
                    else if (s->type == Statement::ZERO) emitZeroCode(d, s->instr.operandTokenIndex, s->scopePrefix);
                    s->size = (int)d.size();
                }
                else if (s->type == Statement::DIRECTIVE) s->size = calculateDirectiveSize(s->dir, cP);
                else if (s->type == Statement::BASIC_UPSTART) s->size = 12;
            }
            if (s->size != oS) addressRecalculationMadeChanges = true;
            cP += s->size;
            if (s->type == Statement::INSTRUCTION) {
                if (s->instr.mnemonic == "rts" || s->instr.mnemonic == "rtn" || s->instr.mnemonic == "rti") isDeadCode = true;
            }
        }
        if (addressRecalculationMadeChanges) overallChanged = true;
    } while (overallChanged);
    return AssemblerGenerator::generate(this);
}


