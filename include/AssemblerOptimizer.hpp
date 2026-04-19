#pragma once
#include <vector>
#include <memory>
#include <string>
#include "AssemblerTypes.hpp"

class AssemblerParser;

class AssemblerOptimizer {
public:
    static bool optimize(AssemblerParser* parser);
};
