#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include "Lexer.hpp"
#include "Parser.hpp"
#include "AST.hpp"
#include "CodeGenerator.hpp"

class ASTPrinter : public ASTVisitor {
public:
    int indent = 0;
    void printIndent() { for (int i = 0; i < indent; i++) std::cout << "  "; }
    void visit(IntegerLiteral& node) override { printIndent(); std::cout << "IntegerLiteral: " << node.value << std::endl; }
    void visit(StringLiteral& node) override { printIndent(); std::cout << "StringLiteral: \"" << node.value << "\"" << std::endl; }
    void visit(VariableReference& node) override { printIndent(); std::cout << "VariableReference: " << node.name << std::endl; }
    void visit(Assignment& node) override {
        printIndent(); std::cout << "Assignment: " << node.name << std::endl;
        indent++;
        node.expression->accept(*this);
        indent--;
    }
    void visit(BinaryOperation& node) override {
        printIndent(); std::cout << "BinaryOperation: " << node.op << std::endl;
        indent++;
        node.left->accept(*this);
        node.right->accept(*this);
        indent--;
    }
    void visit(FunctionCall& node) override {
        printIndent(); std::cout << "FunctionCall: " << node.name << std::endl;
        indent++;
        for (auto& arg : node.arguments) arg->accept(*this);
        indent--;
    }
    void visit(VariableDeclaration& node) override {
        printIndent(); std::cout << "VariableDeclaration: " << node.name << " (" << node.type << ")" << std::endl;
        if (node.initializer) {
            indent++;
            node.initializer->accept(*this);
            indent--;
        }
    }
    void visit(ReturnStatement& node) override {
        printIndent(); std::cout << "ReturnStatement" << std::endl;
        if (node.expression) {
            indent++;
            node.expression->accept(*this);
            indent--;
        }
    }
    void visit(ExpressionStatement& node) override {
        printIndent(); std::cout << "ExpressionStatement" << std::endl;
        indent++;
        node.expression->accept(*this);
        indent--;
    }
    void visit(IfStatement& node) override {
        printIndent(); std::cout << "IfStatement" << std::endl;
        indent++;
        printIndent(); std::cout << "Condition:" << std::endl;
        indent++;
        node.condition->accept(*this);
        indent--;
        printIndent(); std::cout << "Then:" << std::endl;
        indent++;
        node.thenBranch->accept(*this);
        indent--;
        if (node.elseBranch) {
            printIndent(); std::cout << "Else:" << std::endl;
            indent++;
            node.elseBranch->accept(*this);
            indent--;
        }
        indent--;
    }
    void visit(WhileStatement& node) override {
        printIndent(); std::cout << "WhileStatement" << std::endl;
        indent++;
        printIndent(); std::cout << "Condition:" << std::endl;
        indent++;
        node.condition->accept(*this);
        indent--;
        printIndent(); std::cout << "Body:" << std::endl;
        indent++;
        node.body->accept(*this);
        indent--;
        indent--;
    }
    void visit(ForStatement& node) override {
        printIndent(); std::cout << "ForStatement" << std::endl;
        indent++;
        if (node.initializer) {
            printIndent(); std::cout << "Initializer:" << std::endl;
            indent++;
            node.initializer->accept(*this);
            indent--;
        }
        if (node.condition) {
            printIndent(); std::cout << "Condition:" << std::endl;
            indent++;
            node.condition->accept(*this);
            indent--;
        }
        if (node.increment) {
            printIndent(); std::cout << "Increment:" << std::endl;
            indent++;
            node.increment->accept(*this);
            indent--;
        }
        printIndent(); std::cout << "Body:" << std::endl;
        indent++;
        node.body->accept(*this);
        indent--;
        indent--;
    }
    void visit(AsmStatement& node) override {
        printIndent(); std::cout << "AsmStatement: " << node.code << std::endl;
    }
    void visit(CompoundStatement& node) override {
        printIndent(); std::cout << "CompoundStatement" << std::endl;
        indent++;
        for (auto& stmt : node.statements) stmt->accept(*this);
        indent--;
    }
    void visit(FunctionDeclaration& node) override {
        printIndent(); std::cout << "FunctionDeclaration: " << node.name << " (" << node.returnType << ")" << std::endl;
        indent++;
        for (const auto& param : node.parameters) {
            printIndent(); std::cout << "Parameter: " << param.name << " (" << param.type << (param.isPointer ? "*" : "") << ")" << std::endl;
        }
        node.body->accept(*this);
        indent--;
    }
    void visit(TranslationUnit& node) override {
        printIndent(); std::cout << "TranslationUnit" << std::endl;
        indent++;
        for (auto& func : node.functions) func->accept(*this);
        indent--;
    }
};

int main(int argc, char** argv) {
    std::string input_file;
    std::string output_file = "out.s";
    bool assemble = false;
    bool verbose = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-c") {
            assemble = true;
        } else if (arg == "-o" && i + 1 < argc) {
            output_file = argv[++i];
        } else if (arg == "-v") {
            verbose = true;
        } else {
            input_file = arg;
        }
    }

    if (input_file.empty()) {
        std::cerr << "Usage: cc45 [options] <input_file.c>" << std::endl;
        return 1;
    }

    std::ifstream file(input_file);
    if (!file.is_open()) {
        std::cerr << "Failed to open input file: " << input_file << std::endl;
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    if (verbose) {
        std::cout << "Lexing " << input_file << "..." << std::endl;
    }

    Lexer lexer(source);
    std::vector<Token> tokens = lexer.tokenize();

    if (verbose) {
        for (const auto& token : tokens) {
            std::cout << "Token: " << token.typeToString() << " (" << token.value << ") at " << token.line << ":" << token.column << std::endl;
        }
    }

    if (verbose) {
        std::cout << "Parsing " << input_file << "..." << std::endl;
    }

    Parser parser(tokens);
    try {
        auto ast = parser.parse();
        if (verbose) {
            ASTPrinter printer;
            ast->accept(printer);
        }

        std::ofstream asmOut(output_file);
        if (!asmOut.is_open()) {
            std::cerr << "Failed to open output file for assembly: " << output_file << std::endl;
            return 1;
        }

        CodeGenerator codegen(asmOut);
        codegen.generate(*ast);
        if (verbose) {
            std::cout << "Generated assembly in " << output_file << std::endl;
        }
        asmOut.close();

    } catch (const std::exception& e) {
        std::cerr << "Compiler Error: " << e.what() << std::endl;
        return 1;
    }
    
    if (assemble) {
        std::cout << "Calling assembler (ca45)..." << std::endl;
        std::string bin_out = output_file + ".bin";
        std::string command = "./bin/ca45 -o " + bin_out + " " + output_file;
        int ret = std::system(command.c_str());
        if (ret != 0) {
            std::cerr << "Assembler failed with return code " << ret << std::endl;
            return 1;
        }
    }

    return 0;
}
