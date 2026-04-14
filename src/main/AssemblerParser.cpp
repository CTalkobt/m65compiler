#include "AssemblerParser.hpp"
#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <algorithm>

uint8_t toPetscii(char c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    if (c >= 'A' && c <= 'Z') return c + 32;
    return (uint8_t)c;
}

AssemblerParser::AssemblerParser(const std::vector<AssemblerToken>& tokens) : tokens(tokens), pos(0), pc(0) {
}

const AssemblerToken& AssemblerParser::peek() const { return tokens[pos]; }
const AssemblerToken& AssemblerParser::advance() { if (pos < tokens.size()) pos++; return tokens[pos - 1]; }
bool AssemblerParser::match(AssemblerTokenType type) { if (peek().type == type) { advance(); return true; } return false; }

const AssemblerToken& AssemblerParser::expect(AssemblerTokenType type, const std::string& message) {
    if (peek().type == type) return advance();
    throw std::runtime_error("Error at " + std::to_string(peek().line) + ":" + std::to_string(peek().column) + ": " + message + " (got " + peek().typeToString() + ")");
}

uint32_t AssemblerParser::parseNumericLiteral(const std::string& literal) {
    if (literal.empty()) return 0;
    try {
        if (literal[0] == '$') return std::stoul(literal.substr(1), nullptr, 16);
        if (literal[0] == '%') return std::stoul(literal.substr(1), nullptr, 2);
        if (literal[0] == '\'') return (uint8_t)literal[1];
        if (std::isdigit(literal[0])) return std::stoul(literal, nullptr, 0);
    } catch (...) {}
    return 0;
}

uint32_t AssemblerParser::evaluateExpressionAt(int index) {
    auto parseTerm = [&](int& idx) -> uint32_t {
        if ((size_t)idx >= tokens.size()) return 0;
        const auto& token = tokens[idx++];
        if (token.type == AssemblerTokenType::IDENTIFIER) {
            if (symbolTable.count(token.value)) return symbolTable[token.value].value;
            return 0;
        }
        if (token.type == AssemblerTokenType::HEX_LITERAL) return std::stoul(token.value, nullptr, 16);
        if (token.type == AssemblerTokenType::BINARY_LITERAL) return std::stoul(token.value, nullptr, 2);
        if (token.type == AssemblerTokenType::DECIMAL_LITERAL) return std::stoul(token.value, nullptr, 10);
        if (token.type == AssemblerTokenType::CHAR_LITERAL) return (uint8_t)token.value[0];
        return 0;
    };
    int current = index;
    uint32_t result = parseTerm(current);
    while ((size_t)current < tokens.size()) {
        if (tokens[current].type == AssemblerTokenType::PLUS) { current++; result += parseTerm(current); }
        else if (tokens[current].type == AssemblerTokenType::MINUS) { current++; result -= parseTerm(current); }
        else break;
    }
    return result;
}

