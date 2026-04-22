#include "M65Emitter.hpp"
#include "AssemblerOpcodeDatabase.hpp"
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iostream>

M65Emitter::M65Emitter(std::ostream& out, uint32_t zpStart) : out(&out), mode(Mode::TEXT), zeroPageStart(zpStart) {}
M65Emitter::M65Emitter(std::vector<uint8_t>& binary, uint32_t zpStart) : binary(&binary), mode(Mode::BINARY), zeroPageStart(zpStart) {}

static std::string hex8(uint8_t val) {
    std::stringstream ss;
    ss << "$" << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)val;
    return ss.str();
}

static std::string hex16(uint16_t val) {
    std::stringstream ss;
    ss << "$" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << (int)val;
    return ss.str();
}

void M65Emitter::emitByte(uint8_t b) {
    if (mode == Mode::BINARY) {
        binary->push_back(b);
        currentAddress++;
    }
    else emitText(".byte", hex8(b));
}
void M65Emitter::emitWord(uint16_t w) {
    if (mode == Mode::BINARY) {
        binary->push_back(w & 0xFF);
        binary->push_back(w >> 8);
        currentAddress += 2;
    }
    else emitText(".word", hex16(w));
}

void M65Emitter::setAddress(uint32_t addr) {
    if (!addressSet) {
        currentAddress = addr;
        addressSet = true;
        if (mode == Mode::TEXT) emitText(".org", hex16((uint16_t)addr));
        return;
    }

    if (addr < currentAddress) {
        throw std::runtime_error("Cannot set address backward: current=" + hex16((uint16_t)currentAddress) + " new=" + hex16((uint16_t)addr));
    }

    if (addr > currentAddress) {
        uint32_t gap = addr - currentAddress;
        if (mode == Mode::BINARY) {
            for (uint32_t i = 0; i < gap; ++i) binary->push_back(0);
        } else {
            emitText(".org", hex16((uint16_t)addr));
        }
        currentAddress = addr;
    }
}
void M65Emitter::emitText(const std::string& mnemonic, const std::string& operand) {
    *out << "    " << std::left << std::setw(8) << mnemonic << operand << std::endl;
}

void M65Emitter::emitLabel(const std::string& label) {
    if (mode == Mode::TEXT) *out << label << ":" << std::endl;
}

void M65Emitter::emitComment(const std::string& comment) {
    if (mode == Mode::TEXT) *out << "    ; " << comment << std::endl;
}

