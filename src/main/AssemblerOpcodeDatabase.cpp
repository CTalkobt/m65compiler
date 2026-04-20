#include "AssemblerOpcodeDatabase.hpp"
#include <map>
#include <algorithm>
#include <cctype>

const std::map<std::pair<std::string, AddressingMode>, uint8_t>& AssemblerOpcodeDatabase::getOpcodeMap() {
    static const std::map<std::pair<std::string, AddressingMode>, uint8_t> opcodes = {
        {{"adc", AddressingMode::BASE_PAGE_INDIRECT_Y}, 0x71},
        {{"adc", AddressingMode::BASE_PAGE_INDIRECT_Z}, 0x72},
        {{"adc", AddressingMode::FLAT_INDIRECT_Z}, 0x72},
        {{"adc", AddressingMode::STACK_RELATIVE}, 0x72},
        {{"adc", AddressingMode::BASE_PAGE_X_INDIRECT}, 0x61},
        {{"adc", AddressingMode::ABSOLUTE}, 0x6D},
        {{"adc", AddressingMode::ABSOLUTE_X}, 0x7D},
        {{"adc", AddressingMode::ABSOLUTE_Y}, 0x79},
        {{"adc", AddressingMode::BASE_PAGE}, 0x65},
        {{"adc", AddressingMode::BASE_PAGE_X}, 0x75},
        {{"adc", AddressingMode::IMMEDIATE}, 0x69},
        {{"and", AddressingMode::BASE_PAGE_INDIRECT_Y}, 0x31},
        {{"and", AddressingMode::BASE_PAGE_INDIRECT_Z}, 0x32},
        {{"and", AddressingMode::FLAT_INDIRECT_Z}, 0x32},
        {{"and", AddressingMode::STACK_RELATIVE}, 0x32},
        {{"and", AddressingMode::BASE_PAGE_X_INDIRECT}, 0x21},
        {{"and", AddressingMode::ABSOLUTE}, 0x2D},
        {{"and", AddressingMode::ABSOLUTE_X}, 0x3D},
        {{"and", AddressingMode::ABSOLUTE_Y}, 0x39},
        {{"and", AddressingMode::BASE_PAGE}, 0x25},
        {{"and", AddressingMode::BASE_PAGE_X}, 0x35},
        {{"and", AddressingMode::IMMEDIATE}, 0x29},
        {{"asl", AddressingMode::ABSOLUTE}, 0x0E},
        {{"asl", AddressingMode::ABSOLUTE_X}, 0x1E},
        {{"asl", AddressingMode::ACCUMULATOR}, 0x0A},
        {{"asl", AddressingMode::BASE_PAGE}, 0x06},
        {{"asl", AddressingMode::BASE_PAGE_X}, 0x16},
        {{"asr", AddressingMode::ACCUMULATOR}, 0x43},
        {{"asr", AddressingMode::BASE_PAGE}, 0x44},
        {{"asr", AddressingMode::BASE_PAGE_X}, 0x54},
        {{"asw", AddressingMode::ABSOLUTE}, 0xCB},
        {{"bbr0", AddressingMode::BASE_PAGE_RELATIVE}, 0x0F},
        {{"bbr1", AddressingMode::BASE_PAGE_RELATIVE}, 0x1F},
        {{"bbr2", AddressingMode::BASE_PAGE_RELATIVE}, 0x2F},
        {{"bbr3", AddressingMode::BASE_PAGE_RELATIVE}, 0x3F},
        {{"bbr4", AddressingMode::BASE_PAGE_RELATIVE}, 0x4F},
        {{"bbr5", AddressingMode::BASE_PAGE_RELATIVE}, 0x5F},
        {{"bbr6", AddressingMode::BASE_PAGE_RELATIVE}, 0x6F},
        {{"bbr7", AddressingMode::BASE_PAGE_RELATIVE}, 0x7F},
        {{"bbs0", AddressingMode::BASE_PAGE_RELATIVE}, 0x8F},
        {{"bbs1", AddressingMode::BASE_PAGE_RELATIVE}, 0x9F},
        {{"bbs2", AddressingMode::BASE_PAGE_RELATIVE}, 0xAF},
        {{"bbs3", AddressingMode::BASE_PAGE_RELATIVE}, 0xBF},
        {{"bbs4", AddressingMode::BASE_PAGE_RELATIVE}, 0xCF},
        {{"bbs5", AddressingMode::BASE_PAGE_RELATIVE}, 0xDF},
        {{"bbs6", AddressingMode::BASE_PAGE_RELATIVE}, 0xEF},
        {{"bbs7", AddressingMode::BASE_PAGE_RELATIVE}, 0xFF},
        {{"bcc", AddressingMode::RELATIVE}, 0x90},
        {{"bcc", AddressingMode::RELATIVE16}, 0x93},
        {{"bcs", AddressingMode::RELATIVE}, 0xB0},
        {{"bcs", AddressingMode::RELATIVE16}, 0xB3},
        {{"beq", AddressingMode::RELATIVE}, 0xF0},
        {{"beq", AddressingMode::RELATIVE16}, 0xF3},
        {{"bit", AddressingMode::ABSOLUTE}, 0x2C},
        {{"bit", AddressingMode::ABSOLUTE_X}, 0x3C},
        {{"bit", AddressingMode::BASE_PAGE}, 0x24},
        {{"bit", AddressingMode::BASE_PAGE_X}, 0x34},
        {{"bit", AddressingMode::IMMEDIATE}, 0x89},
        {{"bmi", AddressingMode::RELATIVE}, 0x30},
        {{"bmi", AddressingMode::RELATIVE16}, 0x33},
        {{"bne", AddressingMode::RELATIVE}, 0xD0},
        {{"bne", AddressingMode::RELATIVE16}, 0xD3},
        {{"bpl", AddressingMode::RELATIVE}, 0x10},
        {{"bpl", AddressingMode::RELATIVE16}, 0x13},
        {{"bra", AddressingMode::RELATIVE}, 0x80},
        {{"bra", AddressingMode::RELATIVE16}, 0x83},
        {{"brk", AddressingMode::IMPLIED}, 0x00},
        {{"bsr", AddressingMode::RELATIVE16}, 0x63},
        {{"bvc", AddressingMode::RELATIVE}, 0x50},
        {{"bvc", AddressingMode::RELATIVE16}, 0x53},
        {{"bvs", AddressingMode::RELATIVE}, 0x70},
        {{"bvs", AddressingMode::RELATIVE16}, 0x73},
        {{"clc", AddressingMode::IMPLIED}, 0x18},
        {{"cld", AddressingMode::IMPLIED}, 0xD8},
        {{"cle", AddressingMode::IMPLIED}, 0x02},
        {{"cli", AddressingMode::IMPLIED}, 0x58},
        {{"clv", AddressingMode::IMPLIED}, 0xB8},
        {{"cmp", AddressingMode::BASE_PAGE_INDIRECT_Y}, 0xD1},
        {{"cmp", AddressingMode::BASE_PAGE_INDIRECT_Z}, 0xD2},
        {{"cmp", AddressingMode::FLAT_INDIRECT_Z}, 0xD2},
        {{"cmp", AddressingMode::STACK_RELATIVE}, 0xD2},
        {{"cmp", AddressingMode::BASE_PAGE_X_INDIRECT}, 0xC1},
        {{"cmp", AddressingMode::ABSOLUTE}, 0xCD},
        {{"cmp", AddressingMode::ABSOLUTE_X}, 0xDD},
        {{"cmp", AddressingMode::ABSOLUTE_Y}, 0xD9},
        {{"cmp", AddressingMode::BASE_PAGE}, 0xC5},
        {{"cmp", AddressingMode::BASE_PAGE_X}, 0xD5},
        {{"cmp", AddressingMode::IMMEDIATE}, 0xC9},
        {{"cpx", AddressingMode::ABSOLUTE}, 0xEC},
        {{"cpx", AddressingMode::BASE_PAGE}, 0xE4},
        {{"cpx", AddressingMode::IMMEDIATE}, 0xE0},
        {{"cpy", AddressingMode::ABSOLUTE}, 0xCC},
        {{"cpy", AddressingMode::BASE_PAGE}, 0xC4},
        {{"cpy", AddressingMode::IMMEDIATE}, 0xC0},
        {{"cpz", AddressingMode::ABSOLUTE}, 0xDC},
        {{"cpz", AddressingMode::BASE_PAGE}, 0xD4},
        {{"cpz", AddressingMode::IMMEDIATE}, 0xC2},
        {{"dec", AddressingMode::ABSOLUTE}, 0xCE},
        {{"dec", AddressingMode::ABSOLUTE_X}, 0xDE},
        {{"dec", AddressingMode::ACCUMULATOR}, 0x3A},
        {{"dec", AddressingMode::BASE_PAGE}, 0xC6},
        {{"dec", AddressingMode::BASE_PAGE_X}, 0xD6},
        {{"dew", AddressingMode::BASE_PAGE}, 0xC3},
        {{"dex", AddressingMode::IMPLIED}, 0xCA},
        {{"dey", AddressingMode::IMPLIED}, 0x88},
        {{"dez", AddressingMode::IMPLIED}, 0x3B},
        {{"eom", AddressingMode::IMPLIED}, 0xEA},
        {{"eor", AddressingMode::BASE_PAGE_INDIRECT_Y}, 0x51},
        {{"eor", AddressingMode::BASE_PAGE_INDIRECT_Z}, 0x52},
        {{"eor", AddressingMode::FLAT_INDIRECT_Z}, 0x52},
        {{"eor", AddressingMode::STACK_RELATIVE}, 0x52},
        {{"eor", AddressingMode::BASE_PAGE_X_INDIRECT}, 0x41},
        {{"eor", AddressingMode::ABSOLUTE}, 0x4D},
        {{"eor", AddressingMode::ABSOLUTE_X}, 0x5D},
        {{"eor", AddressingMode::ABSOLUTE_Y}, 0x59},
        {{"eor", AddressingMode::BASE_PAGE}, 0x45},
        {{"eor", AddressingMode::BASE_PAGE_X}, 0x55},
        {{"eor", AddressingMode::IMMEDIATE}, 0x49},
        {{"inc", AddressingMode::ABSOLUTE}, 0xEE},
        {{"inc", AddressingMode::ABSOLUTE_X}, 0xFE},
        {{"inc", AddressingMode::ACCUMULATOR}, 0x1A},
        {{"inc", AddressingMode::BASE_PAGE}, 0xE6},
        {{"inc", AddressingMode::BASE_PAGE_X}, 0xF6},
        {{"inw", AddressingMode::BASE_PAGE}, 0xE3},
        {{"inx", AddressingMode::IMPLIED}, 0xE8},
        {{"iny", AddressingMode::IMPLIED}, 0xC8},
        {{"inz", AddressingMode::IMPLIED}, 0x1B},
        {{"jmp", AddressingMode::ABSOLUTE_INDIRECT}, 0x6C},
        {{"jmp", AddressingMode::ABSOLUTE_X_INDIRECT}, 0x7C},
        {{"jmp", AddressingMode::ABSOLUTE}, 0x4C},
        {{"jsr", AddressingMode::ABSOLUTE_INDIRECT}, 0x22},
        {{"jsr", AddressingMode::ABSOLUTE_X_INDIRECT}, 0x23},
        {{"jsr", AddressingMode::ABSOLUTE}, 0x20},
        {{"lda", AddressingMode::BASE_PAGE_INDIRECT_Y}, 0xB1},
        {{"lda", AddressingMode::BASE_PAGE_INDIRECT_Z}, 0xB2},
        {{"lda", AddressingMode::FLAT_INDIRECT_Z}, 0xB2},
        {{"lda", AddressingMode::STACK_RELATIVE}, 0xE2},
        {{"lda", AddressingMode::BASE_PAGE_X_INDIRECT}, 0xA1},
        {{"lda", AddressingMode::ABSOLUTE}, 0xAD},
        {{"lda", AddressingMode::ABSOLUTE_X}, 0xBD},
        {{"lda", AddressingMode::ABSOLUTE_Y}, 0xB9},
        {{"lda", AddressingMode::BASE_PAGE}, 0xA5},
        {{"lda", AddressingMode::BASE_PAGE_X}, 0xB5},
        {{"lda", AddressingMode::IMMEDIATE}, 0xA9},
        {{"ldx", AddressingMode::ABSOLUTE}, 0xAE},
        {{"ldx", AddressingMode::ABSOLUTE_Y}, 0xBE},
        {{"ldx", AddressingMode::BASE_PAGE}, 0xA6},
        {{"ldx", AddressingMode::BASE_PAGE_Y}, 0xB6},
        {{"ldx", AddressingMode::IMMEDIATE}, 0xA2},
        {{"ldy", AddressingMode::ABSOLUTE}, 0xAC},
        {{"ldy", AddressingMode::ABSOLUTE_X}, 0xBC},
        {{"ldy", AddressingMode::BASE_PAGE}, 0xA4},
        {{"ldy", AddressingMode::BASE_PAGE_X}, 0xB4},
        {{"ldy", AddressingMode::IMMEDIATE}, 0xA0},
        {{"ldz", AddressingMode::ABSOLUTE}, 0xAB},
        {{"ldz", AddressingMode::ABSOLUTE_X}, 0xBB},
        {{"ldz", AddressingMode::IMMEDIATE}, 0xA3},
        {{"ldq", AddressingMode::BASE_PAGE}, 0xA5},
        {{"ldq", AddressingMode::ABSOLUTE}, 0xAD},
        {{"ldq", AddressingMode::BASE_PAGE_INDIRECT_Z}, 0xB2},
        {{"ldq", AddressingMode::FLAT_INDIRECT_Z}, 0xB2},
        {{"lsr", AddressingMode::ABSOLUTE}, 0x4E},
        {{"lsr", AddressingMode::ABSOLUTE_X}, 0x5E},
        {{"lsr", AddressingMode::ACCUMULATOR}, 0x4A},
        {{"lsr", AddressingMode::BASE_PAGE}, 0x46},
        {{"lsr", AddressingMode::BASE_PAGE_X}, 0x56},
        {{"map", AddressingMode::IMPLIED}, 0x5C},
        {{"neg", AddressingMode::ACCUMULATOR}, 0x42},
        {{"nop", AddressingMode::IMPLIED}, 0xEA},
        {{"ora", AddressingMode::BASE_PAGE_INDIRECT_Y}, 0x11},
        {{"ora", AddressingMode::BASE_PAGE_INDIRECT_Z}, 0x12},
        {{"ora", AddressingMode::FLAT_INDIRECT_Z}, 0x12},
        {{"ora", AddressingMode::STACK_RELATIVE}, 0x12},
        {{"ora", AddressingMode::BASE_PAGE_X_INDIRECT}, 0x01},
        {{"ora", AddressingMode::ABSOLUTE}, 0x0D},
        {{"ora", AddressingMode::ABSOLUTE_X}, 0x1D},
        {{"ora", AddressingMode::ABSOLUTE_Y}, 0x19},
        {{"ora", AddressingMode::BASE_PAGE}, 0x05},
        {{"ora", AddressingMode::BASE_PAGE_X}, 0x15},
        {{"ora", AddressingMode::IMMEDIATE}, 0x09},
        {{"pha", AddressingMode::IMPLIED}, 0x48},
        {{"php", AddressingMode::IMPLIED}, 0x08},
        {{"phw", AddressingMode::ABSOLUTE}, 0xFC},
        {{"phw", AddressingMode::IMMEDIATE16}, 0xF4},
        {{"phx", AddressingMode::IMPLIED}, 0xDA},
        {{"phy", AddressingMode::IMPLIED}, 0x5A},
        {{"phz", AddressingMode::IMPLIED}, 0xDB},
        {{"pla", AddressingMode::IMPLIED}, 0x68},
        {{"plp", AddressingMode::IMPLIED}, 0x28},
        {{"plx", AddressingMode::IMPLIED}, 0xFA},
        {{"ply", AddressingMode::IMPLIED}, 0x7A},
        {{"plz", AddressingMode::IMPLIED}, 0xFB},
        {{"rmb0", AddressingMode::BASE_PAGE}, 0x07},
        {{"rmb1", AddressingMode::BASE_PAGE}, 0x17},
        {{"rmb2", AddressingMode::BASE_PAGE}, 0x27},
        {{"rmb3", AddressingMode::BASE_PAGE}, 0x37},
        {{"rmb4", AddressingMode::BASE_PAGE}, 0x47},
        {{"rmb5", AddressingMode::BASE_PAGE}, 0x57},
        {{"rmb6", AddressingMode::BASE_PAGE}, 0x67},
        {{"rmb7", AddressingMode::BASE_PAGE}, 0x77},
        {{"rol", AddressingMode::ABSOLUTE}, 0x2E},
        {{"rol", AddressingMode::ABSOLUTE_X}, 0x3E},
        {{"rol", AddressingMode::ACCUMULATOR}, 0x2A},
        {{"rol", AddressingMode::BASE_PAGE}, 0x26},
        {{"rol", AddressingMode::BASE_PAGE_X}, 0x36},
        {{"ror", AddressingMode::ABSOLUTE}, 0x6E},
        {{"ror", AddressingMode::ABSOLUTE_X}, 0x7E},
        {{"ror", AddressingMode::ACCUMULATOR}, 0x6A},
        {{"ror", AddressingMode::BASE_PAGE}, 0x66},
        {{"ror", AddressingMode::BASE_PAGE_X}, 0x76},
        {{"row", AddressingMode::ABSOLUTE}, 0xEB},
        {{"rti", AddressingMode::IMPLIED}, 0x40},
        {{"rts", AddressingMode::IMMEDIATE}, 0x62},
        {{"rts", AddressingMode::IMPLIED}, 0x60},
        {{"sbc", AddressingMode::BASE_PAGE_INDIRECT_Y}, 0xF1},
        {{"sbc", AddressingMode::BASE_PAGE_INDIRECT_Z}, 0xF2},
        {{"sbc", AddressingMode::FLAT_INDIRECT_Z}, 0xF2},
        {{"sbc", AddressingMode::STACK_RELATIVE}, 0xF2},
        {{"sbc", AddressingMode::BASE_PAGE_X_INDIRECT}, 0xE1},
        {{"sbc", AddressingMode::ABSOLUTE}, 0xED},
        {{"sbc", AddressingMode::ABSOLUTE_X}, 0xFD},
        {{"sbc", AddressingMode::ABSOLUTE_Y}, 0xF9},
        {{"sbc", AddressingMode::BASE_PAGE}, 0xE5},
        {{"sbc", AddressingMode::BASE_PAGE_X}, 0xF5},
        {{"sbc", AddressingMode::IMMEDIATE}, 0xE9},
        {{"sec", AddressingMode::IMPLIED}, 0x38},
        {{"sed", AddressingMode::IMPLIED}, 0xF8},
        {{"see", AddressingMode::IMPLIED}, 0x03},
        {{"sei", AddressingMode::IMPLIED}, 0x78},
        {{"smb0", AddressingMode::BASE_PAGE}, 0x87},
        {{"smb1", AddressingMode::BASE_PAGE}, 0x97},
        {{"smb2", AddressingMode::BASE_PAGE}, 0xA7},
        {{"smb3", AddressingMode::BASE_PAGE}, 0xB7},
        {{"smb4", AddressingMode::BASE_PAGE}, 0xC7},
        {{"smb5", AddressingMode::BASE_PAGE}, 0xD7},
        {{"smb6", AddressingMode::BASE_PAGE}, 0xE7},
        {{"smb7", AddressingMode::BASE_PAGE}, 0xF7},
        {{"sta", AddressingMode::BASE_PAGE_INDIRECT_Y}, 0x91},
        {{"sta", AddressingMode::BASE_PAGE_INDIRECT_Z}, 0x92},
        {{"sta", AddressingMode::FLAT_INDIRECT_Z}, 0x92},
        {{"sta", AddressingMode::STACK_RELATIVE}, 0x82},
        {{"sta", AddressingMode::BASE_PAGE_X_INDIRECT}, 0x81},
        {{"sta", AddressingMode::ABSOLUTE}, 0x8D},
        {{"sta", AddressingMode::ABSOLUTE_X}, 0x9D},
        {{"sta", AddressingMode::ABSOLUTE_Y}, 0x99},
        {{"sta", AddressingMode::BASE_PAGE}, 0x85},
        {{"sta", AddressingMode::BASE_PAGE_X}, 0x95},
        {{"stx", AddressingMode::ABSOLUTE}, 0x8E},
        {{"stx", AddressingMode::ABSOLUTE_Y}, 0x9B},
        {{"stx", AddressingMode::BASE_PAGE}, 0x86},
        {{"stx", AddressingMode::BASE_PAGE_Y}, 0x96},
        {{"sty", AddressingMode::ABSOLUTE}, 0x8C},
        {{"sty", AddressingMode::ABSOLUTE_X}, 0x8B},
        {{"sty", AddressingMode::BASE_PAGE}, 0x84},
        {{"sty", AddressingMode::BASE_PAGE_X}, 0x94},
        {{"stz", AddressingMode::ABSOLUTE}, 0x9C},
        {{"stz", AddressingMode::ABSOLUTE_X}, 0x9E},
        {{"stz", AddressingMode::BASE_PAGE}, 0x64},
        {{"stz", AddressingMode::BASE_PAGE_X}, 0x74},
        {{"stq", AddressingMode::BASE_PAGE}, 0x85},
        {{"stq", AddressingMode::ABSOLUTE}, 0x8D},
        {{"stq", AddressingMode::BASE_PAGE_INDIRECT_Z}, 0x92},
        {{"stq", AddressingMode::FLAT_INDIRECT_Z}, 0x92},
        {{"tab", AddressingMode::IMPLIED}, 0x5B},
        {{"tax", AddressingMode::IMPLIED}, 0xAA},
        {{"tay", AddressingMode::IMPLIED}, 0xA8},
        {{"taz", AddressingMode::IMPLIED}, 0x4B},
        {{"tba", AddressingMode::IMPLIED}, 0x7B},
        {{"trb", AddressingMode::ABSOLUTE}, 0x1C},
        {{"trb", AddressingMode::BASE_PAGE}, 0x14},
        {{"tsb", AddressingMode::ABSOLUTE}, 0x0C},
        {{"tsb", AddressingMode::BASE_PAGE}, 0x04},
        {{"tsx", AddressingMode::IMPLIED}, 0xBA},
        {{"tsy", AddressingMode::IMPLIED}, 0x0B},
        {{"txa", AddressingMode::IMPLIED}, 0x8A},
        {{"txs", AddressingMode::IMPLIED}, 0x9A},
        {{"tya", AddressingMode::IMPLIED}, 0x98},
        {{"tys", AddressingMode::IMPLIED}, 0x2B},
        {{"tza", AddressingMode::IMPLIED}, 0x6B},
    };
    return opcodes;
}

