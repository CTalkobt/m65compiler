#include "AssemblerOptimizer.hpp"
#include "AssemblerParser.hpp"
#include <algorithm>
#include <map> // Required for std::map

bool AssemblerOptimizer::optimize(AssemblerParser* parser) {
    bool changed = false; // Flag to track if any optimization occurred
    struct RegState {
        bool known = false;
        std::string var; 
        AddressingMode mode;
        std::string imm; // Last immediate value loaded
    };
    RegState regA, regX, regY, regZ;

    // For tracking last value stored to a stack variable (e.g., _l_c)
    std::map<std::string, std::string> stackVarLastValue;

    auto invalidate = [&](RegState& r) { r.known = false; r.var = ""; r.imm = ""; };

    for (size_t i = 0; i < parser->statements.size(); ++i) {
        auto* s = parser->statements[i].get();
        if (s->deleted) continue;

        // Barrier: Non-dont-care labels reset all knowledge
        if (!s->label.empty() && s->label[0] != '@') {
            invalidate(regA); invalidate(regX); invalidate(regY); invalidate(regZ);
            stackVarLastValue.clear(); // Invalidate stack var knowledge as well
            changed = true; // Clearing state is a change
        }

        bool isStackStoreOptimizationCandidate = false;

        // Handle STW.SP (redundant store elimination)
        if (s->type == AssemblerParser::Statement::STW && s->instr.mnemonic == "STW.SP") {
            isStackStoreOptimizationCandidate = true;
            std::string immediateValue = s->instr.operand; // e.g., "#$0000"
            std::string varName = parser->tokens[s->exprTokenIndex].value; // e.g., "_l_c"

            if (stackVarLastValue.count(varName) && stackVarLastValue[varName] == immediateValue) {
                // Redundant store: delete this instruction
                s->deleted = true;
                s->size = 0;
                changed = true;
            } else {
                // Not redundant, or first store: update last stored value
                stackVarLastValue[varName] = immediateValue;
            }
        }

        // Invalidate stack variable state if this instruction could modify memory
        // or render our knowledge of stack variables stale, UNLESS it was a STW.SP that was optimized.
        if (!isStackStoreOptimizationCandidate) {
            if (s->type == AssemblerParser::Statement::INSTRUCTION) {
                std::string m = s->instr.mnemonic;
                // Opcodes that write to memory or change control flow unpredictably
                if (m == "STA" || m == "STX" || m == "STY" || m == "STZ" ||
                    m == "PHA" || m == "PLA" || m == "PHX" || m == "PLX" || m == "PHY" || m == "PLY" ||
                    m == "PHZ" || m == "PLZ" || m == "PHW" ||
                    m == "JSR" || m == "RTS" || m == "RTN" || m == "RTI" ||
                    m == "JMP" || m == "CALL" || m == "BSR" ||
                    m == "ADC" || m == "SBC" || m == "AND" || m == "ORA" || m == "EOR" ||
                    m == "ASL" || m == "LSR" || m == "ROL" || m == "ROR" ||
                    m == "DEC" || m == "INC" ||
                    m == "INX" || m == "DEX" || m == "INY" || m == "DEY" || m == "INZ" || m == "DEZ" 
                   ) {
                    stackVarLastValue.clear();
                    changed = true; // Clearing state is a change
                }
            } else if (s->type == AssemblerParser::Statement::LDW || s->type == AssemblerParser::Statement::STW || 
                       s->type == AssemblerParser::Statement::EXPR || s->type == AssemblerParser::Statement::MUL ||
                       s->type == AssemblerParser::Statement::DIV || s->type == AssemblerParser::Statement::STACK_INC ||
                       s->type == AssemblerParser::Statement::STACK_DEC || s->type == AssemblerParser::Statement::CPW ||
                       s->type == AssemblerParser::Statement::FILL || s->type == AssemblerParser::Statement::COPY ||
                       s->type == AssemblerParser::Statement::SWAP || s->type == AssemblerParser::Statement::NEG16 ||
                       s->type == AssemblerParser::Statement::NOT16 || s->type == AssemblerParser::Statement::CHKZERO8 ||
                       s->type == AssemblerParser::Statement::CHKZERO16 || s->type == AssemblerParser::Statement::CHKNONZERO8 ||
                       s->type == AssemblerParser::Statement::CHKNONZERO16 || s->type == AssemblerParser::Statement::BRANCH16 ||
                       s->type == AssemblerParser::Statement::SELECT || s->type == AssemblerParser::Statement::PTRSTACK ||
                       s->type == AssemblerParser::Statement::PTRDEREF || s->type == AssemblerParser::Statement::LDWF ||
                       s->type == AssemblerParser::Statement::STWF || s->type == AssemblerParser::Statement::INCF ||
                       s->type == AssemblerParser::Statement::DECF || s->type == AssemblerParser::Statement::PHW_STACK ||
                       s->type == AssemblerParser::Statement::ASR16 || s->type == AssemblerParser::Statement::ZERO
                      ) {
                stackVarLastValue.clear();
                changed = true; // Clearing state is a change
            }
        }

        // Existing register optimizations (keep these as they are, but make sure they don't interfere with stackVarLastValue)
        if (s->type == AssemblerParser::Statement::INSTRUCTION || s->type == AssemblerParser::Statement::LDW) {
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
                changed = true; // Register state change
                continue;
            }

            if (s->type == AssemblerParser::Statement::STW) { 
                changed = true; // Register state change
                continue; 
            }

            if (m == "LDA" && ( (regA.known && regA.mode == mode && regA.var == op) || (mode == AddressingMode::IMMEDIATE && regA.imm == op) )) {
                s->deleted = true; s->size = 0; changed = true; continue;
            }
            if (m == "LDX" && ( (regX.known && regX.mode == mode && regX.var == op) || (mode == AddressingMode::IMMEDIATE && regX.imm == op) )) {
                s->deleted = true; s->size = 0; changed = true; continue;
            }
            if (m == "LDY" && ( (regY.known && regY.mode == mode && regY.var == op) || (mode == AddressingMode::IMMEDIATE && regY.imm == op) )) {
                s->deleted = true; s->size = 0; changed = true; continue;
            }
            if (m == "LDZ" && ( (regZ.known && regZ.mode == mode && regZ.var == op) || (mode == AddressingMode::IMMEDIATE && regZ.imm == op) )) {
                s->deleted = true; s->size = 0; changed = true; continue;
            }

            if (m == "LDY" && mode == AddressingMode::IMMEDIATE && op == "#$00" && regY.imm == "#$00") {
                s->deleted = true; s->size = 0; changed = true; continue;
            }

            if (m == "LDA") { 
                regA.known = true; regA.mode = mode; regA.var = op; 
                if (mode == AddressingMode::IMMEDIATE) regA.imm = op; else regA.imm = "";
                changed = true;
            }
            else if (m == "STA") { 
                if (mode != AddressingMode::IMMEDIATE) { regA.known = true; regA.mode = mode; regA.var = op; }
                changed = true; // Register state change
            }
            else if (m == "LDX") { 
                regX.known = true; regX.mode = mode; regX.var = op; 
                if (mode == AddressingMode::IMMEDIATE) regX.imm = op; else regX.imm = "";
                changed = true;
            }
            else if (m == "STX") { 
                if (mode != AddressingMode::IMMEDIATE) { regX.known = true; regX.mode = mode; regX.var = op; }
                changed = true; // Register state change
            }
            else if (m == "LDY") { 
                regY.known = true; regY.mode = mode; regY.var = op; 
                if (mode == AddressingMode::IMMEDIATE) regY.imm = op; else regY.imm = "";
                changed = true;
            }
            else if (m == "STY") { 
                if (mode != AddressingMode::IMMEDIATE) { regY.known = true; regY.mode = mode; regY.var = op; }
                changed = true; // Register state change
            }
            else if (m == "LDZ") { 
                regZ.known = true; regZ.mode = mode; regZ.var = op; 
                if (mode == AddressingMode::IMMEDIATE) regZ.imm = op; else regZ.imm = "";
                changed = true;
            }
            else if (m == "STZ") { 
                if (mode != AddressingMode::IMMEDIATE) { regZ.known = true; regZ.mode = mode; regZ.var = op; }
                changed = true; // Register state change
            }
            else if (m == "TAX") { regX = regA; changed = true; }
            else if (m == "TXA") { regA = regX; changed = true; }
            else if (m == "TAY") { regY = regA; changed = true; }
            else if (m == "TYA") { regA = regY; changed = true; }
            else if (m == "TAZ") { regZ = regA; changed = true; }
            else if (m == "TZA") { regA = regZ; changed = true; }
            else if (m == "JMP" && mode == AddressingMode::ABSOLUTE) {
                if (s->instr.operandTokenIndex != -1 && parser->tokens[s->instr.operandTokenIndex].type == AssemblerTokenType::IDENTIFIER) {
                    s->instr.mnemonic = "BRA";
                    changed = true;
                }
                invalidate(regA); invalidate(regX); invalidate(regY); invalidate(regZ);
                changed = true; // Invalidate is a change
            }
            else if (m == "PHA" || m == "PLA" || m == "PHX" || m == "PLX" || m == "PHY" || m == "PLY" || m == "PHZ" || m == "PLZ" || m == "PHW" || m == "JSR" || m == "CALL" || m == "RTN" || m == "RTS") {
                invalidate(regA); invalidate(regX); invalidate(regY); invalidate(regZ);
                changed = true; // Invalidate is a change
            }
            else {
                if (m == "ADC" || m == "SBC" || m == "AND" || m == "ORA" || m == "EOR" || m == "ASL" || m == "LSR" || m == "ROL" || m == "ROR" || m == "DEC" || m == "INC") {
                    invalidate(regA); changed = true; // Invalidate is a change
                }
                if (m == "INX" || m == "DEX") { invalidate(regX); changed = true; } // Invalidate is a change
                if (m == "INY" || m == "DEY") { invalidate(regY); changed = true; } // Invalidate is a change
                if (m == "INZ" || m == "DEZ") { invalidate(regZ); changed = true; } // Invalidate is a change
            }
        } else if (s->type == AssemblerParser::Statement::EXPR || s->type == AssemblerParser::Statement::MUL || s->type == AssemblerParser::Statement::DIV || s->type == AssemblerParser::Statement::STACK_INC || s->type == AssemblerParser::Statement::STACK_DEC || s->type == AssemblerParser::Statement::ADD16 || s->type == AssemblerParser::Statement::SUB16 || s->type == AssemblerParser::Statement::AND16 || s->type == AssemblerParser::Statement::ORA16 || s->type == AssemblerParser::Statement::EOR16 || s->type == AssemblerParser::Statement::CPW || s->type == AssemblerParser::Statement::FILL || s->type == AssemblerParser::Statement::COPY || s->type == AssemblerParser::Statement::SWAP || s->type == AssemblerParser::Statement::NEG16 || s->type == AssemblerParser::Statement::NOT16 || s->type == AssemblerParser::Statement::CHKZERO8 || s->type == AssemblerParser::Statement::CHKZERO16 || s->type == AssemblerParser::Statement::CHKNONZERO8 || s->type == AssemblerParser::Statement::CHKNONZERO16 || s->type == AssemblerParser::Statement::BRANCH16 || s->type == AssemblerParser::Statement::SELECT || s->type == AssemblerParser::Statement::PTRSTACK || s->type == AssemblerParser::Statement::PTRDEREF || s->type == AssemblerParser::Statement::LDWF || s->type == AssemblerParser::Statement::STWF || s->type == AssemblerParser::Statement::INCF || s->type == AssemblerParser::Statement::DECF || s->type == AssemblerParser::Statement::PHW_STACK || s->type == AssemblerParser::Statement::ASR16 || s->type == AssemblerParser::Statement::ZERO) {
            stackVarLastValue.clear();
            changed = true; // Clearing state is a change
        }
    }
    return changed;
} 