void M65Emitter::emitInstruction(const std::string& mnemonic, AddressingMode amode, uint32_t value, bool hasValue) {
    if (mode == Mode::TEXT) {
        std::string operand = "";
        if (hasValue) {
            switch (amode) {
                case AddressingMode::IMMEDIATE: operand = "#" + hex8((uint8_t)value); break;
                case AddressingMode::IMMEDIATE16: operand = "#" + hex16((uint16_t)value); break;
                case AddressingMode::BASE_PAGE: operand = hex8((uint8_t)value); break;
                case AddressingMode::BASE_PAGE_X: operand = hex8((uint8_t)value) + ",x"; break;
                case AddressingMode::BASE_PAGE_Y: operand = hex8((uint8_t)value) + ",y"; break;
                case AddressingMode::ABSOLUTE: operand = hex16((uint16_t)value); break;
                case AddressingMode::ABSOLUTE_X: operand = hex16((uint16_t)value) + ",x"; break;
                case AddressingMode::ABSOLUTE_Y: operand = hex16((uint16_t)value) + ",y"; break;
                case AddressingMode::INDIRECT: operand = "(" + hex8((uint8_t)value) + ")"; break;
                case AddressingMode::BASE_PAGE_X_INDIRECT: operand = "(" + hex8((uint8_t)value) + ",x)"; break;
                case AddressingMode::BASE_PAGE_INDIRECT_Y: operand = "(" + hex8((uint8_t)value) + "),y"; break;
                case AddressingMode::BASE_PAGE_INDIRECT_Z: operand = "(" + hex8((uint8_t)value) + "),z"; break;
                case AddressingMode::ABSOLUTE_INDIRECT: operand = "(" + hex16((uint16_t)value) + ")"; break;
                case AddressingMode::ABSOLUTE_X_INDIRECT: operand = "(" + hex16((uint16_t)value) + ",x)"; break;
                case AddressingMode::STACK_RELATIVE: operand = std::to_string((int)value) + ",s"; break;
                case AddressingMode::FLAT_INDIRECT_Z: operand = "[" + hex8((uint8_t)value) + "],z"; break;
                case AddressingMode::RELATIVE: {
                    int8_t v = (int8_t)value;
                    operand = "*" + (v >= 0 ? std::string("+") : "") + std::to_string((int)v);
                    break;
                }
                case AddressingMode::RELATIVE16: {
                    int16_t v = (int16_t)value;
                    operand = "*" + (v >= 0 ? std::string("+") : "") + std::to_string((int)v);
                    break;
                }
                default: operand = hex16((uint16_t)value); break;
            }
        } else if (amode == AddressingMode::ACCUMULATOR) {
            operand = "a";
        }
        emitText(mnemonic, operand);
    } else {
        std::string lowerMnemonic = mnemonic;
        std::transform(lowerMnemonic.begin(), lowerMnemonic.end(), lowerMnemonic.begin(), [](unsigned char c){ return std::tolower(c); });
        uint8_t op = AssemblerOpcodeDatabase::getOpcode(lowerMnemonic, amode);
        if (op == 0 && lowerMnemonic != "brk") { std::cout << "DEBUG: " << lowerMnemonic << " amode: " << (int)amode << std::endl; throw std::runtime_error("Invalid opcode for " + lowerMnemonic); }
        emitByte(op);
        if (hasValue) {
            switch (amode) {
                case AddressingMode::IMMEDIATE:
                case AddressingMode::BASE_PAGE:
                case AddressingMode::BASE_PAGE_X:
                case AddressingMode::BASE_PAGE_Y:
                case AddressingMode::INDIRECT:
                case AddressingMode::BASE_PAGE_X_INDIRECT:
                case AddressingMode::BASE_PAGE_INDIRECT_Y:
                case AddressingMode::BASE_PAGE_INDIRECT_Z:
                case AddressingMode::STACK_RELATIVE:
                case AddressingMode::FLAT_INDIRECT_Z:
                case AddressingMode::RELATIVE:
                    emitByte((uint8_t)value);
                    break;
                case AddressingMode::IMMEDIATE16:
                case AddressingMode::ABSOLUTE:
                case AddressingMode::ABSOLUTE_X:
                case AddressingMode::ABSOLUTE_Y:
                case AddressingMode::ABSOLUTE_INDIRECT:
                case AddressingMode::ABSOLUTE_X_INDIRECT:
                case AddressingMode::RELATIVE16:
                    emitWord((uint16_t)value);
                    break;
                default:
                    break;
            }
        }
    }
}

void M65Emitter::lda_imm(uint8_t val) { emitInstruction("lda", AddressingMode::IMMEDIATE, val, true); }
void M65Emitter::ldx_imm(uint8_t val) { emitInstruction("ldx", AddressingMode::IMMEDIATE, val, true); }
void M65Emitter::ldy_imm(uint8_t val) { emitInstruction("ldy", AddressingMode::IMMEDIATE, val, true); }
void M65Emitter::ldz_imm(uint8_t val) { emitInstruction("ldz", AddressingMode::IMMEDIATE, val, true); }
void M65Emitter::phw_imm(uint16_t val) { emitInstruction("phw", AddressingMode::IMMEDIATE16, val, true); }
void M65Emitter::adc_imm(uint8_t val) { emitInstruction("adc", AddressingMode::IMMEDIATE, val, true); }
void M65Emitter::sbc_imm(uint8_t val) { emitInstruction("sbc", AddressingMode::IMMEDIATE, val, true); }
void M65Emitter::and_imm(uint8_t val) { emitInstruction("and", AddressingMode::IMMEDIATE, val, true); }
void M65Emitter::ora_imm(uint8_t val) { emitInstruction("ora", AddressingMode::IMMEDIATE, val, true); }
void M65Emitter::eor_imm(uint8_t val) { emitInstruction("eor", AddressingMode::IMMEDIATE, val, true); }
void M65Emitter::cmp_imm(uint8_t val) { emitInstruction("cmp", AddressingMode::IMMEDIATE, val, true); }

