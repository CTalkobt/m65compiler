#include "AssemblerParser.hpp"
#include "AssemblerExpression.hpp"
#include "AssemblerOptimizer.hpp"
#include "AssemblerSimulatedOps.hpp"
#include "AssemblerGenerator.hpp"
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
    throw std::runtime_error("Invalid numeric literal: " + literal);
}
}


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
    AssemblerSimulatedOps::emitExpressionCode(this, binary, target, tokenIndex, scopePrefix);
}

void AssemblerParser::emitMulCode(std::vector<uint8_t>& binary, int width, const std::string& dest, int tokenIndex, const std::string& scopePrefix) {
    AssemblerSimulatedOps::emitMulCode(this, binary, width, dest, tokenIndex, scopePrefix);
}

void AssemblerParser::emitDivCode(std::vector<uint8_t>& binary, int width, const std::string& dest, int tokenIndex, const std::string& scopePrefix) {
    AssemblerSimulatedOps::emitDivCode(this, binary, width, dest, tokenIndex, scopePrefix);
}


void AssemblerParser::emitStackIncDecCode(std::vector<uint8_t>& binary, bool isInc, int tokenIndex, const std::string& scopePrefix) {
    AssemblerSimulatedOps::emitStackIncDecCode(this, binary, isInc, tokenIndex, scopePrefix);
}

void AssemblerParser::emitStackIncDec8Code(std::vector<uint8_t>& binary, bool isInc, int tokenIndex, const std::string& scopePrefix) {
    AssemblerSimulatedOps::emitStackIncDec8Code(this, binary, isInc, tokenIndex, scopePrefix);
}

void AssemblerParser::emitAddSub16Code(std::vector<uint8_t>& binary, bool isAdd, const std::string& dest, int tokenIndex, const std::string& scopePrefix) {
    AssemblerSimulatedOps::emitAddSub16Code(this, binary, isAdd, dest, tokenIndex, scopePrefix);
}

void AssemblerParser::emitBitwise16Code(std::vector<uint8_t>& binary, const std::string& mnemonic, const std::string& dest, int tokenIndex, const std::string& scopePrefix) {
    AssemblerSimulatedOps::emitBitwise16Code(this, binary, mnemonic, dest, tokenIndex, scopePrefix);
}

void AssemblerParser::emitCPWCode(std::vector<uint8_t>& binary, const std::string& src1, int tokenIndex, const std::string& scopePrefix) {
    AssemblerSimulatedOps::emitCPWCode(this, binary, src1, tokenIndex, scopePrefix);
}

void AssemblerParser::emitLDWCode(std::vector<uint8_t>& binary, const std::string& dest, int tokenIndex, const std::string& scopePrefix, bool forceStack) {
    AssemblerSimulatedOps::emitLDWCode(this, binary, dest, tokenIndex, scopePrefix, forceStack);
}

void AssemblerParser::emitSTWCode(std::vector<uint8_t>& binary, const std::string& src, int tokenIndex, const std::string& scopePrefix, bool forceStack) {
    AssemblerSimulatedOps::emitSTWCode(this, binary, src, tokenIndex, scopePrefix, forceStack);
}

void AssemblerParser::emitSwapCode(std::vector<uint8_t>& binary, const std::string& r1, int tokenIndex, const std::string& scopePrefix) {
    AssemblerSimulatedOps::emitSwapCode(this, binary, r1, tokenIndex, scopePrefix);
}

void AssemblerParser::emitZeroCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix) {
    AssemblerSimulatedOps::emitZeroCode(this, binary, tokenIndex, scopePrefix);
}

void AssemblerParser::emitNegNot16Code(std::vector<uint8_t>& binary, bool isNeg, const std::string& operand, int tokenIndex, const std::string& scopePrefix) {
    AssemblerSimulatedOps::emitNegNot16Code(this, binary, isNeg, operand, tokenIndex, scopePrefix);
}

void AssemblerParser::emitChkZeroCode(std::vector<uint8_t>& binary, bool is16, bool isInverse, int tokenIndex, const std::string& scopePrefix) {
    AssemblerSimulatedOps::emitChkZeroCode(this, binary, is16, isInverse, tokenIndex, scopePrefix);
}

void AssemblerParser::emitBranch16Code(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix) {
    AssemblerSimulatedOps::emitBranch16Code(this, binary, tokenIndex, scopePrefix);
}

void AssemblerParser::emitSelectCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix) {
    AssemblerSimulatedOps::emitSelectCode(this, binary, tokenIndex, scopePrefix);
}

void AssemblerParser::emitPtrStackCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix) {
    AssemblerSimulatedOps::emitPtrStackCode(this, binary, tokenIndex, scopePrefix);
}

void AssemblerParser::emitPtrDerefCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix) {
    AssemblerSimulatedOps::emitPtrDerefCode(this, binary, tokenIndex, scopePrefix);
}

void AssemblerParser::emitFillCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix, bool forceStack) {
    AssemblerSimulatedOps::emitFillCode(this, binary, tokenIndex, scopePrefix, forceStack);
}