uint8_t AssemblerOpcodeDatabase::getOpcode(const std::string& m, AddressingMode mode) {
    std::string lowerM = m;
    std::transform(lowerM.begin(), lowerM.end(), lowerM.begin(), ::tolower);
    std::string baseM = lowerM;
    if (lowerM.size() > 1 && lowerM.back() == 'q' && lowerM != "ldq" && lowerM != "stq" && lowerM.substr(0, 3) != "beq" && lowerM.substr(0, 3) != "bne" && lowerM.substr(0, 3) != "bra" && lowerM.substr(0, 3) != "bsr") 
        baseM = lowerM.substr(0, lowerM.size() - 1);

    const auto& opMap = getOpcodeMap();
    auto it = opMap.find({baseM, mode});
    if (it != opMap.end()) return it->second;
    return 0;
}

std::vector<AddressingMode> AssemblerOpcodeDatabase::getValidAddressingModes(const std::string& mnemonic) {
    std::string lowerM = mnemonic;
    std::transform(lowerM.begin(), lowerM.end(), lowerM.begin(), ::tolower);
    std::string baseM = lowerM;
    if (lowerM.size() > 1 && lowerM.back() == 'q' && lowerM != "ldq" && lowerM != "stq" && lowerM.substr(0, 3) != "beq" && lowerM.substr(0, 3) != "bne" && lowerM.substr(0, 3) != "bra" && lowerM.substr(0, 3) != "bsr") 
        baseM = lowerM.substr(0, lowerM.size() - 1);

    std::vector<AddressingMode> modes;
    const auto& opMap = getOpcodeMap();
    for (const auto& entry : opMap) {
        if (entry.first.first == baseM) modes.push_back(entry.first.second);
    }
    return modes;
}

