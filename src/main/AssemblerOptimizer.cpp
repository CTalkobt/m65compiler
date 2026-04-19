#include "AssemblerOptimizer.hpp"
#include "AssemblerParser.hpp"
#include <algorithm>

void AssemblerOptimizer::optimize(AssemblerParser* parser) {
    struct RegState {
        bool known = false;
        std::string var; 
        AddressingMode mode;
        std::string imm; // Last immediate value loaded
    };
    RegState regA, regX, regY, regZ;

    auto invalidate = [&](RegState& r) { r.known = false; r.var = ""; r.imm = ""; };

    for (size_t i = 0; i < parser->statements.size(); ++i) {
        auto* s = parser->statements[i].get();
        if (s->deleted) continue;

        // Barrier: Non-dont-care labels reset all knowledge
        if (!s->label.empty() && s->label[0] != '@') {
            invalidate(regA); invalidate(regX); invalidate(regY); invalidate(regZ);
        }

        if (s->type == AssemblerParser::Statement::INSTRUCTION || s->type == AssemblerParser::Statement::LDW || s->type == AssemblerParser::Statement::STW) {
            std::string m = (s->type == AssemblerParser::Statement::INSTRUCTION) ? s->instr.mnemonic : (s->type == AssemblerParser::Statement::LDW ? "LDAX" : "STAX");
            AddressingMode mode = s->instr.mode;
            std::string op = s->instr.operand;
            
            if (s->type == AssemblerParser::Statement::LDW) {
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
                continue;
            }

            if (s->type == AssemblerParser::Statement::STW) {
                continue;
            }

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

            if (m == "LDY" && mode == AddressingMode::IMMEDIATE && op == "#$00" && regY.imm == "#$00") {
                s->deleted = true; s->size = 0; continue;
            }

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
                if (s->instr.operandTokenIndex != -1 && parser->tokens[s->instr.operandTokenIndex].type == AssemblerTokenType::IDENTIFIER) {
                    s->instr.mnemonic = "BRA";
                }
                invalidate(regA); invalidate(regX); invalidate(regY); invalidate(regZ);
            }
            else if (m == "PHA" || m == "PLA" || m == "PHX" || m == "PLX" || m == "PHY" || m == "PLY" || m == "PHZ" || m == "PLZ" || m == "PHW" || m == "JSR" || m == "CALL" || m == "RTN" || m == "RTS") {
                invalidate(regA); invalidate(regX); invalidate(regY); invalidate(regZ);
            }
            else {
                if (m == "ADC" || m == "SBC" || m == "AND" || m == "ORA" || m == "EOR" || m == "ASL" || m == "LSR" || m == "ROL" || m == "ROR" || m == "DEC" || m == "INC") {
                    invalidate(regA);
                }
                if (m == "INX" || m == "DEX") invalidate(regX);
                if (m == "INY" || m == "DEY") invalidate(regY);
                if (m == "INZ" || m == "DEZ") invalidate(regZ);
            }
        } else if (s->type == AssemblerParser::Statement::EXPR || s->type == AssemblerParser::Statement::MUL || s->type == AssemblerParser::Statement::DIV || s->type == AssemblerParser::Statement::STACK_INC || s->type == AssemblerParser::Statement::STACK_DEC) {
            invalidate(regA); invalidate(regX); invalidate(regY); invalidate(regZ);
        }
    }
}
