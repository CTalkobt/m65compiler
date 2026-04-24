#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include "AssemblerLexer.hpp"
#include "AssemblerParser.hpp"
#include "Preprocessor.hpp"
#include "AssemblerGenerator.hpp"
#include "M65Emitter.hpp"

int main(int argc, char** argv) {
    std::string input_file;
    std::string output_file = "out.bin";
    bool verbose = false;
    int listingLevel = 1;
    std::map<std::string, uint32_t> predefinedSymbols;
    std::map<std::string, std::string> initialSymbols;
    std::vector<std::string> includePaths;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-?" || arg == "--help") {
            std::cout << "Usage: ca45 [options] <input_file.s>" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  -o <filename>  Specify output filename (default: out.bin)" << std::endl;
            std::cout << "                 If filename ends in .prg, a 2-byte load address header is added." << std::endl;
            std::cout << "  -l <level>     Listing level: 1=Binary (default), 2=Expanded Assembly" << std::endl;
            std::cout << "  -v             Enable verbose output" << std::endl;
            std::cout << "  -Dname=val     Define a symbol (e.g., -Dcc45.zeroPageStart=$10)" << std::endl;
            std::cout << "  -I<path>       Add include search path" << std::endl;
            std::cout << "  -?             Display this help message" << std::endl;
            return 0;
        } else if (arg == "-o" && i + 1 < argc) {
            output_file = argv[++i];
        } else if (arg == "-l" && i + 1 < argc) {
            listingLevel = std::stoi(argv[++i]);
        } else if (arg == "-v") {
            verbose = true;
        } else if (arg.substr(0, 2) == "-I") {
            includePaths.push_back(arg.substr(2));
        } else if (arg.substr(0, 2) == "-D") {
            std::string define = arg.substr(2);
            size_t eq = define.find('=');
            if (eq != std::string::npos) {
                std::string name = define.substr(0, eq);
                std::string valStr = define.substr(eq + 1);
                initialSymbols[name] = valStr;
                uint32_t val = 0;
                if (valStr.substr(0, 1) == "$") val = std::stoul(valStr.substr(1), nullptr, 16);
                else if (valStr.substr(0, 1) == "%") val = std::stoul(valStr.substr(1), nullptr, 2);
                else val = std::stoul(valStr);
                predefinedSymbols[name] = val;
            } else {
                initialSymbols[define] = "1";
                predefinedSymbols[define] = 1;
            }
        } else {
            input_file = arg;
        }
    }

    if (predefinedSymbols.find("cc45.zeroPageStart") == predefinedSymbols.end()) {
        predefinedSymbols["cc45.zeroPageStart"] = 0x02;
    }

    if (input_file.empty()) {
        std::cerr << "Usage: ca45 [options] <input_file.s>" << std::endl;
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

    Preprocessor preprocessor(false);
    std::string source;
    try {
        source = preprocessor.process(sourceRaw, initialSymbols, includePaths, input_file);
    } catch (const std::exception& e) {
        std::cerr << "Preprocessor Error: " << e.what() << std::endl;
        return 1;
    }

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

    AssemblerParser parser(tokens, predefinedSymbols);
    try {
        parser.pass1();

        if (listingLevel == 2) {
            parser.pass2(false); // Run optimizer and resolve addresses
            std::ofstream out(output_file);
            M65Emitter e(out, predefinedSymbols["cc45.zeroPageStart"]);
            AssemblerGenerator::generate(&parser, e);
            std::cout << "Expanded listing generated to " << output_file << std::endl;
        } else {
            bool isPrg = false;
            if (output_file.length() >= 4 && output_file.substr(output_file.length() - 4) == ".prg") {
                isPrg = true;
            }
            auto binary = parser.pass2(isPrg);
            if (!binary.empty()) {
                std::ofstream out(output_file, std::ios::binary);
                out.write(reinterpret_cast<const char*>(binary.data()), binary.size());
                std::cout << "Assembled to " << output_file << " (" << binary.size() << " bytes)" << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Assembly Error: " << e.what() << std::endl;
        // Optionally print more context if we can find it in the exception message
        return 1;
    }

    return 0;
}