// --- Absolute Mode ---
void M65Emitter::lda_abs(uint16_t addr) { emitInstruction("lda", AddressingMode::ABSOLUTE, addr, true); }
void M65Emitter::ldx_abs(uint16_t addr) { emitInstruction("ldx", AddressingMode::ABSOLUTE, addr, true); }
void M65Emitter::ldy_abs(uint16_t addr) { emitInstruction("ldy", AddressingMode::ABSOLUTE, addr, true); }
void M65Emitter::ldz_abs(uint16_t addr) { emitInstruction("ldz", AddressingMode::ABSOLUTE, addr, true); }
void M65Emitter::sta_abs(uint16_t addr) { emitInstruction("sta", AddressingMode::ABSOLUTE, addr, true); }
void M65Emitter::stx_abs(uint16_t addr) { emitInstruction("stx", AddressingMode::ABSOLUTE, addr, true); }
void M65Emitter::sty_abs(uint16_t addr) { emitInstruction("sty", AddressingMode::ABSOLUTE, addr, true); }
void M65Emitter::stz_abs(uint16_t addr) { emitInstruction("stz", AddressingMode::ABSOLUTE, addr, true); }
void M65Emitter::lda_abs_x(uint16_t addr) { emitInstruction("lda", AddressingMode::ABSOLUTE_X, addr, true); }
void M65Emitter::sta_abs_x(uint16_t addr) { emitInstruction("sta", AddressingMode::ABSOLUTE_X, addr, true); }
void M65Emitter::stz_abs_x(uint16_t addr) { emitInstruction("stz", AddressingMode::ABSOLUTE_X, addr, true); }
void M65Emitter::adc_abs(uint16_t addr) { emitInstruction("adc", AddressingMode::ABSOLUTE, addr, true); }
void M65Emitter::sbc_abs(uint16_t addr) { emitInstruction("sbc", AddressingMode::ABSOLUTE, addr, true); }
void M65Emitter::and_abs(uint16_t addr) { emitInstruction("and", AddressingMode::ABSOLUTE, addr, true); }
void M65Emitter::ora_abs(uint16_t addr) { emitInstruction("ora", AddressingMode::ABSOLUTE, addr, true); }
void M65Emitter::eor_abs(uint16_t addr) { emitInstruction("eor", AddressingMode::ABSOLUTE, addr, true); }
void M65Emitter::cmp_abs(uint16_t addr) { emitInstruction("cmp", AddressingMode::ABSOLUTE, addr, true); }
void M65Emitter::asw_abs(uint16_t addr) { emitInstruction("asw", AddressingMode::ABSOLUTE, addr, true); }
void M65Emitter::row_abs(uint16_t addr) { emitInstruction("row", AddressingMode::ABSOLUTE, addr, true); }

// --- Zero Page Mode ---
void M65Emitter::lda_zp(uint8_t addr) { emitInstruction("lda", AddressingMode::BASE_PAGE, addr, true); }
void M65Emitter::sta_zp(uint8_t addr) { emitInstruction("sta", AddressingMode::BASE_PAGE, addr, true); }
void M65Emitter::stx_zp(uint8_t addr) { emitInstruction("stx", AddressingMode::BASE_PAGE, addr, true); }
void M65Emitter::stz_zp(uint8_t addr) { emitInstruction("stz", AddressingMode::BASE_PAGE, addr, true); }
void M65Emitter::adc_zp(uint8_t addr) { emitInstruction("adc", AddressingMode::BASE_PAGE, addr, true); }
void M65Emitter::sbc_zp(uint8_t addr) { emitInstruction("sbc", AddressingMode::BASE_PAGE, addr, true); }
void M65Emitter::and_zp(uint8_t addr) { emitInstruction("and", AddressingMode::BASE_PAGE, addr, true); }
void M65Emitter::ora_zp(uint8_t addr) { emitInstruction("ora", AddressingMode::BASE_PAGE, addr, true); }
void M65Emitter::eor_zp(uint8_t addr) { emitInstruction("eor", AddressingMode::BASE_PAGE, addr, true); }
void M65Emitter::inc_zp(uint8_t addr) { emitInstruction("inc", AddressingMode::BASE_PAGE, addr, true); }
void M65Emitter::dec_zp(uint8_t addr) { emitInstruction("dec", AddressingMode::BASE_PAGE, addr, true); }
void M65Emitter::inc_abs(uint16_t addr) { emitInstruction("inc", AddressingMode::ABSOLUTE, addr, true); }
void M65Emitter::dec_abs(uint16_t addr) { emitInstruction("dec", AddressingMode::ABSOLUTE, addr, true); }
void M65Emitter::bit_zp(uint8_t addr) { emitInstruction("bit", AddressingMode::BASE_PAGE, addr, true); }
void M65Emitter::cmp_zp(uint8_t addr) { emitInstruction("cmp", AddressingMode::BASE_PAGE, addr, true); }
void M65Emitter::inc_abs_x(uint16_t addr) { emitInstruction("inc", AddressingMode::ABSOLUTE_X, addr, true); }
void M65Emitter::dec_abs_x(uint16_t addr) { emitInstruction("dec", AddressingMode::ABSOLUTE_X, addr, true); }

