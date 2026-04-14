#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include "AssemblerLexer.hpp"
#include "AssemblerParser.hpp"

int main(int argc, char** argv) {
    std::string input_file;
    std::string output_file = "out.bin";
    bool verbose = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-o" && i + 1 < argc) {
            output_file = argv[++i];
        } else if (arg == "-v") {
            verbose = true;
        } else {
            input_file = arg;
        }
    }

    if (input_file.empty()) {
        std::cerr << "Usage: ca45 [options] <input_file.s>" << std::endl;
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

    AssemblerLexer lexer(source);
    std::vector<AssemblerToken> tokens = lexer.tokenize();

    if (verbose) {
        for (const auto& token : tokens) {
            std::cout << "Token: " << token.typeToString() << " (" << token.value << ") at " << token.line << ":" << token.column << std::endl;
        }
    }

    AssemblerParser parser(tokens);
    parser.pass1();
    auto binary = parser.pass2();

    if (!binary.empty()) {
        std::ofstream out(output_file, std::ios::binary);
        out.write(reinterpret_cast<const char*>(binary.data()), binary.size());
        std::cout << "Assembled to " << output_file << " (" << binary.size() << " bytes)" << std::endl;
    }

    return 0;
}