void AssemblerParser::pass1() {
    while (peek().type != AssemblerTokenType::END_OF_FILE) {
        if (match(AssemblerTokenType::NEWLINE)) continue;
        Statement stmt = {};
        if (peek().type == AssemblerTokenType::IDENTIFIER) {
            std::string name = advance().value;
            if (match(AssemblerTokenType::COLON)) {
                stmt.label = name; symbolTable[name] = {pc, true, 2};
            } else if (match(AssemblerTokenType::EQUALS)) {
                uint32_t val = evaluateExpressionAt(pos);
                while (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) advance();
                symbolTable[name] = {val, false, (val > 255) ? 2 : 1};
                continue;
            } else { pos--; }
        }
        if (match(AssemblerTokenType::DIRECTIVE)) {
            stmt.dir.name = tokens[pos-1].value;
            stmt.dir.address = pc;
            if (stmt.dir.name == "var") {
                std::string varName = advance().value; stmt.dir.varName = varName;
                if (match(AssemblerTokenType::EQUALS)) {
                    stmt.dir.varType = Directive::ASSIGN; stmt.dir.tokenIndex = pos;
                    uint32_t val = evaluateExpressionAt(pos);
                    while (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) advance();
                    if (!symbolTable.count(varName)) symbolTable[varName] = {val, false, 2, true, val};
                    else symbolTable[varName].value = val;
                } else if (match(AssemblerTokenType::INCREMENT)) {
                    stmt.dir.varType = Directive::INC; if (symbolTable.count(varName)) symbolTable[varName].value++;
                } else if (match(AssemblerTokenType::DECREMENT)) {
                    stmt.dir.varType = Directive::DEC; if (symbolTable.count(varName)) symbolTable[varName].value--;
                }
                stmt.dir.size = 0;
            } else if (stmt.dir.name == "cleanup") {
                stmt.dir.tokenIndex = pos;
                uint32_t val = evaluateExpressionAt(pos);
                while (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) advance();
                if (currentProc) currentProc->totalParamSize += val;
                stmt.dir.size = 0;
            } else {
                while (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) {
                    if (match(AssemblerTokenType::COMMA)) continue;
                    stmt.dir.arguments.push_back(advance().value);
                }
                if (stmt.dir.name == "org") {
                    if (!stmt.dir.arguments.empty()) pc = parseNumericLiteral(stmt.dir.arguments[0]);
                } else { stmt.dir.size = calculateDirectiveSize(stmt.dir); pc += stmt.dir.size; }
            }
            statements.push_back(stmt);
        } else if (match(AssemblerTokenType::INSTRUCTION)) {
            stmt.isInstruction = true; stmt.instr.mnemonic = tokens[pos-1].value; stmt.instr.address = pc;
            if (stmt.instr.mnemonic == "PROC") {
                std::string procName = advance().value; stmt.label = procName; symbolTable[procName] = {pc, true, 2};
                ProcContext ctx; ctx.name = procName; ctx.totalParamSize = 0;
                std::vector<std::pair<std::string, int>> args;
                while (match(AssemblerTokenType::COMMA)) {
                    bool isByte = false;
                    if (peek().type == AssemblerTokenType::IDENTIFIER && (peek().value == "B" || peek().value == "W")) {
                        isByte = (advance().value == "B"); match(AssemblerTokenType::HASH);
                    }
                    std::string argName = advance().value; int size = isByte ? 1 : 2;
                    args.push_back({argName, size}); ctx.totalParamSize += size;
                }
                int currentOffset = 2;
                for (int i = args.size() - 1; i >= 0; --i) {
                    ctx.localArgs[args[i].first] = currentOffset;
                    ctx.localArgs["ARG" + std::to_string(i + 1)] = currentOffset;
                    symbolTable[args[i].first] = {(uint32_t)currentOffset, false, args[i].second, true, (uint32_t)currentOffset};
                    symbolTable["ARG" + std::to_string(i + 1)] = {(uint32_t)currentOffset, false, args[i].second, true, (uint32_t)currentOffset};
                    currentOffset += args[i].second;
                }
                procedures[pc] = ctx; currentProc = &procedures[pc]; stmt.instr.size = 0;
            } else if (stmt.instr.mnemonic == "ENDPROC") {
                if (currentProc) { stmt.instr.procParamSize = currentProc->totalParamSize; currentProc = nullptr; }
                stmt.instr.size = 2;
            } else if (stmt.instr.mnemonic == "CALL") {
                stmt.instr.operand = advance().value;
                while (match(AssemblerTokenType::COMMA)) {
                    if (match(AssemblerTokenType::HASH)) {
                        const auto& v = advance(); stmt.instr.callArgs.push_back(std::string("#") + (v.type == AssemblerTokenType::HEX_LITERAL ? "$" : "") + v.value);
                    } else if (peek().type == AssemblerTokenType::IDENTIFIER && (peek().value == "B" || peek().value == "W")) {
                        std::string p = advance().value; match(AssemblerTokenType::HASH);
                        const auto& v = advance(); stmt.instr.callArgs.push_back(p + "#" + (v.type == AssemblerTokenType::HEX_LITERAL ? "$" : "") + v.value);
                    } else {
                        const auto& v = advance(); stmt.instr.callArgs.push_back(std::string(v.type == AssemblerTokenType::HEX_LITERAL ? "$" : "") + v.value);
                    }
                }
                stmt.instr.size = calculateInstructionSize(stmt.instr);
            } else {
                if (match(AssemblerTokenType::HASH)) {
                    stmt.instr.mode = AddressingMode::IMMEDIATE;
                    const auto& v = advance();
                    stmt.instr.operand = (v.type == AssemblerTokenType::HEX_LITERAL ? "$" : "") + v.value;
                } else if (match(AssemblerTokenType::OPEN_PAREN)) {
                    const auto& v = advance(); stmt.instr.operand = (v.type == AssemblerTokenType::HEX_LITERAL ? "$" : "") + v.value;
                    if (match(AssemblerTokenType::COMMA)) {
                        std::string r = advance().value;
                        if (r == "X" || r == "x") { expect(AssemblerTokenType::CLOSE_PAREN, "Expected )"); stmt.instr.mode = AddressingMode::BASE_PAGE_X_INDIRECT; }
                        else if (r == "SP" || r == "sp") { expect(AssemblerTokenType::CLOSE_PAREN, "Expected )"); expect(AssemblerTokenType::COMMA, "Expected ,"); advance(); stmt.instr.mode = AddressingMode::BASE_PAGE_INDIRECT_SP_Y; }
                    } else {
                        expect(AssemblerTokenType::CLOSE_PAREN, "Expected )");
                        if (match(AssemblerTokenType::COMMA)) {
                            std::string r = advance().value;
                            if (r == "Y" || r == "y") stmt.instr.mode = AddressingMode::BASE_PAGE_INDIRECT_Y;
                            else if (r == "Z" || r == "z") stmt.instr.mode = AddressingMode::BASE_PAGE_INDIRECT_Z;
                        } else stmt.instr.mode = AddressingMode::ABSOLUTE_INDIRECT;
                    }
                } else if (match(AssemblerTokenType::OPEN_BRACKET)) {
                    const auto& v = advance(); stmt.instr.operand = (v.type == AssemblerTokenType::HEX_LITERAL ? "$" : "") + v.value;
                    expect(AssemblerTokenType::CLOSE_BRACKET, "Expected ]"); expect(AssemblerTokenType::COMMA, "Expected ,"); advance();
                    stmt.instr.mode = AddressingMode::FLAT_INDIRECT_Z;
                } else if (peek().type == AssemblerTokenType::IDENTIFIER || peek().type == AssemblerTokenType::HEX_LITERAL || peek().type == AssemblerTokenType::DECIMAL_LITERAL) {
                    const auto& v = advance(); stmt.instr.operand = (v.type == AssemblerTokenType::HEX_LITERAL ? "$" : "") + v.value;
                    if (match(AssemblerTokenType::COMMA)) {
                        std::string r = advance().value;
                        if (r == "X" || r == "x") stmt.instr.mode = AddressingMode::ABSOLUTE_X;
                        else if (r == "Y" || r == "y") stmt.instr.mode = AddressingMode::ABSOLUTE_Y;
                        else if (r == "S" || r == "s") stmt.instr.mode = AddressingMode::STACK_RELATIVE;
                    } else {
                        if (stmt.instr.mnemonic[0] == 'B' && stmt.instr.mnemonic != "BIT" && stmt.instr.mnemonic != "BRK") stmt.instr.mode = AddressingMode::RELATIVE;
                        else stmt.instr.mode = AddressingMode::ABSOLUTE;
                    }
                } else stmt.instr.mode = AddressingMode::IMPLIED;
                stmt.instr.size = calculateInstructionSize(stmt.instr);
            }
            pc += stmt.instr.size; statements.push_back(stmt);
        } else if (!stmt.label.empty()) statements.push_back(stmt);
        else advance();
    }
}