std::string AssemblerOpcodeDatabase::AddressingModeToString(AddressingMode mode) {
    switch (mode) {
        case AddressingMode::IMPLIED: return "Implied";
        case AddressingMode::ACCUMULATOR: return "A";
        case AddressingMode::IMMEDIATE: return "#imm";
        case AddressingMode::IMMEDIATE16: return "#imm16";
        case AddressingMode::BASE_PAGE: return "zp";
        case AddressingMode::BASE_PAGE_X: return "zp,X";
        case AddressingMode::BASE_PAGE_Y: return "zp,Y";
        case AddressingMode::ABSOLUTE: return "abs";
        case AddressingMode::ABSOLUTE_X: return "abs,X";
        case AddressingMode::ABSOLUTE_Y: return "abs,Y";
        case AddressingMode::INDIRECT: return "(zp)";
        case AddressingMode::BASE_PAGE_X_INDIRECT: return "(zp,X)";
        case AddressingMode::BASE_PAGE_INDIRECT_Y: return "(zp),Y";
        case AddressingMode::BASE_PAGE_INDIRECT_Z: return "(zp),Z";
        case AddressingMode::BASE_PAGE_INDIRECT_SP_Y: return "(zp,SP),Y";
        case AddressingMode::ABSOLUTE_INDIRECT: return "(abs)";
        case AddressingMode::ABSOLUTE_X_INDIRECT: return "(abs,X)";
        case AddressingMode::RELATIVE: return "rel";
        case AddressingMode::RELATIVE16: return "rel16";
        case AddressingMode::BASE_PAGE_RELATIVE: return "zp,rel";
        case AddressingMode::STACK_RELATIVE: return "offset,s";
        case AddressingMode::STACK_RELATIVE_INDIRECT_Y: return "(offset,s),y";
        case AddressingMode::LINEAR_ABSOLUTE: return "lin_abs";
        case AddressingMode::LINEAR_ABSOLUTE_X: return "lin_abs,X";
        case AddressingMode::LINEAR_ABSOLUTE_Y: return "lin_abs,Y";
        case AddressingMode::FLAT_INDIRECT_Z: return "[zp],Z";
        case AddressingMode::QUAD_Q: return "Q";
        case AddressingMode::NONE: return "None";
        default: return "Unknown";
    }
}