void AssemblerParser::emitMoveCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix, bool forceStack) {
    AssemblerSimulatedOps::emitMoveCode(this, binary, tokenIndex, scopePrefix, forceStack);
}

void AssemblerParser::emitFlatMemoryCode(std::vector<uint8_t>& binary, const std::string& mnemonic, int tokenIndex, const std::string& scopePrefix) {
    AssemblerSimulatedOps::emitFlatMemoryCode(this, binary, mnemonic, tokenIndex, scopePrefix);
}

void AssemblerParser::emitPHWStackCode(std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix) {
    AssemblerSimulatedOps::emitPHWStackCode(this, binary, tokenIndex, scopePrefix);
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
                     stmt->instr.mnemonic == "PHW" || stmt->instr.mnemonic == "ASR.16") {

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
                     else if (stmt->instr.mnemonic == "ASR.16") stmt->type = Statement::ASR16;

                     stmt->instr.operandTokenIndex = (int)pos;

                // Handle instructions with 1 or more operands
                if (peek().type != AssemblerTokenType::NEWLINE && peek().type != AssemblerTokenType::END_OF_FILE) {
                    if (peek().type == AssemblerTokenType::REGISTER) {
                        stmt->instr.operand = "." + advance().value;
                    } else if (peek().type == AssemblerTokenType::HASH) {
                        std::string immediateStr = advance().value; // This consumes '#'
                        stmt->instr.operandTokenIndex = (int)pos; // Start of expression after '#'

                        // Capture the tokens for the immediate value
                        std::string valuePart;
                        int tempPos = (int)pos;
                        while(tokens[tempPos].type != AssemblerTokenType::NEWLINE &&
                              tokens[tempPos].type != AssemblerTokenType::END_OF_FILE &&
                              tokens[tempPos].type != AssemblerTokenType::COMMA) {
                            valuePart += tokens[tempPos].value;
                            tempPos++;
                        }
                        
                        stmt->instr.operand = immediateStr + valuePart; // e.g., "#$001E"

                        int idx = (int)pos;
                        auto ast = parseExprAST(tokens, idx, symbolTable, stmt->scopePrefix);
                        pos = idx; // Update pos after expression parsing
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
                // else if (stmt->type == Statement::ROW) emitROWCode(d, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix);
                else if (stmt->type == Statement::LDW) emitLDWCode(d, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix, stmt->instr.mnemonic == "LDW.SP");
                else if (stmt->type == Statement::STW) emitSTWCode(d, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix, stmt->instr.mnemonic == "STW.SP");
                else if (stmt->type == Statement::FILL) emitFillCode(d, stmt->instr.operandTokenIndex, stmt->scopePrefix, stmt->instr.mnemonic == "FILL.SP");
                else if (stmt->type == Statement::COPY) emitMoveCode(d, stmt->instr.operandTokenIndex, stmt->scopePrefix, stmt->instr.mnemonic == "MOVE.SP");
                // else if (stmt->type == Statement::ROW) emitROWCode(d, stmt->instr.operand, stmt->exprTokenIndex, stmt->scopePrefix);
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
    bool overallChanged;
    do {
        overallChanged = false; // Reset for this iteration

        // 1. Run optimizer
        bool optimizerMadeChanges = optimize();
        if (optimizerMadeChanges) {
            overallChanged = true;
        }

        // 2. Remove deleted statements
        std::deque<std::unique_ptr<Statement>> newStatements;
        for (auto& s : statements) {
            if (!s->deleted) {
                newStatements.push_back(std::move(s));
            } else {
                // If a statement was deleted, it's a change that needs to be reflected in overallChanged
                overallChanged = true;
            }
        }
        statements = std::move(newStatements);


        // 3. Recalculate addresses and sizes, and check for changes
        bool addressRecalculationMadeChanges = false;
        uint32_t cP = 0;
        for (auto& s : statements) {
            s->address = cP;
            if (!s->label.empty()) {
                if (symbolTable[s->label].value != cP) {
                    symbolTable[s->label].value = cP;
                    addressRecalculationMadeChanges = true;
                }
            }
            if (s->type == Statement::DIRECTIVE && s->dir.name == "org") {
                if (!s->dir.arguments.empty()) cP = parseNumericLiteral(s->dir.arguments[0]);
            }
            if (s->type == Statement::INSTRUCTION) {
                int oS = s->size;
                s->size = calculateInstructionSize(s->instr, cP, s->scopePrefix);
                if (s->size != oS) addressRecalculationMadeChanges = true;
            }
            else if (s->type == Statement::EXPR) {
                int oS = s->size;
                std::vector<uint8_t> d;
                emitExpressionCode(d, s->exprTarget, s->exprTokenIndex, s->scopePrefix);
                s->size = d.size();
                if (s->size != oS) addressRecalculationMadeChanges = true;
            }
            cP += s->size;
        }
        if (addressRecalculationMadeChanges) {
            overallChanged = true;
        }

    } while (overallChanged); // Continue if any change was made in this iteration

    return AssemblerGenerator::generate(this);
}