int AssemblerParser::calculateDirectiveSize(const Directive& dir) {
    if (dir.name == "byte") return dir.arguments.size();
    if (dir.name == "word") return dir.arguments.size() * 2;
    if (dir.name == "text" || dir.name == "ascii") return dir.arguments.empty() ? 0 : dir.arguments[0].length();
    return 0;
}

int AssemblerParser::calculateInstructionSize(const Instruction& instr) {
    if (instr.mnemonic == "PROC") return 0;
    if (instr.mnemonic == "ENDPROC") return 2;
    if (instr.mnemonic == "CALL") {
        int size = 3;
        for (const auto& arg : instr.callArgs) {
            bool isByte = (arg.size() > 2 && arg.substr(0, 2) == "B#");
            std::string v = isByte ? arg.substr(2) : (arg.substr(0, 2) == "W#" ? arg.substr(2) : (arg[0] == '#' ? arg.substr(1) : arg));
            if (!isByte && arg[0] != '#' && arg.substr(0, 2) != "W#" && symbolTable.count(v)) isByte = (symbolTable[v].size == 1);
            size += 3;
        }
        return size;
    }
    if (instr.mnemonic == "PHW") return 3;
    if (instr.mnemonic == "RTN") return 2;
    if (instr.mnemonic == "BSR") return 3;
    if (instr.mnemonic == "BRA" || (instr.mnemonic[0] == 'B' && instr.mnemonic.size() == 3)) return (instr.mode == AddressingMode::RELATIVE16) ? 3 : 2;
    if (instr.mnemonic == "LDQ" || instr.mnemonic == "STQ") return 4;
    switch (instr.mode) {
        case AddressingMode::IMPLIED: return 1;
        case AddressingMode::IMMEDIATE: case AddressingMode::RELATIVE: case AddressingMode::STACK_RELATIVE: return 2;
        case AddressingMode::FLAT_INDIRECT_Z: return 3; 
        case AddressingMode::BASE_PAGE_X_INDIRECT:
        case AddressingMode::BASE_PAGE_INDIRECT_Y:
        case AddressingMode::BASE_PAGE_INDIRECT_Z:
        case AddressingMode::BASE_PAGE_INDIRECT_SP_Y: return 2;
        default: return 3;
    }
}

