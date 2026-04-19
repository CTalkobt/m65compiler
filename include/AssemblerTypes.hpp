#pragma once
#include <string>
#include <vector>
#include <cstdint>

enum class AddressingMode {
    IMPLIED,
    ACCUMULATOR,
    IMMEDIATE,
    IMMEDIATE16,
    BASE_PAGE,
    BASE_PAGE_X,
    BASE_PAGE_Y,
    ABSOLUTE,
    ABSOLUTE_X,
    ABSOLUTE_Y,
    INDIRECT,
    BASE_PAGE_X_INDIRECT,
    BASE_PAGE_INDIRECT_Y,
    BASE_PAGE_INDIRECT_Z,
    BASE_PAGE_INDIRECT_SP_Y,
    ABSOLUTE_INDIRECT,
    ABSOLUTE_X_INDIRECT,
    RELATIVE,
    RELATIVE16,
    BASE_PAGE_RELATIVE,
    FLAT_INDIRECT_Z,
    QUAD_Q,
    STACK_RELATIVE,
    STACK_RELATIVE_INDIRECT_Y,
    LINEAR_ABSOLUTE,
    LINEAR_ABSOLUTE_X,
    LINEAR_ABSOLUTE_Y,
    NONE
};

struct Symbol {
    uint32_t value;
    bool isAddress;
    int size;
    bool isVariable = false;
    uint32_t initialValue = 0;
    bool isStackRelative = false;
    int stackOffset = 0;
};
