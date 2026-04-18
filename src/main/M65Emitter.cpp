#include "M65Emitter.hpp"
#include <iomanip>
#include <sstream>

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

void M65Emitter::emitByte(uint8_t b) { binary->push_back(b); }
void M65Emitter::emitWord(uint16_t w) { binary->push_back(w & 0xFF); binary->push_back(w >> 8); }
void M65Emitter::emitText(const std::string& mnemonic, const std::string& operand) {
    *out << "    " << std::left << std::setw(8) << mnemonic << operand << std::endl;
}

void M65Emitter::lda_imm(uint8_t val) { if (mode == Mode::TEXT) emitText("lda", "#" + hex8(val)); else { emitByte(0xA9); emitByte(val); } }
void M65Emitter::ldx_imm(uint8_t val) { if (mode == Mode::TEXT) emitText("ldx", "#" + hex8(val)); else { emitByte(0xA2); emitByte(val); } }
void M65Emitter::ldy_imm(uint8_t val) { if (mode == Mode::TEXT) emitText("ldy", "#" + hex8(val)); else { emitByte(0xA0); emitByte(val); } }
void M65Emitter::ldz_imm(uint8_t val) { if (mode == Mode::TEXT) emitText("ldz", "#" + hex8(val)); else { emitByte(0xA3); emitByte(val); } }
void M65Emitter::phw_imm(uint16_t val) { if (mode == Mode::TEXT) emitText("phw", "#" + hex16(val)); else { emitByte(0xF4); emitWord(val); } }
void M65Emitter::adc_imm(uint8_t val) { if (mode == Mode::TEXT) emitText("adc", "#" + hex8(val)); else { emitByte(0x69); emitByte(val); } }
void M65Emitter::sbc_imm(uint8_t val) { if (mode == Mode::TEXT) emitText("sbc", "#" + hex8(val)); else { emitByte(0xE9); emitByte(val); } }
void M65Emitter::and_imm(uint8_t val) { if (mode == Mode::TEXT) emitText("and", "#" + hex8(val)); else { emitByte(0x29); emitByte(val); } }
void M65Emitter::ora_imm(uint8_t val) { if (mode == Mode::TEXT) emitText("ora", "#" + hex8(val)); else { emitByte(0x09); emitByte(val); } }
void M65Emitter::eor_imm(uint8_t val) { if (mode == Mode::TEXT) emitText("eor", "#" + hex8(val)); else { emitByte(0x49); emitByte(val); } }
void M65Emitter::cmp_imm(uint8_t val) { if (mode == Mode::TEXT) emitText("cmp", "#" + hex8(val)); else { emitByte(0xC9); emitByte(val); } }

// --- Absolute Mode ---
void M65Emitter::lda_abs(uint16_t addr) { if (mode == Mode::TEXT) emitText("lda", hex16(addr)); else { emitByte(0xAD); emitWord(addr); } }
void M65Emitter::ldx_abs(uint16_t addr) { if (mode == Mode::TEXT) emitText("ldx", hex16(addr)); else { emitByte(0xAE); emitWord(addr); } }
void M65Emitter::ldy_abs(uint16_t addr) { if (mode == Mode::TEXT) emitText("ldy", hex16(addr)); else { emitByte(0xAC); emitWord(addr); } }
void M65Emitter::ldz_abs(uint16_t addr) { if (mode == Mode::TEXT) emitText("ldz", hex16(addr)); else { emitByte(0xAB); emitWord(addr); } }
void M65Emitter::sta_abs(uint16_t addr) { if (mode == Mode::TEXT) emitText("sta", hex16(addr)); else { emitByte(0x8D); emitWord(addr); } }
void M65Emitter::stx_abs(uint16_t addr) { if (mode == Mode::TEXT) emitText("stx", hex16(addr)); else { emitByte(0x8E); emitWord(addr); } }
void M65Emitter::sty_abs(uint16_t addr) { if (mode == Mode::TEXT) emitText("sty", hex16(addr)); else { emitByte(0x8C); emitWord(addr); } }
void M65Emitter::stz_abs(uint16_t addr) { if (mode == Mode::TEXT) emitText("stz", hex16(addr)); else { emitByte(0x9C); emitWord(addr); } }
void M65Emitter::lda_abs_x(uint16_t addr) { if (mode == Mode::TEXT) emitText("lda", hex16(addr) + ",x"); else { emitByte(0xBD); emitWord(addr); } }
void M65Emitter::sta_abs_x(uint16_t addr) { if (mode == Mode::TEXT) emitText("sta", hex16(addr) + ",x"); else { emitByte(0x9D); emitWord(addr); } }
void M65Emitter::stz_abs_x(uint16_t addr) { if (mode == Mode::TEXT) emitText("stz", hex16(addr) + ",x"); else { emitByte(0x9E); emitWord(addr); } }
void M65Emitter::adc_abs(uint16_t addr) { if (mode == Mode::TEXT) emitText("adc", hex16(addr)); else { emitByte(0x6D); emitWord(addr); } }
void M65Emitter::sbc_abs(uint16_t addr) { if (mode == Mode::TEXT) emitText("sbc", hex16(addr)); else { emitByte(0xED); emitWord(addr); } }
void M65Emitter::and_abs(uint16_t addr) { if (mode == Mode::TEXT) emitText("and", hex16(addr)); else { emitByte(0x2D); emitWord(addr); } }
void M65Emitter::ora_abs(uint16_t addr) { if (mode == Mode::TEXT) emitText("ora", hex16(addr)); else { emitByte(0x0D); emitWord(addr); } }
void M65Emitter::eor_abs(uint16_t addr) { if (mode == Mode::TEXT) emitText("eor", hex16(addr)); else { emitByte(0x4D); emitWord(addr); } }
void M65Emitter::cmp_abs(uint16_t addr) { if (mode == Mode::TEXT) emitText("cmp", hex16(addr)); else { emitByte(0xCD); emitWord(addr); } }

