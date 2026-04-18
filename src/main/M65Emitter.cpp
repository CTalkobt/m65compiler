#include "M65Emitter.hpp"
#include <iomanip>
#include <sstream>

M65Emitter::M65Emitter(std::ostream& out, uint32_t zpStart) : mode(Mode::TEXT), out(&out), binary(nullptr), zeroPageStart(zpStart) {}
M65Emitter::M65Emitter(std::vector<uint8_t>& binary, uint32_t zpStart) : mode(Mode::BINARY), out(nullptr), binary(&binary), zeroPageStart(zpStart) {}

void M65Emitter::emitByte(uint8_t b) { if (binary) binary->push_back(b); }
void M65Emitter::emitWord(uint16_t w) { emitByte(w & 0xFF); emitByte(w >> 8); }

void M65Emitter::emitText(const std::string& mnemonic, const std::string& operand) {
    if (out) {
        *out << "    " << std::left << std::setw(8) << mnemonic;
        if (!operand.empty()) *out << " " << operand;
        *out << std::endl;
    }
}

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

// --- Immediate Mode ---
void M65Emitter::lda_imm(uint8_t val) { if (mode == Mode::TEXT) emitText("LDA", "#" + hex8(val)); else { emitByte(0xA9); emitByte(val); } }
void M65Emitter::ldx_imm(uint8_t val) { if (mode == Mode::TEXT) emitText("LDX", "#" + hex8(val)); else { emitByte(0xA2); emitByte(val); } }
void M65Emitter::ldy_imm(uint8_t val) { if (mode == Mode::TEXT) emitText("LDY", "#" + hex8(val)); else { emitByte(0xA0); emitByte(val); } }
void M65Emitter::ldz_imm(uint8_t val) { if (mode == Mode::TEXT) emitText("LDZ", "#" + hex8(val)); else { emitByte(0xA3); emitByte(val); } }
void M65Emitter::phw_imm(uint16_t val) { if (mode == Mode::TEXT) emitText("PHW", "#" + hex16(val)); else { emitByte(0xF4); emitWord(val); } }
void M65Emitter::adc_imm(uint8_t val) { if (mode == Mode::TEXT) emitText("ADC", "#" + hex8(val)); else { emitByte(0x69); emitByte(val); } }
void M65Emitter::sbc_imm(uint8_t val) { if (mode == Mode::TEXT) emitText("SBC", "#" + hex8(val)); else { emitByte(0xE9); emitByte(val); } }
void M65Emitter::and_imm(uint8_t val) { if (mode == Mode::TEXT) emitText("AND", "#" + hex8(val)); else { emitByte(0x29); emitByte(val); } }
void M65Emitter::ora_imm(uint8_t val) { if (mode == Mode::TEXT) emitText("ORA", "#" + hex8(val)); else { emitByte(0x09); emitByte(val); } }
void M65Emitter::eor_imm(uint8_t val) { if (mode == Mode::TEXT) emitText("EOR", "#" + hex8(val)); else { emitByte(0x49); emitByte(val); } }

// --- Absolute Mode ---
void M65Emitter::lda_abs(uint16_t addr) { if (mode == Mode::TEXT) emitText("LDA", hex16(addr)); else { emitByte(0xAD); emitWord(addr); } }
void M65Emitter::ldx_abs(uint16_t addr) { if (mode == Mode::TEXT) emitText("LDX", hex16(addr)); else { emitByte(0xAE); emitWord(addr); } }
void M65Emitter::ldy_abs(uint16_t addr) { if (mode == Mode::TEXT) emitText("LDY", hex16(addr)); else { emitByte(0xAC); emitWord(addr); } }
void M65Emitter::ldz_abs(uint16_t addr) { if (mode == Mode::TEXT) emitText("LDZ", hex16(addr)); else { emitByte(0xAB); emitWord(addr); } }
void M65Emitter::sta_abs(uint16_t addr) { if (mode == Mode::TEXT) emitText("STA", hex16(addr)); else { emitByte(0x8D); emitWord(addr); } }
void M65Emitter::stx_abs(uint16_t addr) { if (mode == Mode::TEXT) emitText("STX", hex16(addr)); else { emitByte(0x8E); emitWord(addr); } }
void M65Emitter::sty_abs(uint16_t addr) { if (mode == Mode::TEXT) emitText("STY", hex16(addr)); else { emitByte(0x8C); emitWord(addr); } }
void M65Emitter::stz_abs(uint16_t addr) { if (mode == Mode::TEXT) emitText("STZ", hex16(addr)); else { emitByte(0x9C); emitWord(addr); } }
void M65Emitter::lda_abs_x(uint16_t addr) { if (mode == Mode::TEXT) emitText("LDA", hex16(addr) + ",X"); else { emitByte(0xBD); emitWord(addr); } }
void M65Emitter::sta_abs_x(uint16_t addr) { if (mode == Mode::TEXT) emitText("STA", hex16(addr) + ",X"); else { emitByte(0x9D); emitWord(addr); } }
void M65Emitter::adc_abs(uint16_t addr) { if (mode == Mode::TEXT) emitText("ADC", hex16(addr)); else { emitByte(0x6D); emitWord(addr); } }
void M65Emitter::sbc_abs(uint16_t addr) { if (mode == Mode::TEXT) emitText("SBC", hex16(addr)); else { emitByte(0xED); emitWord(addr); } }

