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
#include "ConstantFolder.hpp"
#include "Preprocessor.hpp"
#include "AssemblerLexer.hpp"
#include "AssemblerParser.hpp"
#include "AssemblerGenerator.hpp"
#include "M65Emitter.hpp"

class ASTPrinter : public ASTVisitor {
public:
    int indent = 0;
    void printIndent() { for (int i = 0; i < indent; i++) std::cout << "  "; }
    void visit(IntegerLiteral& node) override { printIndent(); std::cout << "IntegerLiteral: " << node.value << std::endl; }
    void visit(StringLiteral& node) override { printIndent(); std::cout << "StringLiteral: \"" << node.value << "\"" << std::endl; }
    void visit(VariableReference& node) override { printIndent(); std::cout << "VariableReference: " << node.name << std::endl; }
    void visit(Assignment& node) override {
        printIndent(); std::cout << "Assignment: " << std::endl;
        indent++;
        node.target->accept(*this);
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
    void visit(UnaryOperation& node) override {
        printIndent(); std::cout << "UnaryOperation: " << node.op << std::endl;
        indent++;
        node.operand->accept(*this);
        indent--;
    }
    void visit(FunctionCall& node) override {
        printIndent(); std::cout << "FunctionCall: " << node.name << std::endl;
        indent++;
        for (auto& arg : node.arguments) arg->accept(*this);
        indent--;
    }
    void visit(MemberAccess& node) override {
        printIndent(); std::cout << "MemberAccess: " << (node.isArrow ? "->" : ".") << node.memberName << std::endl;
        indent++;
        node.structExpr->accept(*this);
        indent--;
    }
    void visit(ConditionalExpression& node) override {
        printIndent(); std::cout << "ConditionalExpression:" << std::endl;
        indent++;
        printIndent(); std::cout << "Condition:" << std::endl;
        indent++;
        node.condition->accept(*this);
        indent--;
        printIndent(); std::cout << "Then:" << std::endl;
        indent++;
        node.thenExpr->accept(*this);
        indent--;
        printIndent(); std::cout << "Else:" << std::endl;
        indent++;
        node.elseExpr->accept(*this);
        indent--;
        indent--;
    }
    void visit(GenericSelection& node) override {
        printIndent(); std::cout << "GenericSelection:" << std::endl;
        indent++;
        printIndent(); std::cout << "Control:" << std::endl;
        indent++;
        node.control->accept(*this);
        indent--;
        printIndent(); std::cout << "Associations:" << std::endl;
        indent++;
        for (auto& assoc : node.associations) {
            printIndent();
            if (assoc.isDefault) std::cout << "default:";
            else {
                std::cout << assoc.typeName;
                for (int i = 0; i < assoc.pointerLevel; i++) std::cout << "*";
                std::cout << ":";
            }
            std::cout << std::endl;
            indent++;
            assoc.result->accept(*this);
            indent--;
        }
        indent--;
        indent--;
    }
    void visit(ArrayAccess& node) override {
        printIndent(); std::cout << "ArrayAccess:" << std::endl;
        indent++;
        printIndent(); std::cout << "Array:" << std::endl;
        indent++;
        node.arrayExpr->accept(*this);
        indent--;
        printIndent(); std::cout << "Index:" << std::endl;
        indent++;
        node.indexExpr->accept(*this);
        indent--;
        indent--;
    }
    void visit(AlignofExpression& node) override {
        printIndent(); std::cout << "Alignof: " << node.typeName;
        for (int i = 0; i < node.pointerLevel; i++) std::cout << "*";
        std::cout << std::endl;
    }
    void visit(VariableDeclaration& node) override {
        printIndent(); std::cout << "VariableDeclaration: " << node.name << " (" << (node.isSigned ? "signed " : "") << node.type << ")" << std::endl;
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
    void visit(BreakStatement&) override {
        printIndent(); std::cout << "BreakStatement" << std::endl;
    }
    void visit(ContinueStatement&) override {
        printIndent(); std::cout << "ContinueStatement" << std::endl;
    }
    void visit(SwitchContinueStatement& node) override {
        printIndent(); std::cout << "SwitchContinueStatement";
        if (node.target) {
            std::cout << ":" << std::endl;
            indent++;
            node.target->accept(*this);
            indent--;
        } else {
            std::cout << " (default)" << std::endl;
        }
    }
    void visit(GotoStatement& node) override {
        printIndent(); std::cout << "GotoStatement: " << node.label << std::endl;
    }
    void visit(LabelledStatement& node) override {
        printIndent(); std::cout << "Label: " << node.label << ":" << std::endl;
        indent++;
        node.statement->accept(*this);
        indent--;
    }
    void visit(SizeofExpression& node) override {
        printIndent(); std::cout << "Sizeof: ";
        if (node.isType) {
            std::cout << node.typeName;
            for (int i = 0; i < node.pointerLevel; i++) std::cout << "*";
        } else {
            std::cout << "expr" << std::endl;
            indent++;
            node.expression->accept(*this);
            indent--;
        }
        std::cout << std::endl;
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
    void visit(DoWhileStatement& node) override {
        printIndent(); std::cout << "DoWhileStatement" << std::endl;
        indent++;
        printIndent(); std::cout << "Body:" << std::endl;
        indent++;
        node.body->accept(*this);
        indent--;
        printIndent(); std::cout << "Condition:" << std::endl;
        indent++;
        node.condition->accept(*this);
        indent--;
        indent--;
    }
    void visit(ForStatement& node) override {
        printIndent(); std::cout << "ForStatement" << std::endl;
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
    }
    void visit(SwitchStatement& node) override {
        printIndent(); std::cout << "SwitchStatement" << std::endl;
        indent++;
        printIndent(); std::cout << "Expression:" << std::endl;
        indent++;
        node.expression->accept(*this);
        indent--;
        printIndent(); std::cout << "Body:" << std::endl;
        indent++;
        node.body->accept(*this);
        indent--;
        indent--;
    }
    void visit(CaseStatement& node) override {
        printIndent(); std::cout << "CaseStatement: " << std::endl;
        indent++;
        node.value->accept(*this);
        indent--;
    }
    void visit(DefaultStatement&) override {
        printIndent(); std::cout << "DefaultStatement" << std::endl;
    }
    void visit(AsmStatement& node) override {
        printIndent(); std::cout << "AsmStatement: " << node.code << std::endl;
    }
    void visit(StaticAssert& node) override {
        printIndent(); std::cout << "StaticAssert: " << node.message << std::endl;
    }
    void visit(StructDefinition& node) override {
        printIndent(); std::cout << (node.isUnion ? "UnionDefinition: " : "StructDefinition: ") << node.name << std::endl;
        indent++;
        for (const auto& member : node.members) {
            std::string ptrs = "";
            for (int i = 0; i < member.pointerLevel; i++) ptrs += "*";
            printIndent(); 
            if (member.isAnonymous) std::cout << "(Anonymous) ";
            std::cout << "Member: " << member.name << " (" << member.type << ptrs << ")" << std::endl;
        }
        indent--;
    }
    void visit(CompoundStatement& node) override {
        printIndent(); std::cout << "CompoundStatement" << std::endl;
        indent++;
        for (auto& stmt : node.statements) stmt->accept(*this);
        indent--;
    }
    void visit(FunctionDeclaration& node) override {
        printIndent(); std::cout << "FunctionDeclaration: " << node.name << " (" << (node.isSigned ? "signed " : "") << node.returnType << ")";
        if (node.isNoreturn) std::cout << " [noreturn]";
        std::cout << std::endl;
        indent++;
        for (const auto& param : node.parameters) {
            std::string ptrs = "";
            for (int i = 0; i < param.pointerLevel; i++) ptrs += "*";
            printIndent(); std::cout << "Parameter: " << param.name << " (" << (param.isSigned ? "signed " : "") << param.type << ptrs << ")" << std::endl;
        }
        node.body->accept(*this);
        indent--;
    }
    void visit(TranslationUnit& node) override {
        printIndent(); std::cout << "TranslationUnit" << std::endl;
        indent++;
        for (auto& decl : node.topLevelDecls) decl->accept(*this);
        indent--;
    }
};

int main(int argc, char** argv) {
    std::string programName = argv[0];
    size_t lastSlash = programName.find_last_of("/\\");
    if (lastSlash != std::string::npos) programName = programName.substr(lastSlash + 1);

    std::string input_file;
    bool preprocessOnly = (programName == "cp45");
    std::string output_file = "";
    bool outputFileSet = false;
    bool assemble = false;
    bool verbose = false;
    int listingLevel = 1;
    uint32_t zeroPageStart = 0x02;
    uint32_t zeroPageAvail = 9;
    std::string defineFlag = "";
    std::map<std::string, std::string> initialSymbols;
    std::vector<std::string> includePaths;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-?" || arg == "--help") {
            std::cout << "Usage: cc45 [options] <input_file.c>" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  -E             Run only the preprocessor (output to stdout or -o file)" << std::endl;
            std::cout << "  -c             Assemble the output with ca45" << std::endl;
            std::cout << "  -o <filename>  Specify output assembly filename (default: out.s)" << std::endl;
            std::cout << "  -l <level>     Listing level: 1=Standard (default), 2=Expanded" << std::endl;
            std::cout << "  -v             Enable verbose output" << std::endl;
            std::cout << "  -Dname=val     Define a symbol (e.g., -Dcc45.zeroPageStart=$10)" << std::endl;
            std::cout << "  -I<path>       Add include search path" << std::endl;
            std::cout << "  -?             Display this help message" << std::endl;
            return 0;
        } else if (arg == "-c") {
            assemble = true;
        } else if (arg == "-E") {
            preprocessOnly = true;
        } else if (arg == "-o" && i + 1 < argc) {
            output_file = argv[++i];
            outputFileSet = true;
        } else if (arg == "-l" && i + 1 < argc) {
            listingLevel = std::stoi(argv[++i]);
        } else if (arg == "-v") {
            verbose = true;
        } else if (arg.substr(0, 2) == "-I") {
            includePaths.push_back(arg.substr(2));
        } else if (arg.substr(0, 2) == "-D") {
            defineFlag = arg;
            size_t eq = arg.find('=');
            if (eq != std::string::npos) {
                std::string name = arg.substr(2, eq - 2);
                std::string valStr = arg.substr(eq + 1);
                initialSymbols[name] = valStr;
                uint32_t val = 0;
                if (valStr.empty()) {}
                else if (valStr.substr(0, 1) == "$") val = std::stoul(valStr.substr(1), nullptr, 16);
                else if (valStr.substr(0, 1) == "%") val = std::stoul(valStr.substr(1), nullptr, 2);
                else val = std::stoul(valStr);

                if (name == "cc45.zeroPageStart") {
                    zeroPageStart = val;
                } else if (name == "cc45.zeroPageAvail") {
                    zeroPageAvail = val;
                }
            } else {
                initialSymbols[arg.substr(2)] = "1";
            }
        } else {
            input_file = arg;
        }
    }

    if (input_file.empty()) {
        std::cerr << "Usage: cc45 [options] <input_file.c>" << std::endl;
        std::cerr << "Use -? for a list of options." << std::endl;
        return 1;
    }

    std::ifstream file(input_file);
    if (!file.is_open()) {
        std::cerr << "Failed to open input file: " << input_file << std::endl;
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string sourceRaw = buffer.str();

    if (verbose) {
        std::cout << "Preprocessing " << input_file << "..." << std::endl;
    }

    Preprocessor preprocessor(true);
    std::string source;
    try {
        source = preprocessor.process(sourceRaw, initialSymbols, includePaths, input_file);
    } catch (const std::exception& e) {
        std::cerr << "Preprocessor Error: " << e.what() << std::endl;
        return 1;
    }

    if (preprocessOnly) {
        if (outputFileSet) {
            std::ofstream out(output_file);
            if (!out.is_open()) {
                std::cerr << "Failed to open output file: " << output_file << std::endl;
                return 1;
            }
            out << source;
            out.close();
        } else {
            std::cout << source;
        }
        return 0;
    }

    if (!outputFileSet) output_file = "out.s";

    std::vector<std::string> sourceLines;
    {
        std::stringstream ss(source);
        std::string line;
        while (std::getline(ss, line)) {
            sourceLines.push_back(line);
        }
    }

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
        if (verbose) std::cout << "Parsing " << input_file << "..." << std::endl;
        auto ast = parser.parse();
        if (verbose) std::cout << "Parsing complete." << std::endl;

        if (verbose) std::cout << "Constant folding..." << std::endl;
        ConstantFolder folder;
        ast = folder.foldTranslationUnit(std::move(ast));
        if (verbose) std::cout << "Constant folding complete." << std::endl;

        if (verbose && listingLevel >= 1) {
            ASTPrinter printer;
            ast->accept(printer);
        }

        std::ofstream asmOut(output_file);
        if (!asmOut.is_open()) {
            std::cerr << "Failed to open output file for assembly: " << output_file << std::endl;
            return 1;
        }

        if (verbose) std::cout << "Code generation..." << std::endl;
        CodeGenerator codegen(asmOut);
        codegen.zeroPageStart = zeroPageStart;
        codegen.zeroPageAvail = zeroPageAvail;
        codegen.setSourceInfo(input_file, sourceLines);
        codegen.generate(*ast);
        if (verbose) {
            std::cout << "Generated assembly in " << output_file << std::endl;
        }
        asmOut.close();
        if (verbose) std::cout << "Code generation complete." << std::endl;

        if (listingLevel == 2) {
            if (verbose) std::cout << "Generating expanded listing..." << std::endl;
            
            std::ifstream asmIn(output_file);
            std::stringstream asmBuf;
            asmBuf << asmIn.rdbuf();
            asmIn.close();

            std::map<std::string, uint32_t> predefinedSymbols;
            predefinedSymbols["cc45.zeroPageStart"] = zeroPageStart;

            AssemblerLexer lex(asmBuf.str());
            auto tokens = lex.tokenize();
            AssemblerParser parser(tokens, predefinedSymbols);
            parser.pass1();
            parser.pass2(); // Run optimizer and resolve addresses

            std::ofstream expandedOut(output_file);
            M65Emitter e(expandedOut, zeroPageStart);
            AssemblerGenerator::generate(&parser, e);
            expandedOut.close();
        }

    } catch (const std::exception& e) {
        std::cerr << "Compiler Error: " << e.what() << std::endl;
        return 1;
    }
    
    if (assemble) {
        std::cout << "Calling assembler (ca45)..." << std::endl;
        std::string bin_out = output_file + ".bin";
        std::string command = "./bin/ca45 " + defineFlag + " -o " + bin_out + " " + output_file;
        int ret = std::system(command.c_str());
        if (ret != 0) {
            std::cerr << "Assembler failed with return code " << ret << std::endl;
            return 1;
        }
    }

    return 0;
}
