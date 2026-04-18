#pragma once
#include <vector>
#include <string>
#include <ostream>
#include <cstdint>

class M65Emitter {
public:
    enum class Mode { TEXT, BINARY };

    M65Emitter(std::ostream& out, uint32_t zpStart = 0x02);
    M65Emitter(std::vector<uint8_t>& binary, uint32_t zpStart = 0x02);

    uint8_t getZP(int offset) const { return (uint8_t)(zeroPageStart + offset); }

    // --- Immediate Mode ---
    void lda_imm(uint8_t val);
    void ldx_imm(uint8_t val);
    void ldy_imm(uint8_t val);
    void ldz_imm(uint8_t val);
    void phw_imm(uint16_t val);
    void adc_imm(uint8_t val);
    void sbc_imm(uint8_t val);
    void and_imm(uint8_t val);
    void ora_imm(uint8_t val);
    void eor_imm(uint8_t val);

    // --- Absolute Mode ---
    void lda_abs(uint16_t addr);
    void ldx_abs(uint16_t addr);
    void ldy_abs(uint16_t addr);
    void ldz_abs(uint16_t addr);
    void sta_abs(uint16_t addr);
    void stx_abs(uint16_t addr);
    void sty_abs(uint16_t addr);
    void stz_abs(uint16_t addr);
    void adc_abs(uint16_t addr);
    void sbc_abs(uint16_t addr);

    // --- Zero Page Mode ---
    void lda_zp(uint8_t addr);
    void sta_zp(uint8_t addr);
    void stx_zp(uint8_t addr);
    void stz_zp(uint8_t addr);
    void adc_zp(uint8_t addr);
    void sbc_zp(uint8_t addr);
    void and_zp(uint8_t addr);
    void ora_zp(uint8_t addr);
    void eor_zp(uint8_t addr);
    void inc_zp(uint8_t addr);
    void dec_zp(uint8_t addr);
    void bit_zp(uint8_t addr);

    // --- Scratchpad (Relative to zeroPageStart) ---
    void lda_s(uint8_t index) { lda_zp(getZP(index)); }
    void sta_s(uint8_t index) { sta_zp(getZP(index)); }
    void stx_s(uint8_t index) { stx_zp(getZP(index)); }
    void stz_s(uint8_t index) { stz_zp(getZP(index)); }
    void lda_ind_zs(uint8_t index, bool flat = false) { lda_ind_z(getZP(index), flat); }
    void sta_ind_zs(uint8_t index, bool flat = false) { sta_ind_z(getZP(index), flat); }
    void adc_s(uint8_t index) { adc_zp(getZP(index)); }
    void sbc_s(uint8_t index) { sbc_zp(getZP(index)); }
    void and_s(uint8_t index) { and_zp(getZP(index)); }
    void ora_s(uint8_t index) { ora_zp(getZP(index)); }
    void eor_s(uint8_t index) { eor_zp(getZP(index)); }
    void inc_s(uint8_t index) { inc_zp(getZP(index)); }
    void dec_s(uint8_t index) { dec_zp(getZP(index)); }
    void bit_s(uint8_t index) { bit_zp(getZP(index)); }

    // --- Other Addressing Modes ---
    void lda_stack(uint8_t offset);
    void sta_stack(uint8_t offset);
    void lda_ind_z(uint8_t addr, bool flat = false);
    void sta_ind_z(uint8_t addr, bool flat = false);
    void bit_abs(uint16_t addr);

    // --- Register Transfers ---
    void tax();
    void txa();
    void tay();
    void tya();
    void taz();
    void tza();
    void tsx();
    void inx();
    void dex();
    void iny();
    void dey();
    void inz();
    void dez();

    // --- Stack Operations ---
    void pha();
    void pla();
    void phx();
    void plx();
    void phy();
    void ply();
    void phz();
    void plz();

    // --- ALU & Branching ---
    void clc();
    void sec();
    void neg_a();
    void asl_a();
    void rol_a();
    void lsr_a();
    void ror_a();
    void inc_a();
    void dec_a();
    void eom();
    
    void bra(int8_t offset);
    void bne(int8_t offset);
    void beq(int8_t offset);
    void bcc(int8_t offset);
    void bcs(int8_t offset);

    // --- High-Level Semantic Helpers ---
    void add_16_imm(uint16_t val);
    void sub_16_imm(uint16_t val);
    void neg_16();
    void not_16();
    void push_ax();
    void pop_ax();
    void transfer_ax_to_zp(uint8_t addr);

private:
    Mode mode;
    std::ostream* out = nullptr;
    std::vector<uint8_t>* binary = nullptr;
    uint32_t zeroPageStart;

    void emitByte(uint8_t b);
    void emitWord(uint16_t w);
    void emitText(const std::string& mnemonic, const std::string& operand = "");
};