// --- Zero Page Mode ---
void M65Emitter::lda_zp(uint8_t addr) { if (mode == Mode::TEXT) emitText("LDA", hex8(addr)); else { emitByte(0xA5); emitByte(addr); } }
void M65Emitter::sta_zp(uint8_t addr) { if (mode == Mode::TEXT) emitText("STA", hex8(addr)); else { emitByte(0x85); emitByte(addr); } }
void M65Emitter::stx_zp(uint8_t addr) { if (mode == Mode::TEXT) emitText("STX", hex8(addr)); else { emitByte(0x86); emitByte(addr); } }
void M65Emitter::stz_zp(uint8_t addr) { if (mode == Mode::TEXT) emitText("STZ", hex8(addr)); else { emitByte(0x64); emitByte(addr); } }
void M65Emitter::adc_zp(uint8_t addr) { if (mode == Mode::TEXT) emitText("ADC", hex8(addr)); else { emitByte(0x65); emitByte(addr); } }
void M65Emitter::sbc_zp(uint8_t addr) { if (mode == Mode::TEXT) emitText("SBC", hex8(addr)); else { emitByte(0xE5); emitByte(addr); } }
void M65Emitter::and_zp(uint8_t addr) { if (mode == Mode::TEXT) emitText("AND", hex8(addr)); else { emitByte(0x25); emitByte(addr); } }
void M65Emitter::ora_zp(uint8_t addr) { if (mode == Mode::TEXT) emitText("ORA", hex8(addr)); else { emitByte(0x05); emitByte(addr); } }
void M65Emitter::eor_zp(uint8_t addr) { if (mode == Mode::TEXT) emitText("EOR", hex8(addr)); else { emitByte(0x45); emitByte(addr); } }
void M65Emitter::inc_zp(uint8_t addr) { if (mode == Mode::TEXT) emitText("INC", hex8(addr)); else { emitByte(0xE6); emitByte(addr); } }
void M65Emitter::dec_zp(uint8_t addr) { if (mode == Mode::TEXT) emitText("DEC", hex8(addr)); else { emitByte(0xC6); emitByte(addr); } }
void M65Emitter::bit_zp(uint8_t addr) { if (mode == Mode::TEXT) emitText("BIT", hex8(addr)); else { emitByte(0x24); emitByte(addr); } }
void M65Emitter::cmp_zp(uint8_t addr) { if (mode == Mode::TEXT) emitText("CMP", hex8(addr)); else { emitByte(0xC5); emitByte(addr); } }
void M65Emitter::inc_abs_x(uint16_t addr) { if (mode == Mode::TEXT) emitText("INC", hex16(addr) + ",X"); else { emitByte(0xFE); emitWord(addr); } }
void M65Emitter::dec_abs_x(uint16_t addr) { if (mode == Mode::TEXT) emitText("DEC", hex16(addr) + ",X"); else { emitByte(0xDE); emitWord(addr); } }

