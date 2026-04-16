#pragma once
#include <vector>
#include <string>
#include <ostream>
#include <cstdint>

class M65Emitter {
public:
    enum class Mode { TEXT, BINARY };

    M65Emitter(std::ostream& out);
    M65Emitter(std::vector<uint8_t>& binary);

    // --- Low-Level Opcodes ---
    void lda_imm(uint8_t val);
    void ldx_imm(uint8_t val);
    void ldy_imm(uint8_t val);
    void ldz_imm(uint8_t val);
    
    void lda_zp(uint8_t addr);
    void lda_abs(uint16_t addr);
    void ldx_abs(uint16_t addr);
    void lda_stack(uint8_t offset);
    void lda_ind_z(uint8_t addr, bool flat = false);

    void sta_zp(uint8_t addr);
    void stx_zp(uint8_t addr);
    void stz_zp(uint8_t addr);
    void sta_abs(uint16_t addr);
    void stx_abs(uint16_t addr);
    void sty_abs(uint16_t addr);
    void stz_abs(uint16_t addr);
    void sta_stack(uint8_t offset);

    void tax();
    void txa();
    void tay();
    void tya();
    void taz();
    void tza();
    void tsx();

    void pha();
    void pla();
    void phx();
    void plx();
    void phy();
    void ply();
    void phz();
    void plz();
    void phw_imm(uint16_t val);

    void clc();
    void sec();
    void neg_a();
    
    void adc_imm(uint8_t val);
    void adc_zp(uint8_t addr);
    void sbc_imm(uint8_t val);
    void sbc_zp(uint8_t addr);
    void sbc_abs(uint16_t addr);
    
    void and_imm(uint8_t val);
    void and_zp(uint8_t addr);
    void ora_imm(uint8_t val);
    void ora_zp(uint8_t addr);
    void eor_imm(uint8_t val);
    void eor_zp(uint8_t addr);

    void asl_a();
    void rol_a();
    void lsr_a();
    void ror_a();

    void inc_a();
    void dec_a();
    void inc_zp(uint8_t addr);
    void dec_zp(uint8_t addr);

    void bit_zp(uint8_t addr);
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

    void emitByte(uint8_t b);
    void emitWord(uint16_t w);
    void emitText(const std::string& mnemonic, const std::string& operand = "");
};