// --- Other Addressing Modes ---
void M65Emitter::lda_stack(uint8_t offset) { emitInstruction("lda", AddressingMode::STACK_RELATIVE, offset, true); }
void M65Emitter::sta_stack(uint8_t offset) { emitInstruction("sta", AddressingMode::STACK_RELATIVE, offset, true); }
void M65Emitter::stx_stack(uint8_t offset) { 
    if (mode == Mode::TEXT) emitText("stx", std::to_string((int)offset) + ", s"); 
    else { 
        txa();
        sta_stack(offset);
    } 
}
void M65Emitter::sty_stack(uint8_t offset) { 
    if (mode == Mode::TEXT) emitText("sty", std::to_string((int)offset) + ", s"); 
    else { 
        tya();
        sta_stack(offset);
    } 
}
void M65Emitter::stz_stack(uint8_t offset) { 
    if (mode == Mode::TEXT) emitText("stz", std::to_string((int)offset) + ", s"); 
    else { 
        tsx();
        emitByte(0x9E);
        emitWord(0x0101 + offset);
    } 
}

void M65Emitter::lda_ind_z(uint8_t addr, bool flat) {
    if (mode == Mode::TEXT) {
        if (flat) emitText("lda", "[" + hex8(addr) + "],z");
        else { emitText("ldy", "#$00"); emitText("lda", "(" + hex8(addr) + "),y"); }
    } else {
        if (flat) { eom(); emitByte(0xB2); emitByte(addr); }
        else { ldy_imm(0); emitByte(0xB1); emitByte(addr); }
    }
}
void M65Emitter::sta_ind_z(uint8_t addr, bool flat) {
    if (mode == Mode::TEXT) {
        if (flat) emitText("sta", "[" + hex8(addr) + "],z");
        else { emitText("ldy", "#$00"); emitText("sta", "(" + hex8(addr) + "),y"); }
    } else {
        if (flat) { eom(); emitByte(0x92); emitByte(addr); }
        else { ldy_imm(0); emitByte(0x91); emitByte(addr); }
    }
}

void M65Emitter::bit_abs(uint16_t addr) { emitInstruction("bit", AddressingMode::ABSOLUTE, addr, true); }

// --- Register Transfers ---
void M65Emitter::tax() { emitInstruction("tax", AddressingMode::IMPLIED); }
void M65Emitter::txa() { emitInstruction("txa", AddressingMode::IMPLIED); }
void M65Emitter::tay() { emitInstruction("tay", AddressingMode::IMPLIED); }
void M65Emitter::tya() { emitInstruction("tya", AddressingMode::IMPLIED); }
void M65Emitter::taz() { emitInstruction("taz", AddressingMode::IMPLIED); }
void M65Emitter::tza() { emitInstruction("tza", AddressingMode::IMPLIED); }
void M65Emitter::tsx() { emitInstruction("tsx", AddressingMode::IMPLIED); }
void M65Emitter::txs() { emitInstruction("txs", AddressingMode::IMPLIED); }
void M65Emitter::inx() { emitInstruction("inx", AddressingMode::IMPLIED); }
void M65Emitter::dex() { emitInstruction("dex", AddressingMode::IMPLIED); }
void M65Emitter::iny() { emitInstruction("iny", AddressingMode::IMPLIED); }
void M65Emitter::dey() { emitInstruction("dey", AddressingMode::IMPLIED); }
void M65Emitter::inz() { emitInstruction("inz", AddressingMode::IMPLIED); }
void M65Emitter::dez() { emitInstruction("dez", AddressingMode::IMPLIED); }