// --- Other Addressing Modes ---
void M65Emitter::lda_stack(uint8_t offset) { if (mode == Mode::TEXT) emitText("LDA", std::to_string((int)offset) + ", s"); else { emitByte(0xE2); emitByte(offset); } }
void M65Emitter::sta_stack(uint8_t offset) { if (mode == Mode::TEXT) emitText("STA", std::to_string((int)offset) + ", s"); else { emitByte(0x82); emitByte(offset); } }
void M65Emitter::lda_ind_z(uint8_t addr, bool flat) {
    if (mode == Mode::TEXT) {
        if (flat) emitText("LDA", "[" + hex8(addr) + "],Z");
        else { emitText("LDY", "#$00"); emitText("LDA", "(" + hex8(addr) + "),Y"); }
    } else {
        if (flat) { eom(); emitByte(0xB2); emitByte(addr); }
        else { ldy_imm(0); emitByte(0xB1); emitByte(addr); }
    }
}
void M65Emitter::sta_ind_z(uint8_t addr, bool flat) {
    if (mode == Mode::TEXT) {
        if (flat) emitText("STA", "[" + hex8(addr) + "],Z");
        else { emitText("LDY", "#$00"); emitText("STA", "(" + hex8(addr) + "),Y"); }
    } else {
        if (flat) { eom(); emitByte(0x92); emitByte(addr); }
        else { ldy_imm(0); emitByte(0x91); emitByte(addr); }
    }
}
void M65Emitter::bit_abs(uint16_t addr) { if (mode == Mode::TEXT) emitText("BIT", hex16(addr)); else { emitByte(0x2C); emitWord(addr); } }

// --- Register Transfers ---
void M65Emitter::tax() { if (mode == Mode::TEXT) emitText("TAX"); else emitByte(0xAA); }
void M65Emitter::txa() { if (mode == Mode::TEXT) emitText("TXA"); else emitByte(0x8A); }
void M65Emitter::tay() { if (mode == Mode::TEXT) emitText("TAY"); else emitByte(0xA8); }
void M65Emitter::tya() { if (mode == Mode::TEXT) emitText("TYA"); else emitByte(0x98); }
void M65Emitter::taz() { if (mode == Mode::TEXT) emitText("TAZ"); else emitByte(0x4B); }
void M65Emitter::tza() { if (mode == Mode::TEXT) emitText("TZA"); else emitByte(0x6B); }
void M65Emitter::tsx() { if (mode == Mode::TEXT) emitText("TSX"); else emitByte(0xBA); }
void M65Emitter::inx() { if (mode == Mode::TEXT) emitText("INX"); else emitByte(0xE8); }
void M65Emitter::dex() { if (mode == Mode::TEXT) emitText("DEX"); else emitByte(0xCA); }
void M65Emitter::iny() { if (mode == Mode::TEXT) emitText("INY"); else emitByte(0xC8); }
void M65Emitter::dey() { if (mode == Mode::TEXT) emitText("DEY"); else emitByte(0x88); }
void M65Emitter::inz() { if (mode == Mode::TEXT) emitText("INZ"); else emitByte(0x1B); }
void M65Emitter::dez() { if (mode == Mode::TEXT) emitText("DEZ"); else emitByte(0x3B); }

// --- Stack Operations ---
void M65Emitter::pha() { if (mode == Mode::TEXT) emitText("PHA"); else emitByte(0x48); }
void M65Emitter::pla() { if (mode == Mode::TEXT) emitText("PLA"); else emitByte(0x68); }
void M65Emitter::phx() { if (mode == Mode::TEXT) emitText("PHX"); else emitByte(0xDA); }
void M65Emitter::plx() { if (mode == Mode::TEXT) emitText("PLX"); else emitByte(0xFA); }
void M65Emitter::phy() { if (mode == Mode::TEXT) emitText("PHY"); else emitByte(0x5A); }
void M65Emitter::ply() { if (mode == Mode::TEXT) emitText("PLY"); else emitByte(0x7A); }
void M65Emitter::phz() { if (mode == Mode::TEXT) emitText("PHZ"); else emitByte(0xDB); }
void M65Emitter::plz() { if (mode == Mode::TEXT) emitText("PLZ"); else emitByte(0xFB); }