// --- Zero Page Mode ---
void M65Emitter::lda_zp(uint8_t addr) { if (mode == Mode::TEXT) emitText("lda", hex8(addr)); else { emitByte(0xA5); emitByte(addr); } }
void M65Emitter::sta_zp(uint8_t addr) { if (mode == Mode::TEXT) emitText("sta", hex8(addr)); else { emitByte(0x85); emitByte(addr); } }
void M65Emitter::stx_zp(uint8_t addr) { if (mode == Mode::TEXT) emitText("stx", hex8(addr)); else { emitByte(0x86); emitByte(addr); } }
void M65Emitter::stz_zp(uint8_t addr) { if (mode == Mode::TEXT) emitText("stz", hex8(addr)); else { emitByte(0x64); emitByte(addr); } }
void M65Emitter::adc_zp(uint8_t addr) { if (mode == Mode::TEXT) emitText("adc", hex8(addr)); else { emitByte(0x65); emitByte(addr); } }
void M65Emitter::sbc_zp(uint8_t addr) { if (mode == Mode::TEXT) emitText("sbc", hex8(addr)); else { emitByte(0xE5); emitByte(addr); } }
void M65Emitter::and_zp(uint8_t addr) { if (mode == Mode::TEXT) emitText("and", hex8(addr)); else { emitByte(0x25); emitByte(addr); } }
void M65Emitter::ora_zp(uint8_t addr) { if (mode == Mode::TEXT) emitText("ora", hex8(addr)); else { emitByte(0x05); emitByte(addr); } }
void M65Emitter::eor_zp(uint8_t addr) { if (mode == Mode::TEXT) emitText("eor", hex8(addr)); else { emitByte(0x45); emitByte(addr); } }
void M65Emitter::inc_zp(uint8_t addr) { if (mode == Mode::TEXT) emitText("inc", hex8(addr)); else { emitByte(0xE6); emitByte(addr); } }
void M65Emitter::dec_zp(uint8_t addr) { if (mode == Mode::TEXT) emitText("dec", hex8(addr)); else { emitByte(0xC6); emitByte(addr); } }
void M65Emitter::inc_abs(uint16_t addr) { if (mode == Mode::TEXT) emitText("inc", hex16(addr)); else { emitByte(0xEE); emitWord(addr); } }
void M65Emitter::dec_abs(uint16_t addr) { if (mode == Mode::TEXT) emitText("dec", hex16(addr)); else { emitByte(0xCE); emitWord(addr); } }
void M65Emitter::bit_zp(uint8_t addr) { if (mode == Mode::TEXT) emitText("bit", hex8(addr)); else { emitByte(0x24); emitByte(addr); } }
void M65Emitter::cmp_zp(uint8_t addr) { if (mode == Mode::TEXT) emitText("cmp", hex8(addr)); else { emitByte(0xC5); emitByte(addr); } }
void M65Emitter::inc_abs_x(uint16_t addr) { if (mode == Mode::TEXT) emitText("inc", hex16(addr) + ",x"); else { emitByte(0xFE); emitWord(addr); } }
void M65Emitter::dec_abs_x(uint16_t addr) { if (mode == Mode::TEXT) emitText("dec", hex16(addr) + ",x"); else { emitByte(0xDE); emitWord(addr); } }

// --- Other Addressing Modes ---
void M65Emitter::lda_stack(uint8_t offset) { if (mode == Mode::TEXT) emitText("lda", std::to_string((int)offset) + ", s"); else { emitByte(0xE2); emitByte(offset); } }
void M65Emitter::sta_stack(uint8_t offset) { if (mode == Mode::TEXT) emitText("sta", std::to_string((int)offset) + ", s"); else { emitByte(0x82); emitByte(offset); } }
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

void M65Emitter::bit_abs(uint16_t addr) { if (mode == Mode::TEXT) emitText("bit", hex16(addr)); else { emitByte(0x2C); emitWord(addr); } }

