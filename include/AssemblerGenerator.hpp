#pragma once
#include <vector>
#include <cstdint>
#include "AssemblerTypes.hpp"

class AssemblerParser;

class AssemblerGenerator {
public:
    static std::vector<uint8_t> generate(AssemblerParser* parser);
};