uint8_t AssemblerParser::getOpcode(const std::string& m, AddressingMode mode) {
    if (m == "LDA") {
        if (mode == AddressingMode::IMMEDIATE) return 0xA9;
        if (mode == AddressingMode::ABSOLUTE_X) return 0xBD;
        if (mode == AddressingMode::STACK_RELATIVE) return 0xD2; 
        if (mode == AddressingMode::BASE_PAGE_INDIRECT_Z) return 0xB2;
        if (mode == AddressingMode::FLAT_INDIRECT_Z) return 0xB2; 
        if (mode == AddressingMode::BASE_PAGE_INDIRECT_SP_Y) return 0xE2;
        return 0xAD;
    }
    if (m == "LDX") return (mode == AddressingMode::IMMEDIATE) ? 0xA2 : 0xAE;
    if (m == "LDY") return (mode == AddressingMode::IMMEDIATE) ? 0xA0 : 0xAC;
    if (m == "LDZ") return (mode == AddressingMode::IMMEDIATE) ? 0xA3 : 0xAB;
    if (m == "STA") {
        if (mode == AddressingMode::BASE_PAGE_INDIRECT_Z) return 0x92;
        if (mode == AddressingMode::FLAT_INDIRECT_Z) return 0x92; 
        if (mode == AddressingMode::BASE_PAGE_INDIRECT_SP_Y) return 0x82;
        return 0x8D;
    }
    if (m == "JSR") return 0x20; if (m == "RTS") return 0x60;
    if (m == "RTN") return 0x62; if (m == "PHW") return 0xF2; if (m == "BEQ") return 0xF0;
    if (m == "BRA") return 0x80; if (m == "INX") return 0xE8; if (m == "INY") return 0xC8;
    if (m == "INZ") return 0x1B; if (m == "DEZ") return 0x3B;
    if (m == "PHZ") return 0xDB; if (m == "PLZ") return 0xFB;
    return 0;
}