// --- ALU & Branching ---
void M65Emitter::clc() { if (mode == Mode::TEXT) emitText("CLC"); else emitByte(0x18); }
void M65Emitter::sec() { if (mode == Mode::TEXT) emitText("SEC"); else emitByte(0x38); }
void M65Emitter::neg_a() { if (mode == Mode::TEXT) emitText("NEG", "A"); else emitByte(0x42); }
void M65Emitter::asl_a() { if (mode == Mode::TEXT) emitText("ASL", "A"); else emitByte(0x0A); }
void M65Emitter::rol_a() { if (mode == Mode::TEXT) emitText("ROL", "A"); else emitByte(0x2A); }
void M65Emitter::lsr_a() { if (mode == Mode::TEXT) emitText("LSR", "A"); else emitByte(0x4A); }
void M65Emitter::ror_a() { if (mode == Mode::TEXT) emitText("ROR", "A"); else emitByte(0x6A); }
void M65Emitter::inc_a() { if (mode == Mode::TEXT) emitText("INC", "A"); else emitByte(0x1A); }
void M65Emitter::dec_a() { if (mode == Mode::TEXT) emitText("DEC", "A"); else emitByte(0x3A); }
void M65Emitter::eom() { if (mode == Mode::TEXT) emitText("EOM"); else emitByte(0xEA); }

void M65Emitter::bra(int8_t offset) { if (mode == Mode::TEXT) emitText("BRA", "*+" + std::to_string((int)offset)); else { emitByte(0x80); emitByte((uint8_t)offset); } }
void M65Emitter::bne(int8_t offset) { if (mode == Mode::TEXT) emitText("BNE", "*+" + std::to_string((int)offset)); else { emitByte(0xD0); emitByte((uint8_t)offset); } }
void M65Emitter::beq(int8_t offset) { if (mode == Mode::TEXT) emitText("BEQ", "*+" + std::to_string((int)offset)); else { emitByte(0xF0); emitByte((uint8_t)offset); } }
void M65Emitter::bcc(int8_t offset) { if (mode == Mode::TEXT) emitText("BCC", "*+" + std::to_string((int)offset)); else { emitByte(0x90); emitByte((uint8_t)offset); } }
void M65Emitter::bcs(int8_t offset) { if (mode == Mode::TEXT) emitText("BCS", "*+" + std::to_string((int)offset)); else { emitByte(0xB0); emitByte((uint8_t)offset); } }

// --- Semantic Helpers ---
void M65Emitter::add_16_imm(uint16_t val) {
    if (val == 0) return;
    if (val == 1) {
        inc_a();
        if (mode == Mode::TEXT) emitText("BNE", "*+3"); else { emitByte(0xD0); emitByte(0x01); }
        inx();
    } else if (val < 256) {
        clc();
        adc_imm(val & 0xFF);
        if (mode == Mode::TEXT) emitText("BCC", "*+3"); else { emitByte(0x90); emitByte(0x01); }
        inx();
    } else {
        clc();
        adc_imm(val & 0xFF);
        pha();
        txa();
        adc_imm(val >> 8);
        tax();
        pla();
    }
}
void M65Emitter::sub_16_imm(uint16_t val) {
    if (val == 0) return;
    if (val == 1) {
        if (mode == Mode::TEXT) emitText("BNE", "*+3"); else { emitByte(0xD0); emitByte(0x01); }
        dex();
        dec_a();
    } else if (val < 256) {
        sec();
        sbc_imm(val & 0xFF);
        if (mode == Mode::TEXT) emitText("BCS", "*+3"); else { emitByte(0xB0); emitByte(0x01); }
        dex();
    } else {
        sec();
        sbc_imm(val & 0xFF);
        pha();
        txa();
        sbc_imm(val >> 8);
        tax();
        pla();
    }
}
void M65Emitter::neg_16() { neg_a(); pha(); txa(); eor_imm(0xFF); adc_imm(0); tax(); pla(); }
void M65Emitter::not_16() { eor_imm(0xFF); pha(); txa(); eor_imm(0xFF); tax(); pla(); }
void M65Emitter::push_ax() { phx(); pha(); }
void M65Emitter::pop_ax() { pla(); plx(); }
void M65Emitter::transfer_ax_to_zp(uint8_t addr) { sta_zp(addr); stx_zp(addr + 1); }
