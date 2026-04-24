#include "AssemblerGenerator.hpp"
#include "AssemblerParser.hpp"
#include "M65Emitter.hpp"
#include "AssemblerOpcodeDatabase.hpp"
#include "AssemblerSimulatedOps.hpp"
#include <algorithm>
#include <stdexcept>
#include <cmath>
#include <iostream>

// Forward declarations for helper functions if they are static in AssemblerParser.cpp
// or just re-implement them if they are simple enough and used here.

static uint8_t toPetscii(char c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    if (c >= 'A' && c <= 'Z') return c + 32;
    return (uint8_t)c;
}

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

static std::vector<uint8_t> encodeFloat(double val) {
    std::vector<uint8_t> result(5);
    if (val == 0.0) { std::fill(result.begin(), result.end(), 0); return result; }
    int exponent;
    double mantissa = std::frexp(std::abs(val), &exponent);
    result[0] = (uint8_t)(exponent + 0x80);
    uint32_t mVal = (uint32_t)(mantissa * 4294967296.0);
    result[1] = (uint8_t)((mVal >> 24) & 0x7F) | (val < 0 ? 0x80 : 0x00);
    result[2] = (uint8_t)(mVal >> 16);
    result[3] = (uint8_t)(mVal >> 8);
    result[4] = (uint8_t)mVal;
    return result;
}

std::vector<uint8_t> AssemblerGenerator::generate(AssemblerParser* parser) {
    std::vector<uint8_t> binary;
    M65Emitter e(binary, parser->getZPStart());
    generate(parser, e);
    return binary;
}