std::vector<uint8_t> AssemblerParser::pass2() {
    std::vector<uint8_t> binary;
    ProcContext* currentPass2Proc = nullptr;
    for (auto& [name, symbol] : symbolTable) if (symbol.isVariable) symbol.value = symbol.initialValue;
    for (const auto& stmt : statements) {
        if (procedures.count(stmt.isInstruction ? stmt.instr.address : 0)) {
             if (stmt.isInstruction && stmt.instr.mnemonic == "PROC") currentPass2Proc = &procedures[stmt.instr.address];
        }
        if (stmt.isInstruction) {
            if (stmt.instr.mnemonic == "PROC") continue;
            else if (stmt.instr.mnemonic == "ENDPROC") {
                binary.push_back(0x62); binary.push_back((uint8_t)stmt.instr.procParamSize); currentPass2Proc = nullptr;
            } else if (stmt.instr.mnemonic == "CALL") {
                for (const auto& arg : stmt.instr.callArgs) {
                    bool isByte = (arg.size() > 2 && arg.substr(0, 2) == "B#");
                    std::string v = isByte ? arg.substr(2) : (arg.substr(0, 2) == "W#" ? arg.substr(2) : (arg[0] == '#' ? arg.substr(1) : arg));
                    uint32_t val;
                    if (currentPass2Proc && currentPass2Proc->localArgs.count(v)) val = currentPass2Proc->localArgs[v];
                    else if (symbolTable.count(v)) val = symbolTable[v].value;
                    else val = parseNumericLiteral(v);
                    if (!isByte && arg[0] != '#' && arg.substr(0,2) != "W#" && symbolTable.count(v)) isByte = (symbolTable[v].size == 1);
                    if (isByte) { binary.push_back(0xA9); binary.push_back(val & 0xFF); binary.push_back(0x48); }
                    else { binary.push_back(0xF2); binary.push_back(val & 0xFF); binary.push_back((val >> 8) & 0xFF); }
                }
                binary.push_back(0x20); uint32_t a = symbolTable[stmt.instr.operand].value;
                binary.push_back(a & 0xFF); binary.push_back((a >> 8) & 0xFF);
            } else {
                if (stmt.instr.mode == AddressingMode::FLAT_INDIRECT_Z) binary.push_back(0xEA); 
                binary.push_back(getOpcode(stmt.instr.mnemonic, stmt.instr.mode));
                if (stmt.instr.mode == AddressingMode::IMMEDIATE || stmt.instr.mode == AddressingMode::STACK_RELATIVE || 
                    stmt.instr.mode == AddressingMode::BASE_PAGE_INDIRECT_Z || stmt.instr.mode == AddressingMode::FLAT_INDIRECT_Z ||
                    stmt.instr.mode == AddressingMode::BASE_PAGE_INDIRECT_SP_Y || stmt.instr.mode == AddressingMode::BASE_PAGE_X_INDIRECT ||
                    stmt.instr.mode == AddressingMode::BASE_PAGE_INDIRECT_Y) {
                    uint32_t v;
                    if (currentPass2Proc && currentPass2Proc->localArgs.count(stmt.instr.operand)) v = currentPass2Proc->localArgs[stmt.instr.operand];
                    else if (symbolTable.count(stmt.instr.operand)) v = symbolTable[stmt.instr.operand].value;
                    else v = parseNumericLiteral(stmt.instr.operand);
                    binary.push_back((uint8_t)v);
                } else if (stmt.instr.mode == AddressingMode::ABSOLUTE || stmt.instr.mode == AddressingMode::ABSOLUTE_X || stmt.instr.mode == AddressingMode::ABSOLUTE_Y) {
                    uint32_t a = symbolTable.count(stmt.instr.operand) ? symbolTable[stmt.instr.operand].value : parseNumericLiteral(stmt.instr.operand);
                    binary.push_back(a & 0xFF); binary.push_back((a >> 8) & 0xFF);
                } else if (stmt.instr.mnemonic == "BEQ" || stmt.instr.mnemonic == "BRA" || stmt.instr.mnemonic == "BNE") {
                    uint32_t t = symbolTable[stmt.instr.operand].value;
                    binary.push_back((uint8_t)((int32_t)t - (int32_t)(stmt.instr.address + 2)));
                }
            }
        } else if (!stmt.dir.name.empty()) {
            if (stmt.dir.name == "var") {
                if (stmt.dir.varType == Directive::ASSIGN) symbolTable[stmt.dir.varName].value = evaluateExpressionAt(stmt.dir.tokenIndex);
                else if (stmt.dir.varType == Directive::INC) symbolTable[stmt.dir.varName].value++;
                else if (stmt.dir.varType == Directive::DEC) symbolTable[stmt.dir.varName].value--;
            } else if (stmt.dir.name == "cleanup") {
                if (currentPass2Proc) currentPass2Proc->totalParamSize += evaluateExpressionAt(stmt.dir.tokenIndex);
            } else if (stmt.dir.name == "byte") for (const auto& a : stmt.dir.arguments) binary.push_back(parseNumericLiteral(a));
            else if (stmt.dir.name == "text") for (char c : stmt.dir.arguments[0]) binary.push_back(toPetscii(c));
            else if (stmt.dir.name == "ascii") for (char c : stmt.dir.arguments[0]) binary.push_back((uint8_t)c);
        }
    }
    return binary;
}