// --- Stack Operations ---
void M65Emitter::pha() { emitInstruction("pha", AddressingMode::IMPLIED); }
void M65Emitter::pla() { emitInstruction("pla", AddressingMode::IMPLIED); }
void M65Emitter::phx() { emitInstruction("phx", AddressingMode::IMPLIED); }
void M65Emitter::plx() { emitInstruction("plx", AddressingMode::IMPLIED); }
void M65Emitter::phy() { emitInstruction("phy", AddressingMode::IMPLIED); }
void M65Emitter::ply() { emitInstruction("ply", AddressingMode::IMPLIED); }
void M65Emitter::phz() { emitInstruction("phz", AddressingMode::IMPLIED); }
void M65Emitter::plz() { emitInstruction("plz", AddressingMode::IMPLIED); }

// --- ALU & Branching ---
void M65Emitter::clc() { emitInstruction("clc", AddressingMode::IMPLIED); }
void M65Emitter::sec() { emitInstruction("sec", AddressingMode::IMPLIED); }
void M65Emitter::cla() { lda_imm(0); }
void M65Emitter::clx() { ldx_imm(0); }
void M65Emitter::cly() { ldy_imm(0); }
void M65Emitter::clz() { ldz_imm(0); }
void M65Emitter::neg_a() { emitInstruction("neg", AddressingMode::ACCUMULATOR); }
void M65Emitter::asl_a() { emitInstruction("asl", AddressingMode::ACCUMULATOR); }
void M65Emitter::rol_a() { emitInstruction("rol", AddressingMode::ACCUMULATOR); }
void M65Emitter::lsr_a() { emitInstruction("lsr", AddressingMode::ACCUMULATOR); }
void M65Emitter::ror_a() { emitInstruction("ror", AddressingMode::ACCUMULATOR); }
void M65Emitter::inc_a() { emitInstruction("inc", AddressingMode::ACCUMULATOR); }
void M65Emitter::dec_a() { emitInstruction("dec", AddressingMode::ACCUMULATOR); }
void M65Emitter::eom() { emitInstruction("eom", AddressingMode::IMPLIED); }

void M65Emitter::bra(int8_t offset) { emitInstruction("bra", AddressingMode::RELATIVE, (uint32_t)offset, true); }
void M65Emitter::bne(int8_t offset) { emitInstruction("bne", AddressingMode::RELATIVE, (uint32_t)offset, true); }
void M65Emitter::beq(int8_t offset) { emitInstruction("beq", AddressingMode::RELATIVE, (uint32_t)offset, true); }
void M65Emitter::bcc(int8_t offset) { emitInstruction("bcc", AddressingMode::RELATIVE, (uint32_t)offset, true); }
void M65Emitter::bcs(int8_t offset) { emitInstruction("bcs", AddressingMode::RELATIVE, (uint32_t)offset, true); }

void M65Emitter::add_16_imm(uint16_t val) {
    if (mode == Mode::TEXT) {
        std::stringstream ss;
        ss << "#" << hex16(val);
        emitText("add.16", ".ax, " + ss.str());
    } else {
        clc();
        adc_imm(val & 0xFF);
        pha(); txa();
        adc_imm(val >> 8);
        tax(); pla();
    }
}

void M65Emitter::sub_16_imm(uint16_t val) {
    if (mode == Mode::TEXT) {
        std::stringstream ss;
        ss << "#" << hex16(val);
        emitText("sub.16", ".ax, " + ss.str());
    } else {
        sec();
        sbc_imm(val & 0xFF);
        pha(); txa();
        sbc_imm(val >> 8);
        tax(); pla();
    }
}

void M65Emitter::neg_16() {
    if (mode == Mode::TEXT) emitText("neg.16");
    else { eor_imm(0xFF); pha(); txa(); eor_imm(0xFF); adc_imm(0); tax(); pla(); }
}
void M65Emitter::not_16() {
    if (mode == Mode::TEXT) emitText("not.16");
    else { eor_imm(0xFF); pha(); txa(); eor_imm(0xFF); tax(); pla(); }
}

void M65Emitter::push_ax() { phx(); pha(); }
void M65Emitter::pop_ax() { pla(); plx(); }
void M65Emitter::transfer_ax_to_zp(uint8_t addr) { sta_zp(addr); stx_zp(addr + 1); }
