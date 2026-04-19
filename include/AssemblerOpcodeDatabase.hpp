#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include "AssemblerTypes.hpp"

class AssemblerOpcodeDatabase {
public:
    static uint8_t getOpcode(const std::string& mnemonic, AddressingMode mode);
    static std::vector<AddressingMode> getValidAddressingModes(const std::string& mnemonic);
    static std::string AddressingModeToString(AddressingMode mode);

private:
    static const std::map<std::pair<std::string, AddressingMode>, uint8_t>& getOpcodeMap();
};
