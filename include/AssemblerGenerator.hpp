#pragma once
#include <vector>
#include <cstdint>

class AssemblerParser;
class M65Emitter;

class AssemblerGenerator {
public:
    static std::vector<uint8_t> generate(AssemblerParser* parser);
    static void generate(AssemblerParser* parser, M65Emitter& e);
};
