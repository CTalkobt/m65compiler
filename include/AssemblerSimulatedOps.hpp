#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include "AssemblerTypes.hpp"

class AssemblerParser;

class AssemblerSimulatedOps {
public:
    static void emitExpressionCode(AssemblerParser* parser, std::vector<uint8_t>& binary, const std::string& target, int tokenIndex, const std::string& scopePrefix);
    static void emitMulCode(AssemblerParser* parser, std::vector<uint8_t>& binary, int width, const std::string& dest, int tokenIndex, const std::string& scopePrefix);
    static void emitDivCode(AssemblerParser* parser, std::vector<uint8_t>& binary, int width, const std::string& dest, int tokenIndex, const std::string& scopePrefix);
    static void emitStackIncDecCode(AssemblerParser* parser, std::vector<uint8_t>& binary, bool isInc, int tokenIndex, const std::string& scopePrefix);
    static void emitStackIncDec8Code(AssemblerParser* parser, std::vector<uint8_t>& binary, bool isInc, int tokenIndex, const std::string& scopePrefix);
    static void emitAddSub16Code(AssemblerParser* parser, std::vector<uint8_t>& binary, bool isAdd, const std::string& dest, int tokenIndex, const std::string& scopePrefix);
    static void emitBitwise16Code(AssemblerParser* parser, std::vector<uint8_t>& binary, const std::string& mnemonic, const std::string& dest, int tokenIndex, const std::string& scopePrefix);
    static void emitCPWCode(AssemblerParser* parser, std::vector<uint8_t>& binary, const std::string& src1, int tokenIndex, const std::string& scopePrefix);
    static void emitLDWCode(AssemblerParser* parser, std::vector<uint8_t>& binary, const std::string& dest, int tokenIndex, const std::string& scopePrefix, bool forceStack = false);
    static void emitSTWCode(AssemblerParser* parser, std::vector<uint8_t>& binary, const std::string& src, int tokenIndex, const std::string& scopePrefix, bool forceStack = false);
    static void emitSwapCode(AssemblerParser* parser, std::vector<uint8_t>& binary, const std::string& r1, int tokenIndex, const std::string& scopePrefix);
    static void emitZeroCode(AssemblerParser* parser, std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix);
    static void emitNegNot16Code(AssemblerParser* parser, std::vector<uint8_t>& binary, bool isNeg, const std::string& operand, int tokenIndex, const std::string& scopePrefix);
    static void emitChkZeroCode(AssemblerParser* parser, std::vector<uint8_t>& binary, bool is16, bool isInverse, int tokenIndex, const std::string& scopePrefix);
    static void emitBranch16Code(AssemblerParser* parser, std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix);
    static void emitSelectCode(AssemblerParser* parser, std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix);
    static void emitPtrStackCode(AssemblerParser* parser, std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix);
    static void emitPtrDerefCode(AssemblerParser* parser, std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix);
    static void emitFillCode(AssemblerParser* parser, std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix, bool forceStack = false);
    static void emitMoveCode(AssemblerParser* parser, std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix, bool forceStack = false);
    static void emitFlatMemoryCode(AssemblerParser* parser, std::vector<uint8_t>& binary, const std::string& mnemonic, int tokenIndex, const std::string& scopePrefix);
    static void emitPHWStackCode(AssemblerParser* parser, std::vector<uint8_t>& binary, int tokenIndex, const std::string& scopePrefix);
};
