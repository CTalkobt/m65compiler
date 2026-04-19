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
    std::shared_ptr<AssemblerParser::ProcContext> currentPass2Proc;
    bool isDeadCode = false;
    std::vector<std::shared_ptr<AssemblerParser::ProcContext>> pass2ProcStack;
    M65Emitter e(binary, parser->getZPStart());

    for (auto& [name, symbol] : parser->symbolTable) if (symbol.isVariable) symbol.value = symbol.initialValue;

    for (auto& stmt : parser->statements) {
        if (!stmt->label.empty()) isDeadCode = false;

        if (stmt->type == AssemblerParser::Statement::EXPR) {
            if (!isDeadCode) AssemblerSimulatedOps::emitExpressionCode(parser, binary, stmt->exprTarget, stmt->exprTokenIndex, stmt->scopePrefix);
            continue;
        }
        if (stmt->type == AssemblerParser::Statement::MUL) {
            if (!isDeadCode) AssemblerSimulatedOps::emitMulCode(parser, binary, stmt->mulWidth, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix);
            continue;
        }
        if (stmt->type == AssemblerParser::Statement::DIV) {
            if (!isDeadCode) AssemblerSimulatedOps::emitDivCode(parser, binary, stmt->mulWidth, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix);
            continue;
        }
        if (stmt->type == AssemblerParser::Statement::STACK_INC) {
            if (!isDeadCode) AssemblerSimulatedOps::emitStackIncDecCode(parser, binary, true, stmt->instr.operandTokenIndex, stmt->scopePrefix);
            continue;
        }
        if (stmt->type == AssemblerParser::Statement::STACK_DEC) {
            if (!isDeadCode) AssemblerSimulatedOps::emitStackIncDecCode(parser, binary, false, stmt->instr.operandTokenIndex, stmt->scopePrefix);
            continue;
        }
        if (stmt->type == AssemblerParser::Statement::STACK_INC8) {
            if (!isDeadCode) AssemblerSimulatedOps::emitStackIncDec8Code(parser, binary, true, stmt->instr.operandTokenIndex, stmt->scopePrefix);
            continue;
        }
        if (stmt->type == AssemblerParser::Statement::STACK_DEC8) {
            if (!isDeadCode) AssemblerSimulatedOps::emitStackIncDec8Code(parser, binary, false, stmt->instr.operandTokenIndex, stmt->scopePrefix);
            continue;
        }
        if (stmt->type == AssemblerParser::Statement::ADD16) {
            if (!isDeadCode) AssemblerSimulatedOps::emitAddSub16Code(parser, binary, true, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix);
            continue;
        }
        if (stmt->type == AssemblerParser::Statement::SUB16) {
            if (!isDeadCode) AssemblerSimulatedOps::emitAddSub16Code(parser, binary, false, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix);
            continue;
        }
        if (stmt->type == AssemblerParser::Statement::AND16 || stmt->type == AssemblerParser::Statement::ORA16 || stmt->type == AssemblerParser::Statement::EOR16) {
            if (!isDeadCode) AssemblerSimulatedOps::emitBitwise16Code(parser, binary, stmt->instr.mnemonic, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix);
            continue;
        }
        if (stmt->type == AssemblerParser::Statement::CPW) {
            if (!isDeadCode) AssemblerSimulatedOps::emitCPWCode(parser, binary, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix);
            continue;
        }
        if (stmt->type == AssemblerParser::Statement::LDW) {
            if (!isDeadCode) AssemblerSimulatedOps::emitLDWCode(parser, binary, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix, stmt->instr.mnemonic == "LDW.SP");
            continue;
        }
        if (stmt->type == AssemblerParser::Statement::STW) {
            if (!isDeadCode) AssemblerSimulatedOps::emitSTWCode(parser, binary, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix, stmt->instr.mnemonic == "STW.SP");
            continue;
        }
        if (stmt->type == AssemblerParser::Statement::FILL) {
            if (!isDeadCode) AssemblerSimulatedOps::emitFillCode(parser, binary, stmt->instr.operandTokenIndex, stmt->scopePrefix, stmt->instr.mnemonic == "FILL.SP");
            continue;
        }
        if (stmt->type == AssemblerParser::Statement::COPY) {
            if (!isDeadCode) AssemblerSimulatedOps::emitMoveCode(parser, binary, stmt->instr.operandTokenIndex, stmt->scopePrefix, stmt->instr.mnemonic == "MOVE.SP");
            continue;
        }
        if (stmt->type == AssemblerParser::Statement::SWAP) {
            if (!isDeadCode) AssemblerSimulatedOps::emitSwapCode(parser, binary, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix);
            continue;
        }
        if (stmt->type == AssemblerParser::Statement::NEG16) {
            if (!isDeadCode) AssemblerSimulatedOps::emitNegNot16Code(parser, binary, true, stmt->instr.operand, stmt->instr.operandTokenIndex, stmt->scopePrefix);
            continue;
        }
        if (stmt->type == AssemblerParser::Statement::NOT16) {
            if (!isDeadCode) AssemblerSimulatedOps::emitNegNot16Code(parser, binary, false, stmt->instr.operand, stmt->instr.operandTokenIndex, stmt->scopePrefix);
            continue;
        }
        if (stmt->type == AssemblerParser::Statement::CHKZERO8) {
            if (!isDeadCode) AssemblerSimulatedOps::emitChkZeroCode(parser, binary, false, false, stmt->instr.operandTokenIndex, stmt->scopePrefix);
            continue;
        }
        if (stmt->type == AssemblerParser::Statement::CHKZERO16) {
            if (!isDeadCode) AssemblerSimulatedOps::emitChkZeroCode(parser, binary, true, false, stmt->instr.operandTokenIndex, stmt->scopePrefix);
            continue;
        }
        if (stmt->type == AssemblerParser::Statement::CHKNONZERO8) {
            if (!isDeadCode) AssemblerSimulatedOps::emitChkZeroCode(parser, binary, false, true, stmt->instr.operandTokenIndex, stmt->scopePrefix);
            continue;
        }
        if (stmt->type == AssemblerParser::Statement::CHKNONZERO16) {
            if (!isDeadCode) AssemblerSimulatedOps::emitChkZeroCode(parser, binary, true, true, stmt->instr.operandTokenIndex, stmt->scopePrefix);
            continue;
        }
        if (stmt->type == AssemblerParser::Statement::BRANCH16) {
            if (!isDeadCode) AssemblerSimulatedOps::emitBranch16Code(parser, binary, stmt->instr.operandTokenIndex, stmt->scopePrefix);
            continue;
        }
        if (stmt->type == AssemblerParser::Statement::SELECT) {
            if (!isDeadCode) AssemblerSimulatedOps::emitSelectCode(parser, binary, stmt->instr.operandTokenIndex, stmt->scopePrefix);
            continue;
        }
        if (stmt->type == AssemblerParser::Statement::PTRSTACK) {
            if (!isDeadCode) AssemblerSimulatedOps::emitPtrStackCode(parser, binary, stmt->instr.operandTokenIndex, stmt->scopePrefix);
            continue;
        }
        if (stmt->type == AssemblerParser::Statement::PTRDEREF) {
            if (!isDeadCode) AssemblerSimulatedOps::emitPtrDerefCode(parser, binary, stmt->instr.operandTokenIndex, stmt->scopePrefix);
            continue;
        }
        if (stmt->type == AssemblerParser::Statement::LDWF || stmt->type == AssemblerParser::Statement::STWF || stmt->type == AssemblerParser::Statement::INCF || stmt->type == AssemblerParser::Statement::DECF) {
            if (!isDeadCode) AssemblerSimulatedOps::emitFlatMemoryCode(parser, binary, stmt->instr.mnemonic, stmt->instr.operandTokenIndex, stmt->scopePrefix);
            continue;
        }
        if (stmt->type == AssemblerParser::Statement::PHW_STACK) {
            if (!isDeadCode) AssemblerSimulatedOps::emitPHWStackCode(parser, binary, stmt->instr.operandTokenIndex, stmt->scopePrefix);
            continue;
        }
        if (stmt->type == AssemblerParser::Statement::ZERO) {
            if (!isDeadCode) AssemblerSimulatedOps::emitZeroCode(parser, binary, stmt->instr.operandTokenIndex, stmt->scopePrefix);
            continue;
        }

        if (stmt->type == AssemblerParser::Statement::BASIC_UPSTART) {
            if (!isDeadCode) {
                uint32_t addr = parser->evaluateExpressionAt(stmt->basicUpstartTokenIndex, stmt->scopePrefix);
                std::string aS = std::to_string(addr);
                while (aS.length() < 4) aS = " " + aS;
                if (aS.length() > 4) aS = aS.substr(aS.length() - 4);
                uint16_t nL = (uint16_t)(stmt->address + 12 - 2);
                binary.push_back(nL & 0xFF); binary.push_back(nL >> 8);
                binary.push_back(0x0A); binary.push_back(0x00); binary.push_back(0x9E);
                for (char c : aS) binary.push_back((uint8_t)c);
                binary.push_back(0x00); binary.push_back(0x00); binary.push_back(0x00);
            }
            continue;
        }

        if (stmt->type == AssemblerParser::Statement::INSTRUCTION) {
            if (stmt->instr.mnemonic == "PROC") {
                pass2ProcStack.push_back(currentPass2Proc);
                currentPass2Proc = stmt->procCtx;
                continue;
            } else if (stmt->instr.mnemonic == "ENDPROC") {
                if (!isDeadCode) { binary.push_back(0x62); binary.push_back((uint8_t)stmt->instr.procParamSize); }
                currentPass2Proc = pass2ProcStack.empty() ? nullptr : pass2ProcStack.back();
                if (!pass2ProcStack.empty()) pass2ProcStack.pop_back();
                isDeadCode = false;
                continue;
            } else if (stmt->instr.mnemonic == "CALL") {
                if (!isDeadCode) {
                    for (const auto& arg : stmt->instr.callArgs) {
                        bool isB = (arg.size() >= 2 && arg.substr(0, 2) == "B#");
                        std::string v = isB ? arg.substr(2) : ( (arg.size() >= 2 && arg.substr(0, 2) == "W#") ? arg.substr(2) : (arg[0] == '#' ? arg.substr(1) : arg));
                        uint32_t val;
                        Symbol* sym = parser->resolveSymbol(v, stmt->scopePrefix);
                        if (sym) val = sym->value; else val = parseNumericLiteral(v);
                        if (!isB && !arg.empty() && arg[0] != '#' && arg.size() >= 2 && arg.substr(0,2) != "W#" && sym) isB = (sym->size == 1);
                        if (isB) { e.lda_imm(val & 0xFF); e.pha(); } else { binary.push_back(0xF2); binary.push_back(val & 0xFF); binary.push_back(val >> 8); }
                    }
                    Symbol* sym = parser->resolveSymbol(stmt->instr.operand, stmt->scopePrefix);
                    uint32_t target = sym ? sym->value : 0;
                    binary.push_back(0x20); binary.push_back(target & 0xFF); binary.push_back(target >> 8);
                }
            } else { // Handle all other instructions
                if (!isDeadCode) {
                    AddressingMode resolvedMode = stmt->instr.mode;
                    if (stmt->instr.operandTokenIndex != -1 && (stmt->instr.mode == AddressingMode::BASE_PAGE || stmt->instr.mode == AddressingMode::ABSOLUTE ||
                        stmt->instr.mode == AddressingMode::BASE_PAGE_X || stmt->instr.mode == AddressingMode::ABSOLUTE_X ||
                        stmt->instr.mode == AddressingMode::BASE_PAGE_Y || stmt->instr.mode == AddressingMode::ABSOLUTE_Y)) {
                        uint32_t val = parser->evaluateExpressionAt(stmt->instr.operandTokenIndex, stmt->scopePrefix);
                        bool fitsIn8 = (val <= 0xFF);
                        bool forceAbs = (stmt->instr.mnemonic == "JSR" || stmt->instr.mnemonic == "JMP");
                        if (stmt->instr.mode == AddressingMode::BASE_PAGE || stmt->instr.mode == AddressingMode::ABSOLUTE) resolvedMode = (fitsIn8 && !forceAbs) ? AddressingMode::BASE_PAGE : AddressingMode::ABSOLUTE;
                        else if (stmt->instr.mode == AddressingMode::BASE_PAGE_X || stmt->instr.mode == AddressingMode::ABSOLUTE_X) resolvedMode = (fitsIn8 && !forceAbs) ? AddressingMode::BASE_PAGE_X : AddressingMode::ABSOLUTE_X;
                        else if (stmt->instr.mode == AddressingMode::BASE_PAGE_Y || stmt->instr.mode == AddressingMode::ABSOLUTE_Y) resolvedMode = (fitsIn8 && !forceAbs) ? AddressingMode::BASE_PAGE_Y : AddressingMode::ABSOLUTE_Y;
                    }
                    bool isQuad = (stmt->instr.mnemonic.size() > 1 && stmt->instr.mnemonic.back() == 'Q' && stmt->instr.mnemonic != "LDQ" && stmt->instr.mnemonic != "STQ" && stmt->instr.mnemonic != "BEQ" && stmt->instr.mnemonic != "BNE" && stmt->instr.mnemonic != "BRA" && stmt->instr.mnemonic != "BSR");
                    if (isQuad) { binary.push_back(0x42); binary.push_back(0x42); }
                    if (resolvedMode == AddressingMode::FLAT_INDIRECT_Z) e.eom();


                    if (stmt->instr.mnemonic == "ASW" && resolvedMode == AddressingMode::ABSOLUTE) {
                        uint32_t addr = parser->evaluateExpressionAt(stmt->instr.operandTokenIndex, stmt->scopePrefix);
                        e.asw_abs(addr);
                        continue;                    } else if (stmt->instr.mnemonic == "ROW" && resolvedMode == AddressingMode::ABSOLUTE) {
                        uint32_t addr = parser->evaluateExpressionAt(stmt->instr.operandTokenIndex, stmt->scopePrefix);
                        e.row_abs(addr);
                        continue;                    } else { // Handle generic instructions and their operands
                        bool isBranch = (stmt->instr.mnemonic == "BEQ" || stmt->instr.mnemonic == "BNE" || stmt->instr.mnemonic == "BRA" || stmt->instr.mnemonic == "BCC" || stmt->instr.mnemonic == "BCS" || stmt->instr.mnemonic == "BPL" || stmt->instr.mnemonic == "BMI" || stmt->instr.mnemonic == "BVC" || stmt->instr.mnemonic == "BVS" || stmt->instr.mnemonic == "BSR");
                        if (!isBranch) {
                            uint8_t op = AssemblerOpcodeDatabase::getOpcode(stmt->instr.mnemonic, resolvedMode);
                            if (op == 0 && stmt->instr.mnemonic != "BRK") {
                                std::string errorMsg = "Invalid instruction or addressing mode: " + stmt->instr.mnemonic + " " + AssemblerOpcodeDatabase::AddressingModeToString(resolvedMode);
                                auto validModes = AssemblerOpcodeDatabase::getValidAddressingModes(stmt->instr.mnemonic);
                                if (validModes.empty()) errorMsg = "Unknown mnemonic: " + stmt->instr.mnemonic;
                                else { errorMsg += ". Valid addressing modes for " + stmt->instr.mnemonic + " are: "; for (size_t i = 0; i < validModes.size(); ++i) { errorMsg += AssemblerOpcodeDatabase::AddressingModeToString(validModes[i]); if (i < validModes.size() - 1) errorMsg += ", "; } }
                                throw std::runtime_error(errorMsg + " at line " + std::to_string(stmt->line));
                            }
                            binary.push_back(op);
                            if (resolvedMode == AddressingMode::IMMEDIATE || resolvedMode == AddressingMode::STACK_RELATIVE || resolvedMode == AddressingMode::BASE_PAGE_INDIRECT_Z || resolvedMode == AddressingMode::FLAT_INDIRECT_Z || resolvedMode == AddressingMode::BASE_PAGE_INDIRECT_SP_Y || resolvedMode == AddressingMode::BASE_PAGE_X_INDIRECT || resolvedMode == AddressingMode::BASE_PAGE_INDIRECT_Y || resolvedMode == AddressingMode::BASE_PAGE || resolvedMode == AddressingMode::BASE_PAGE_X || resolvedMode == AddressingMode::BASE_PAGE_Y) {
                                uint32_t v = parser->evaluateExpressionAt(stmt->instr.operandTokenIndex, stmt->scopePrefix);
                                binary.push_back((uint8_t)v);
                            } else if (resolvedMode == AddressingMode::ABSOLUTE || resolvedMode == AddressingMode::ABSOLUTE_X || resolvedMode == AddressingMode::ABSOLUTE_Y || resolvedMode == AddressingMode::ABSOLUTE_INDIRECT || resolvedMode == AddressingMode::ABSOLUTE_X_INDIRECT || resolvedMode == AddressingMode::IMMEDIATE16) {
                                uint32_t a = parser->evaluateExpressionAt(stmt->instr.operandTokenIndex, stmt->scopePrefix);
                                binary.push_back(a & 0xFF); binary.push_back(a >> 8);
                            }
                        } else { // Branch instructions (already handled below, this is just for structure)
                            Symbol* sym = parser->resolveSymbol(stmt->instr.operand, stmt->scopePrefix);
                            uint32_t t = sym ? sym->value : 0;
                            if (stmt->instr.mnemonic == "BSR") {
                                int32_t off = (int32_t)t - (int32_t)(stmt->address + 3);
                                binary.push_back(0x63); binary.push_back(off & 0xFF); binary.push_back(off >> 8);
                            } else if (stmt->size == 2) {
                                int32_t off2 = (int32_t)t - (int32_t)(stmt->address + 2);
                                binary.push_back(AssemblerOpcodeDatabase::getOpcode(stmt->instr.mnemonic, AddressingMode::RELATIVE));
                                binary.push_back((uint8_t)(int8_t)off2);
                            } else {
                                int32_t off3 = (int32_t)t - (int32_t)(stmt->address + 3);
                                binary.push_back(AssemblerOpcodeDatabase::getOpcode(stmt->instr.mnemonic, AddressingMode::RELATIVE16));
                                binary.push_back(off3 & 0xFF); binary.push_back(off3 >> 8);
                            }
                        }
                    }
                    if (stmt->instr.mode == AddressingMode::BASE_PAGE_RELATIVE) {
                        Symbol* sym = parser->resolveSymbol(stmt->instr.operand, stmt->scopePrefix);
                        uint32_t v = sym ? sym->value : parseNumericLiteral(stmt->instr.operand);
                        binary.push_back((uint8_t)v);
                        Symbol* tsym = parser->resolveSymbol(stmt->instr.bitBranchTarget, stmt->scopePrefix);
                        uint32_t t = tsym ? tsym->value : 0;
                        int32_t off = (int32_t)t - (int32_t)(stmt->address + 3);
                        binary.push_back((uint8_t)off);
                    } else if (stmt->instr.mnemonic == "RTN") {
                        Symbol* sym = parser->resolveSymbol(stmt->instr.operand, stmt->scopePrefix);
                        uint32_t v = sym ? sym->value : parseNumericLiteral(stmt->instr.operand);
                        if (v == 0) binary.push_back(0x60); else { binary.push_back(0x62); binary.push_back((uint8_t)v); }
                    }
                }
                if (stmt->instr.mnemonic == "RTS" || stmt->instr.mnemonic == "RTN" || stmt->instr.mnemonic == "RTI") isDeadCode = true;
            }
        } else if (stmt->type == AssemblerParser::Statement::DIRECTIVE) {
            if (!isDeadCode || stmt->dir.name == "org") {
                if (stmt->dir.name == "var") { if (stmt->dir.varType == Directive::ASSIGN) parser->symbolTable[stmt->dir.varName].value = parser->evaluateExpressionAt(stmt->dir.tokenIndex, stmt->scopePrefix); else if (stmt->dir.varType == Directive::INC) parser->symbolTable[stmt->dir.varName].value++; else if (stmt->dir.varType == Directive::DEC) parser->symbolTable[stmt->dir.varName].value--; }
                else if (stmt->dir.name == "cleanup") { if (currentPass2Proc) currentPass2Proc->totalParamSize += parser->evaluateExpressionAt(stmt->dir.tokenIndex, stmt->scopePrefix); }
                else if (stmt->dir.name == "byte") for (const auto& a : stmt->dir.arguments) binary.push_back((uint8_t)parseNumericLiteral(a));
                else if (stmt->dir.name == "word") for (const auto& a : stmt->dir.arguments) { uint32_t v = parseNumericLiteral(a); binary.push_back(v & 0xFF); binary.push_back(v >> 8); }
                else if (stmt->dir.name == "dword" || stmt->dir.name == "long") for (const auto& a : stmt->dir.arguments) { uint32_t v = parseNumericLiteral(a); binary.push_back(v & 0xFF); binary.push_back(v >> 8); binary.push_back(v >> 16); binary.push_back(v >> 24); }
                else if (stmt->dir.name == "float") for (const auto& a : stmt->dir.arguments) { double v = std::stod(a); std::vector<uint8_t> enc = encodeFloat(v); for (uint8_t eb : enc) binary.push_back(eb); }
                else if (stmt->dir.name == "text") for (char c : stmt->dir.arguments[0]) binary.push_back(toPetscii(c));
                else if (stmt->dir.name == "ascii") for (char c : stmt->dir.arguments[0]) binary.push_back((uint8_t)c);
            }
        }
    }
    return binary;
}
