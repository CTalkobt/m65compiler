#pragma once
#include <vector>
#include <string>
#include <memory>
#include <cstdint>
#include <map>
#include "AssemblerToken.hpp"

class AssemblerParser;
struct Symbol;

struct ExprAST {
    virtual ~ExprAST() = default;
    virtual uint32_t getValue(AssemblerParser* parser) const = 0;
    virtual bool isConstant(AssemblerParser* parser) const = 0;
    virtual bool is16Bit(AssemblerParser* parser) const = 0;
    virtual void emit(std::vector<uint8_t>& binary, AssemblerParser* parser, int width, const std::string& target) = 0;
};

struct ConstantNode : public ExprAST {
    uint32_t value;
    ConstantNode(uint32_t v) : value(v) {}
    uint32_t getValue(AssemblerParser*) const override;
    bool isConstant(AssemblerParser*) const override;
    bool is16Bit(AssemblerParser*) const override;
    void emit(std::vector<uint8_t>& binary, AssemblerParser* parser, int width, const std::string& target) override;
};

struct RegisterNode : public ExprAST {
    std::string name;
    RegisterNode(const std::string& n) : name(n) {}
    uint32_t getValue(AssemblerParser*) const override;
    bool isConstant(AssemblerParser*) const override;
    bool is16Bit(AssemblerParser*) const override;
    void emit(std::vector<uint8_t>& binary, AssemblerParser* parser, int width, const std::string& target) override;
};

struct FlagNode : public ExprAST {
    char flag;
    FlagNode(char f) : flag(f) {}
    uint32_t getValue(AssemblerParser*) const override;
    bool isConstant(AssemblerParser*) const override;
    bool is16Bit(AssemblerParser*) const override;
    void emit(std::vector<uint8_t>& binary, AssemblerParser* parser, int width, const std::string& target) override;
};

struct VariableNode : public ExprAST {
    std::string name;
    std::string scopePrefix;
    VariableNode(const std::string& n, const std::string& scope = "") : name(n), scopePrefix(scope) {}
    uint32_t getValue(AssemblerParser* parser) const override;
    bool isConstant(AssemblerParser* parser) const override;
    bool is16Bit(AssemblerParser* parser) const override;
    void emit(std::vector<uint8_t>& binary, AssemblerParser* parser, int width, const std::string& target) override;
};

struct UnaryExpr : public ExprAST {
    std::string op;
    std::unique_ptr<ExprAST> operand;
    UnaryExpr(std::string o, std::unique_ptr<ExprAST> opnd) : op(o), operand(std::move(opnd)) {}
    uint32_t getValue(AssemblerParser* parser) const override;
    bool isConstant(AssemblerParser* parser) const override;
    bool is16Bit(AssemblerParser* parser) const override;
    void emit(std::vector<uint8_t>& binary, AssemblerParser* parser, int width, const std::string& target) override;
};

struct DereferenceNode : public ExprAST {
    std::unique_ptr<ExprAST> address;
    bool isFlat;
    DereferenceNode(std::unique_ptr<ExprAST> addr, bool flat = false) : address(std::move(addr)), isFlat(flat) {}
    uint32_t getValue(AssemblerParser*) const override;
    bool isConstant(AssemblerParser*) const override;
    bool is16Bit(AssemblerParser*) const override;
    void emit(std::vector<uint8_t>& binary, AssemblerParser* parser, int width, const std::string& target) override;
};

struct BinaryExpr : public ExprAST {
    std::string op;
    std::unique_ptr<ExprAST> left, right;
    BinaryExpr(std::string o, std::unique_ptr<ExprAST> l, std::unique_ptr<ExprAST> r) : op(o), left(std::move(l)), right(std::move(r)) {}
    uint32_t getValue(AssemblerParser* parser) const override;
    bool isConstant(AssemblerParser* parser) const override;
    bool is16Bit(AssemblerParser* parser) const override;
    void emit(std::vector<uint8_t>& binary, AssemblerParser* parser, int width, const std::string& target) override;
};

std::unique_ptr<ExprAST> parseExprAST(const std::vector<AssemblerToken>& tokens, int& idx, std::map<std::string, Symbol>& symbolTable, const std::string& scopePrefix = "");