// --- Register Transfers ---
void M65Emitter::tax() { if (mode == Mode::TEXT) emitText("tax"); else emitByte(0xAA); }
void M65Emitter::txa() { if (mode == Mode::TEXT) emitText("txa"); else emitByte(0x8A); }
void M65Emitter::tay() { if (mode == Mode::TEXT) emitText("tay"); else emitByte(0xA8); }
void M65Emitter::tya() { if (mode == Mode::TEXT) emitText("tya"); else emitByte(0x98); }
void M65Emitter::taz() { if (mode == Mode::TEXT) emitText("taz"); else emitByte(0x4B); }
void M65Emitter::tza() { if (mode == Mode::TEXT) emitText("tza"); else emitByte(0x6B); }
void M65Emitter::tsx() { if (mode == Mode::TEXT) emitText("tsx"); else emitByte(0xBA); }
void M65Emitter::inx() { if (mode == Mode::TEXT) emitText("inx"); else emitByte(0xE8); }
void M65Emitter::dex() { if (mode == Mode::TEXT) emitText("dex"); else emitByte(0xCA); }
void M65Emitter::iny() { if (mode == Mode::TEXT) emitText("iny"); else emitByte(0xC8); }
void M65Emitter::dey() { if (mode == Mode::TEXT) emitText("dey"); else emitByte(0x88); }
void M65Emitter::inz() { if (mode == Mode::TEXT) emitText("inz"); else emitByte(0x1B); }
void M65Emitter::dez() { if (mode == Mode::TEXT) emitText("dez"); else emitByte(0x3B); }

// --- Stack Operations ---
void M65Emitter::pha() { if (mode == Mode::TEXT) emitText("pha"); else emitByte(0x48); }
void M65Emitter::pla() { if (mode == Mode::TEXT) emitText("pla"); else emitByte(0x68); }
void M65Emitter::phx() { if (mode == Mode::TEXT) emitText("phx"); else emitByte(0xDA); }
void M65Emitter::plx() { if (mode == Mode::TEXT) emitText("plx"); else emitByte(0xFA); }
void M65Emitter::phy() { if (mode == Mode::TEXT) emitText("phy"); else emitByte(0x5A); }
void M65Emitter::ply() { if (mode == Mode::TEXT) emitText("ply"); else emitByte(0x7A); }
void M65Emitter::phz() { if (mode == Mode::TEXT) emitText("phz"); else emitByte(0xDB); }
void M65Emitter::plz() { if (mode == Mode::TEXT) emitText("plz"); else emitByte(0xFB); }

// --- ALU & Branching ---
void M65Emitter::clc() { if (mode == Mode::TEXT) emitText("clc"); else emitByte(0x18); }
void M65Emitter::sec() { if (mode == Mode::TEXT) emitText("sec"); else emitByte(0x38); }
void M65Emitter::cla() { lda_imm(0); }
void M65Emitter::clx() { ldx_imm(0); }
void M65Emitter::cly() { ldy_imm(0); }
void M65Emitter::clz() { ldz_imm(0); }
void M65Emitter::neg_a() { if (mode == Mode::TEXT) emitText("neg", "a"); else emitByte(0x42); }
void M65Emitter::asl_a() { if (mode == Mode::TEXT) emitText("asl", "a"); else emitByte(0x0A); }
void M65Emitter::rol_a() { if (mode == Mode::TEXT) emitText("rol", "a"); else emitByte(0x2A); }
void M65Emitter::lsr_a() { if (mode == Mode::TEXT) emitText("lsr", "a"); else emitByte(0x4A); }
void M65Emitter::ror_a() { if (mode == Mode::TEXT) emitText("ror", "a"); else emitByte(0x6A); }
void M65Emitter::inc_a() { if (mode == Mode::TEXT) emitText("inc", "a"); else emitByte(0x1A); }
void M65Emitter::dec_a() { if (mode == Mode::TEXT) emitText("dec", "a"); else emitByte(0x3A); }
void M65Emitter::eom() { if (mode == Mode::TEXT) emitText("eom"); else emitByte(0xEA); }

void M65Emitter::bra(int8_t offset) { if (mode == Mode::TEXT) emitText("bra", "*+" + std::to_string((int)offset)); else { emitByte(0x80); emitByte((uint8_t)offset); } }
void M65Emitter::bne(int8_t offset) { if (mode == Mode::TEXT) emitText("bne", "*+" + std::to_string((int)offset)); else { emitByte(0xD0); emitByte((uint8_t)offset); } }
void M65Emitter::beq(int8_t offset) { if (mode == Mode::TEXT) emitText("beq", "*+" + std::to_string((int)offset)); else { emitByte(0xF0); emitByte((uint8_t)offset); } }
void M65Emitter::bcc(int8_t offset) { if (mode == Mode::TEXT) emitText("bcc", "*+" + std::to_string((int)offset)); else { emitByte(0x90); emitByte((uint8_t)offset); } }
void M65Emitter::bcs(int8_t offset) { if (mode == Mode::TEXT) emitText("bcs", "*+" + std::to_string((int)offset)); else { emitByte(0xB0); emitByte((uint8_t)offset); } }

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