void AssemblerGenerator::generate(AssemblerParser* parser, M65Emitter& e) {
    std::shared_ptr<AssemblerParser::ProcContext> currentPass2Proc;
    std::vector<std::shared_ptr<AssemblerParser::ProcContext>> pass2ProcStack;

    for (auto& [name, symbol] : parser->symbolTable) if (symbol.isVariable) symbol.value = symbol.initialValue;

    std::vector<std::string> order = parser->requestedSegmentOrder;
    if (order.empty()) {
        order = {"code", "data", "bss"};
    }
    
    // Create a list of all known segments to ensure they are all processed
    std::vector<std::string> allSegNames;
    for (const auto& s : order) {
        if (parser->segments.count(s)) allSegNames.push_back(s);
    }
    // Also include any segments not in the requested order
    for (auto const& seg : parser->segmentOrder) {
        bool found = false;
        for (const auto& s : order) if (s == seg->name) { found = true; break; }
        if (!found) allSegNames.push_back(seg->name);
    }

    for (const auto& segName : allSegNames) {
        if (segName == "bss") continue; // Skip BSS segments for binary output
        auto seg = parser->segments[segName];
        if (seg->startAddress == 0xFFFFFFFF) continue; // Skip segments with no content

        bool isDeadCode = false;
        for (auto& stmt : parser->statements) {
            if (stmt->segmentName != segName) continue;
            
            if (stmt->address > e.getAddress()) {
                e.setAddress(stmt->address);
            }
            
            parser->pc = stmt->address; // Update parser pc for .PC evaluations
            if (stmt->deleted) continue;
            if (!stmt->label.empty()) {
                isDeadCode = false;
                e.emitLabel(stmt->label);
            }

            if (stmt->type == AssemblerParser::Statement::EXPR) {
                if (!isDeadCode) AssemblerSimulatedOps::emitExpressionCode(parser, e, stmt->exprTarget, stmt->exprTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::MUL) {
                if (!isDeadCode) AssemblerSimulatedOps::emitMulCode(parser, e, stmt->mulWidth, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::DIV) {
                if (!isDeadCode) AssemblerSimulatedOps::emitDivCode(parser, e, stmt->mulWidth, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::STACK_INC) {
                if (!isDeadCode) AssemblerSimulatedOps::emitStackIncDecCode(parser, e, true, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::STACK_DEC) {
                if (!isDeadCode) AssemblerSimulatedOps::emitStackIncDecCode(parser, e, false, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::STACK_INC8) {
                if (!isDeadCode) AssemblerSimulatedOps::emitStackIncDec8Code(parser, e, true, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::STACK_DEC8) {
                if (!isDeadCode) AssemblerSimulatedOps::emitStackIncDec8Code(parser, e, false, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::ADD16 || stmt->type == AssemblerParser::Statement::ADDS16) {
                if (!isDeadCode) AssemblerSimulatedOps::emitAddSub16Code(parser, e, true, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::SUB16 || stmt->type == AssemblerParser::Statement::SUBS16) {
                if (!isDeadCode) AssemblerSimulatedOps::emitAddSub16Code(parser, e, false, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::AND16 || stmt->type == AssemblerParser::Statement::ORA16 || stmt->type == AssemblerParser::Statement::EOR16) {
                if (!isDeadCode) AssemblerSimulatedOps::emitBitwise16Code(parser, e, stmt->instr.mnemonic, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::CMP16) {
                if (!isDeadCode) AssemblerSimulatedOps::emitCMP16Code(parser, e, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::CMP_S16) {
                if (!isDeadCode) AssemblerSimulatedOps::emitCMP_S16Code(parser, e, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::ABS16 || stmt->type == AssemblerParser::Statement::ABS_S16) {
                if (!isDeadCode) AssemblerSimulatedOps::emitABS16Code(parser, e, stmt->instr.operand, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::ROL16 || stmt->type == AssemblerParser::Statement::ROL_S16) {
                if (!isDeadCode) AssemblerSimulatedOps::emitROL16Code(parser, e, stmt->instr.operand, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::ROR16 || stmt->type == AssemblerParser::Statement::ROR_S16) {
                if (!isDeadCode) AssemblerSimulatedOps::emitROR16Code(parser, e, stmt->instr.operand, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::LDW) {
                if (!isDeadCode) AssemblerSimulatedOps::emitLDWCode(parser, e, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix, stmt->instr.mnemonic == "ldw.sp");
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::STW) {
                if (!isDeadCode) AssemblerSimulatedOps::emitSTWCode(parser, e, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix, stmt->instr.mnemonic == "stw.sp");
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::FILL) {
                if (!isDeadCode) AssemblerSimulatedOps::emitFillCode(parser, e, stmt->instr.operandTokenIndex, stmt->scopePrefix, stmt->instr.mnemonic == "fill.sp");
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::COPY) {
                if (!isDeadCode) AssemblerSimulatedOps::emitMoveCode(parser, e, stmt->instr.operandTokenIndex, stmt->scopePrefix, stmt->instr.mnemonic == "move.sp");
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::SWAP) {
                if (!isDeadCode) AssemblerSimulatedOps::emitSwapCode(parser, e, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::NEG16 || stmt->type == AssemblerParser::Statement::NEG_S16) {
                if (!isDeadCode) AssemblerSimulatedOps::emitNegNot16Code(parser, e, true, stmt->instr.operand, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::NOT16) {
                if (!isDeadCode) AssemblerSimulatedOps::emitNegNot16Code(parser, e, false, stmt->instr.operand, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::CHKZERO8) {
                if (!isDeadCode) AssemblerSimulatedOps::emitChkZeroCode(parser, e, false, false, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::CHKZERO16) {
                if (!isDeadCode) AssemblerSimulatedOps::emitChkZeroCode(parser, e, true, false, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::CHKNONZERO8) {
                if (!isDeadCode) AssemblerSimulatedOps::emitChkZeroCode(parser, e, false, true, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::CHKNONZERO16) {
                if (!isDeadCode) AssemblerSimulatedOps::emitChkZeroCode(parser, e, true, true, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::BRANCH16) {
                if (!isDeadCode) AssemblerSimulatedOps::emitBranch16Code(parser, e, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::SELECT) {
                if (!isDeadCode) AssemblerSimulatedOps::emitSelectCode(parser, e, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::PTRSTACK) {
                if (!isDeadCode) AssemblerSimulatedOps::emitPtrStackCode(parser, e, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::PTRDEREF) {
                if (!isDeadCode) AssemblerSimulatedOps::emitPtrDerefCode(parser, e, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::LDWF || stmt->type == AssemblerParser::Statement::STWF || stmt->type == AssemblerParser::Statement::INCF || stmt->type == AssemblerParser::Statement::DECF) {
                if (!isDeadCode) AssemblerSimulatedOps::emitFlatMemoryCode(parser, e, stmt->instr.mnemonic, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::ASW) {
                if (!isDeadCode) AssemblerSimulatedOps::emitASWCode(parser, e, stmt->instr.operand, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::ROW) {
                if (!isDeadCode) AssemblerSimulatedOps::emitROWCode(parser, e, stmt->instr.operand, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::LSL16 || stmt->type == AssemblerParser::Statement::LSL_S16) {
                if (!isDeadCode) AssemblerSimulatedOps::emitLSL16Code(parser, e, stmt->instr.operand, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::LSR16 || stmt->type == AssemblerParser::Statement::LSR_S16) {
                if (!isDeadCode) AssemblerSimulatedOps::emitLSR16Code(parser, e, stmt->instr.operand, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::ASR16 || stmt->type == AssemblerParser::Statement::ASR_S16) {
                if (!isDeadCode) AssemblerSimulatedOps::emitASR16Code(parser, e, stmt->instr.operand, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::SXT8) {
                if (!isDeadCode) AssemblerSimulatedOps::emitSXT8Code(parser, e, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::PUSH || stmt->type == AssemblerParser::Statement::POP) {
                if (!isDeadCode) AssemblerSimulatedOps::emitPushPopCode(parser, e, stmt->type == AssemblerParser::Statement::PUSH, stmt->instr.operand, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::PHW_STACK) {
                if (!isDeadCode) AssemblerSimulatedOps::emitPHWStackCode(parser, e, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::LDX_STACK) {
                if (!isDeadCode) AssemblerSimulatedOps::emitLDX_StackCode(parser, e, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::LDY_STACK) {
                if (!isDeadCode) AssemblerSimulatedOps::emitLDY_StackCode(parser, e, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::LDZ_STACK) {
                if (!isDeadCode) AssemblerSimulatedOps::emitLDZ_StackCode(parser, e, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::STX_STACK) {
                if (!isDeadCode) AssemblerSimulatedOps::emitSTX_StackCode(parser, e, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::STY_STACK) {
                if (!isDeadCode) AssemblerSimulatedOps::emitSTY_StackCode(parser, e, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::STZ_STACK) {
                if (!isDeadCode) AssemblerSimulatedOps::emitSTZ_StackCode(parser, e, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::ZERO) {
                if (!isDeadCode) AssemblerSimulatedOps::emitZeroCode(parser, e, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::LDAX || stmt->type == AssemblerParser::Statement::LDAY || stmt->type == AssemblerParser::Statement::LDAZ) {
                if (!isDeadCode) AssemblerSimulatedOps::emitLDWCode(parser, e, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix, false);
                continue;
            }
            if (stmt->type == AssemblerParser::Statement::STAX || stmt->type == AssemblerParser::Statement::STAY || stmt->type == AssemblerParser::Statement::STAZ) {
                if (!isDeadCode) AssemblerSimulatedOps::emitSTWCode(parser, e, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix, false);
                continue;
            }

            if (stmt->type == AssemblerParser::Statement::PUSH || stmt->type == AssemblerParser::Statement::POP) {
                if (!isDeadCode) AssemblerSimulatedOps::emitPushPopCode(parser, e, stmt->type == AssemblerParser::Statement::PUSH, stmt->instr.operand, stmt->instr.operandTokenIndex, stmt->scopePrefix);
                continue;
            }

            if (stmt->type == AssemblerParser::Statement::BASIC_UPSTART) {
                if (!isDeadCode) {
                    uint32_t addr = parser->evaluateExpressionAt(stmt->basicUpstartTokenIndex, stmt->scopePrefix);
                    std::string aS = std::to_string(addr);
                    while (aS.length() < 4) aS = " " + aS;
                    if (aS.length() > 4) aS = aS.substr(aS.length() - 4);
                    e.emitByte(0x0b); e.emitByte(0x08); e.emitByte(0x0a); e.emitByte(0x00);
                    e.emitByte(0x9e); // SYS
                    for (char c : aS) e.emitByte(toPetscii(c));
                    e.emitByte(0x00); e.emitByte(0x00); e.emitByte(0x00);
                }
                continue;
            }

            if (stmt->type == AssemblerParser::Statement::INSTRUCTION) {
                if (stmt->isSimulatedOp()) continue; // Handled above
                if (stmt->instr.mnemonic == "proc") {
                    pass2ProcStack.push_back(currentPass2Proc);
                    currentPass2Proc = stmt->procCtx;
                    continue;
                } else if (stmt->instr.mnemonic == "endproc") {
                    if (!isDeadCode) {
                        if (stmt->instr.procParamSize == 0) e.emitInstruction("rts", AddressingMode::IMPLIED);
                        else e.emitInstruction("rts", AddressingMode::IMMEDIATE, stmt->instr.procParamSize, true);
                    }
                    currentPass2Proc = pass2ProcStack.empty() ? nullptr : pass2ProcStack.back();
                    if (!pass2ProcStack.empty()) pass2ProcStack.pop_back();
                    isDeadCode = false;
                    continue;
                } else if (stmt->instr.mnemonic == "call") {
                    if (!isDeadCode) {
                        for (const auto& arg : stmt->instr.callArgs) {
                            bool isB = (arg.size() >= 2 && arg.substr(0, 2) == "B#");
                            std::string v = isB ? arg.substr(2) : ( (arg.size() >= 2 && arg.substr(0, 2) == "W#") ? arg.substr(2) : (arg[0] == '#' ? arg.substr(1) : arg));
                            uint32_t val;
                            Symbol* sym = parser->resolveSymbol(v, stmt->scopePrefix);
                            if (sym) val = sym->value; else val = parseNumericLiteral(v);
                            if (!isB && !arg.empty() && arg[0] != '#' && arg.size() >= 2 && arg.substr(0,2) != "W#" && sym) isB = (sym->size == 1);
                            if (isB) { e.lda_imm(val & 0xFF); e.pha(); } else { e.emitInstruction("phw", AddressingMode::IMMEDIATE16, val & 0xFFFF, true); }
                        }
                        Symbol* sym = parser->resolveSymbol(stmt->instr.operand, stmt->scopePrefix);
                        uint32_t target = sym ? sym->value : 0;
                        e.emitInstruction("jsr", AddressingMode::ABSOLUTE, target, true);
                    }
                } else { // Handle all other instructions
                    if (!isDeadCode) {
                        AddressingMode resolvedMode = stmt->instr.mode;
                        if (!stmt->instr.forceMode && stmt->instr.operandTokenIndex != -1 && (stmt->instr.mode == AddressingMode::BASE_PAGE || stmt->instr.mode == AddressingMode::ABSOLUTE ||
                            stmt->instr.mode == AddressingMode::BASE_PAGE_X || stmt->instr.mode == AddressingMode::ABSOLUTE_X ||
                            stmt->instr.mode == AddressingMode::BASE_PAGE_Y || stmt->instr.mode == AddressingMode::ABSOLUTE_Y)) {
                            uint32_t val = parser->evaluateExpressionAt(stmt->instr.operandTokenIndex, stmt->scopePrefix);
                            bool fitsIn8 = (val <= 0xFF);
                            bool forceAbs = (stmt->instr.mnemonic == "jsr" || stmt->instr.mnemonic == "jmp");
                            if (stmt->instr.mode == AddressingMode::BASE_PAGE || stmt->instr.mode == AddressingMode::ABSOLUTE) resolvedMode = (fitsIn8 && !forceAbs) ? AddressingMode::BASE_PAGE : AddressingMode::ABSOLUTE;
                            else if (stmt->instr.mode == AddressingMode::BASE_PAGE_X || stmt->instr.mode == AddressingMode::ABSOLUTE_X) resolvedMode = (fitsIn8 && !forceAbs) ? AddressingMode::BASE_PAGE_X : AddressingMode::ABSOLUTE_X;
                            else if (stmt->instr.mode == AddressingMode::BASE_PAGE_Y || stmt->instr.mode == AddressingMode::ABSOLUTE_Y) resolvedMode = (fitsIn8 && !forceAbs) ? AddressingMode::BASE_PAGE_Y : AddressingMode::ABSOLUTE_Y;
                        }
                        bool isQuad = (stmt->instr.mnemonic.size() > 1 && stmt->instr.mnemonic.back() == 'q' && stmt->instr.mnemonic != "ldq" && stmt->instr.mnemonic != "stq" && stmt->instr.mnemonic != "beq" && stmt->instr.mnemonic != "bne" && stmt->instr.mnemonic != "bra" && stmt->instr.mnemonic != "bsr");
                        if (isQuad) { e.emitByte(0x42); e.emitByte(0x42); }
                        if (resolvedMode == AddressingMode::FLAT_INDIRECT_Z) e.eom();


                        if (stmt->instr.mnemonic == "asw" && resolvedMode == AddressingMode::ABSOLUTE) {
                            uint32_t addr = parser->evaluateExpressionAt(stmt->instr.operandTokenIndex, stmt->scopePrefix);
                            e.asw_abs(addr);
                            continue;
                        } else if (stmt->instr.mnemonic == "row" && resolvedMode == AddressingMode::ABSOLUTE) {
                            uint32_t addr = parser->evaluateExpressionAt(stmt->instr.operandTokenIndex, stmt->scopePrefix);
                            e.row_abs(addr);
                            continue;
                        } else if (stmt->instr.mode == AddressingMode::BASE_PAGE_RELATIVE) {
                            Symbol* sym = parser->resolveSymbol(stmt->instr.operand, stmt->scopePrefix);
                            uint32_t v = sym ? sym->value : parseNumericLiteral(stmt->instr.operand);
                            uint8_t op = AssemblerOpcodeDatabase::getOpcode(stmt->instr.mnemonic, resolvedMode);
                            e.emitByte(op);
                            e.emitByte((uint8_t)v);
                            Symbol* tsym = parser->resolveSymbol(stmt->instr.bitBranchTarget, stmt->scopePrefix);
                            uint32_t t = tsym ? tsym->value : 0;
                            int32_t off = (int32_t)t - (int32_t)(stmt->address + 3);
                            e.emitByte((uint8_t)off);
                        } else if (stmt->instr.mnemonic == "rtn") {
                            Symbol* sym = parser->resolveSymbol(stmt->instr.operand, stmt->scopePrefix);
                            uint32_t v = sym ? sym->value : parseNumericLiteral(stmt->instr.operand);
                            if (v == 0) e.emitInstruction("rts", AddressingMode::IMPLIED);
                            else e.emitInstruction("rts", AddressingMode::IMMEDIATE, v, true);
                        } else { // Handle generic instructions and their operands
                            bool isBranch = (stmt->instr.mnemonic == "beq" || stmt->instr.mnemonic == "bne" || stmt->instr.mnemonic == "bra" || stmt->instr.mnemonic == "bcc" || stmt->instr.mnemonic == "bcs" || stmt->instr.mnemonic == "bpl" || stmt->instr.mnemonic == "bmi" || stmt->instr.mnemonic == "bvc" || stmt->instr.mnemonic == "bvs" || stmt->instr.mnemonic == "bsr");
                            uint32_t val = 0;
                            bool hasValue = false;
                            if (!isBranch) {
                                if (resolvedMode == AddressingMode::IMMEDIATE || resolvedMode == AddressingMode::STACK_RELATIVE || resolvedMode == AddressingMode::BASE_PAGE_INDIRECT_Z || resolvedMode == AddressingMode::FLAT_INDIRECT_Z || resolvedMode == AddressingMode::BASE_PAGE_INDIRECT_SP_Y || resolvedMode == AddressingMode::BASE_PAGE_X_INDIRECT || resolvedMode == AddressingMode::BASE_PAGE_INDIRECT_Y || resolvedMode == AddressingMode::BASE_PAGE || resolvedMode == AddressingMode::BASE_PAGE_X || resolvedMode == AddressingMode::BASE_PAGE_Y || resolvedMode == AddressingMode::ABSOLUTE || resolvedMode == AddressingMode::ABSOLUTE_X || resolvedMode == AddressingMode::ABSOLUTE_Y || resolvedMode == AddressingMode::ABSOLUTE_INDIRECT || resolvedMode == AddressingMode::ABSOLUTE_X_INDIRECT || resolvedMode == AddressingMode::IMMEDIATE16) {
                                    val = parser->evaluateExpressionAt(stmt->instr.operandTokenIndex, stmt->scopePrefix);
                                    hasValue = true;
                                }
                                e.emitInstruction(stmt->instr.mnemonic, resolvedMode, val, hasValue);
                            } else { // Branch instructions
                                Symbol* sym = parser->resolveSymbol(stmt->instr.operand, stmt->scopePrefix);
                                uint32_t t = sym ? sym->value : 0;
                                if (stmt->instr.mnemonic == "bsr") {
                                    int32_t off = (int32_t)t - (int32_t)(stmt->address + 3);
                                    e.emitInstruction("bsr", AddressingMode::RELATIVE16, (uint32_t)(uint16_t)(int16_t)off, true);
                                } else if (stmt->size == 2) {
                                    int32_t off2 = (int32_t)t - (int32_t)(stmt->address + 2);
                                    e.emitInstruction(stmt->instr.mnemonic, AddressingMode::RELATIVE, (uint32_t)(uint8_t)(int8_t)off2, true);
                                } else {
                                    int32_t off3 = (int32_t)t - (int32_t)(stmt->address + 3);
                                    e.emitInstruction(stmt->instr.mnemonic, AddressingMode::RELATIVE16, (uint32_t)(uint16_t)(int16_t)off3, true);
                                }
                            }
                        }
                    }
                    if (stmt->instr.mnemonic == "rts" || stmt->instr.mnemonic == "rtn" || stmt->instr.mnemonic == "rti") isDeadCode = true;
                }
            } else if (stmt->type == AssemblerParser::Statement::DIRECTIVE) {
                if (!isDeadCode || stmt->dir.name == "org") {
                    if (stmt->dir.name == "var") { if (stmt->dir.varType == Directive::ASSIGN) parser->symbolTable[stmt->dir.varName].value = parser->evaluateExpressionAt(stmt->dir.tokenIndex, stmt->scopePrefix); else if (stmt->dir.varType == Directive::INC) parser->symbolTable[stmt->dir.varName].value++; else if (stmt->dir.varType == Directive::DEC) parser->symbolTable[stmt->dir.varName].value--; }
                    else if (stmt->dir.name == "cleanup") { if (currentPass2Proc) currentPass2Proc->totalParamSize += parser->evaluateExpressionAt(stmt->dir.tokenIndex, stmt->scopePrefix); }
                    else if (stmt->dir.name == "byte") for (const auto& a : stmt->dir.arguments) e.emitByte((uint8_t)parseNumericLiteral(a));
                    else if (stmt->dir.name == "word") for (const auto& a : stmt->dir.arguments) e.emitWord((uint16_t)parseNumericLiteral(a));
                    else if (stmt->dir.name == "dword" || stmt->dir.name == "long") for (const auto& a : stmt->dir.arguments) { uint32_t v = parseNumericLiteral(a); e.emitWord(v & 0xFFFF); e.emitWord(v >> 16); }
                    else if (stmt->dir.name == "float") for (const auto& a : stmt->dir.arguments) { double v = std::stod(a); std::vector<uint8_t> enc = encodeFloat(v); for (uint8_t eb : enc) e.emitByte(eb); }
                    else if (stmt->dir.name == "text") for (char c : stmt->dir.arguments[0]) e.emitByte(toPetscii(c));
                    else if (stmt->dir.name == "ascii") for (char c : stmt->dir.arguments[0]) e.emitByte((uint8_t)c);
                    else if (stmt->dir.name == "res") {
                        uint32_t count = parser->evaluateExpressionAt(stmt->dir.tokenIndex, stmt->scopePrefix);
                        uint8_t fill = 0;
                        if (stmt->dir.arguments.size() > 1) fill = (uint8_t)parseNumericLiteral(stmt->dir.arguments[1]);
                        for (uint32_t i = 0; i < count; ++i) e.emitByte(fill);
                    }
                    else if (stmt->dir.name == "align" || stmt->dir.name == "balign") {
                        uint8_t fill = 0;
                        if (stmt->dir.arguments.size() > 1) fill = (uint8_t)parseNumericLiteral(stmt->dir.arguments[1]);
                        for (int i = 0; i < stmt->size; ++i) e.emitByte(fill);
                    }
                }
            }
        }
    }
}
