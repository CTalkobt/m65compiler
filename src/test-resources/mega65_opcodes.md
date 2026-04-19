# 45GS02 Opcode Reference

Source: MEGA65 Book (mega65-book.pdf), Appendix L.

Addressing mode abbreviations used in the tables:
- `imp` — implied (no operand)
- `acc` — accumulator
- `imm` — immediate (`#nn`)
- `imm16` — 16-bit immediate (`#nnnn`)
- `bp` — base-page / zero-page (`nn`)
- `bp,X` — base-page indexed X (`nn,X`)
- `bp,Y` — base-page indexed Y (`nn,Y`)
- `abs` — absolute (`nnnn`)
- `abs,X` — absolute indexed X (`nnnn,X`)
- `abs,Y` — absolute indexed Y (`nnnn,Y`)
- `(bp,X)` — base-page pre-indexed indirect (`(nn,X)`)
- `(bp),Y` — base-page post-indexed indirect Y (`(nn),Y`)
- `(bp),Z` — base-page post-indexed indirect Z (`(nn),Z`)
- `[bp],Z` — base-page 32-bit flat post-indexed Z (`[nn],Z`) — EOM prefix
- `(bp,SP),Y` — stack-relative indirect indexed Y
- `(abs)` — absolute indirect
- `(abs,X)` — absolute indexed indirect
- `rel` — PC-relative 8-bit
- `relfar` — PC-relative 16-bit (word)
- `bp+rel8` — base-page address + 8-bit relative (bit-branch)

---

## Standard Opcode Table ($00–$FF)

| Byte | Mnemonic | Mode         | Operand bytes | Total bytes |
|------|----------|--------------|---------------|-------------|
| $00  | BRK      | imp          | 0             | 1           |
| $01  | ORA      | (bp,X)       | 1             | 2           |
| $02  | CLE      | imp          | 0             | 1           |
| $03  | SEE      | imp          | 0             | 1           |
| $04  | TSB      | bp           | 1             | 2           |
| $05  | ORA      | bp           | 1             | 2           |
| $06  | ASL      | bp           | 1             | 2           |
| $07  | RMB0     | bp           | 1             | 2           |
| $08  | PHP      | imp          | 0             | 1           |
| $09  | ORA      | imm          | 1             | 2           |
| $0A  | ASL      | acc          | 0             | 1           |
| $0B  | TSY      | imp          | 0             | 1           |
| $0C  | TSB      | abs          | 2             | 3           |
| $0D  | ORA      | abs          | 2             | 3           |
| $0E  | ASL      | abs          | 2             | 3           |
| $0F  | BBR0     | bp+rel8      | 2             | 3           |
| $10  | BPL      | rel          | 1             | 2           |
| $11  | ORA      | (bp),Y       | 1             | 2           |
| $12  | ORA      | (bp),Z       | 1             | 2           |
| $13  | BPL      | relfar       | 2             | 3           |
| $14  | TRB      | bp           | 1             | 2           |
| $15  | ORA      | bp,X         | 1             | 2           |
| $16  | ASL      | bp,X         | 1             | 2           |
| $17  | RMB1     | bp           | 1             | 2           |
| $18  | CLC      | imp          | 0             | 1           |
| $19  | ORA      | abs,Y        | 2             | 3           |
| $1A  | INC      | acc          | 0             | 1           |
| $1B  | INZ      | imp          | 0             | 1           |
| $1C  | TRB      | abs          | 2             | 3           |
| $1D  | ORA      | abs,X        | 2             | 3           |
| $1E  | ASL      | abs,X        | 2             | 3           |
| $1F  | BBR1     | bp+rel8      | 2             | 3           |
| $20  | JSR      | abs          | 2             | 3           |
| $21  | AND      | (bp,X)       | 1             | 2           |
| $22  | JSR      | (abs)        | 2             | 3           |
| $23  | JSR      | (abs,X)      | 2             | 3           |
| $24  | BIT      | bp           | 1             | 2           |
| $25  | AND      | bp           | 1             | 2           |
| $26  | ROL      | bp           | 1             | 2           |
| $27  | RMB2     | bp           | 1             | 2           |
| $28  | PLP      | imp          | 0             | 1           |
| $29  | AND      | imm          | 1             | 2           |
| $2A  | ROL      | acc          | 0             | 1           |
| $2B  | TYS      | imp          | 0             | 1           |
| $2C  | BIT      | abs          | 2             | 3           |
| $2D  | AND      | abs          | 2             | 3           |
| $2E  | ROL      | abs          | 2             | 3           |
| $2F  | BBR2     | bp+rel8      | 2             | 3           |
| $30  | BMI      | rel          | 1             | 2           |
| $31  | AND      | (bp),Y       | 1             | 2           |
| $32  | AND      | (bp),Z       | 1             | 2           |
| $33  | BMI      | relfar       | 2             | 3           |
| $34  | BIT      | bp,X         | 1             | 2           |
| $35  | AND      | bp,X         | 1             | 2           |
| $36  | ROL      | bp,X         | 1             | 2           |
| $37  | RMB3     | bp           | 1             | 2           |
| $38  | SEC      | imp          | 0             | 1           |
| $39  | AND      | abs,Y        | 2             | 3           |
| $3A  | DEC      | acc          | 0             | 1           |
| $3B  | DEZ      | imp          | 0             | 1           |
| $3C  | BIT      | abs,X        | 2             | 3           |
| $3D  | AND      | abs,X        | 2             | 3           |
| $3E  | ROL      | abs,X        | 2             | 3           |
| $3F  | BBR3     | bp+rel8      | 2             | 3           |
| $40  | RTI      | imp          | 0             | 1           |
| $41  | EOR      | (bp,X)       | 1             | 2           |
| $42  | NEG      | acc          | 0             | 1           |
| $43  | ASR      | acc          | 0             | 1           |
| $44  | ASR      | bp           | 1             | 2           |
| $45  | EOR      | bp           | 1             | 2           |
| $46  | LSR      | bp           | 1             | 2           |
| $47  | RMB4     | bp           | 1             | 2           |
| $48  | PHA      | imp          | 0             | 1           |
| $49  | EOR      | imm          | 1             | 2           |
| $4A  | LSR      | acc          | 0             | 1           |
| $4B  | TAZ      | imp          | 0             | 1           |
| $4C  | JMP      | abs          | 2             | 3           |
| $4D  | EOR      | abs          | 2             | 3           |
| $4E  | LSR      | abs          | 2             | 3           |
| $4F  | BBR4     | bp+rel8      | 2             | 3           |
| $50  | BVC      | rel          | 1             | 2           |
| $51  | EOR      | (bp),Y       | 1             | 2           |
| $52  | EOR      | (bp),Z       | 1             | 2           |
| $53  | BVC      | relfar       | 2             | 3           |
| $54  | ASR      | bp,X         | 1             | 2           |
| $55  | EOR      | bp,X         | 1             | 2           |
| $56  | LSR      | bp,X         | 1             | 2           |
| $57  | RMB5     | bp           | 1             | 2           |
| $58  | CLI      | imp          | 0             | 1           |
| $59  | EOR      | abs,Y        | 2             | 3           |
| $5A  | PHY      | imp          | 0             | 1           |
| $5B  | TAB      | imp          | 0             | 1           |
| $5C  | MAP      | imp          | 0             | 1           |
| $5D  | EOR      | abs,X        | 2             | 3           |
| $5E  | LSR      | abs,X        | 2             | 3           |
| $5F  | BBR5     | bp+rel8      | 2             | 3           |
| $60  | RTS      | imp          | 0             | 1           |
| $61  | ADC      | (bp,X)       | 1             | 2           |
| $62  | RTS      | imm          | 1             | 2           |
| $63  | BSR      | relfar       | 2             | 3           |
| $64  | STZ      | bp           | 1             | 2           |
| $65  | ADC      | bp           | 1             | 2           |
| $66  | ROR      | bp           | 1             | 2           |
| $67  | RMB6     | bp           | 1             | 2           |
| $68  | PLA      | imp          | 0             | 1           |
| $69  | ADC      | imm          | 1             | 2           |
| $6A  | ROR      | acc          | 0             | 1           |
| $6B  | TZA      | imp          | 0             | 1           |
| $6C  | JMP      | (abs)        | 2             | 3           |
| $6D  | ADC      | abs          | 2             | 3           |
| $6E  | ROR      | abs          | 2             | 3           |
| $6F  | BBR6     | bp+rel8      | 2             | 3           |
| $70  | BVS      | rel          | 1             | 2           |
| $71  | ADC      | (bp),Y       | 1             | 2           |
| $72  | ADC      | (bp),Z       | 1             | 2           |
| $73  | BVS      | relfar       | 2             | 3           |
| $74  | STZ      | bp,X         | 1             | 2           |
| $75  | ADC      | bp,X         | 1             | 2           |
| $76  | ROR      | bp,X         | 1             | 2           |
| $77  | RMB7     | bp           | 1             | 2           |
| $78  | SEI      | imp          | 0             | 1           |
| $79  | ADC      | abs,Y        | 2             | 3           |
| $7A  | PLY      | imp          | 0             | 1           |
| $7B  | TBA      | imp          | 0             | 1           |
| $7C  | JMP      | (abs,X)      | 2             | 3           |
| $7D  | ADC      | abs,X        | 2             | 3           |
| $7E  | ROR      | abs,X        | 2             | 3           |
| $7F  | BBR7     | bp+rel8      | 2             | 3           |
| $80  | BRA      | rel          | 1             | 2           |
| $81  | STA      | (bp,X)       | 1             | 2           |
| $82  | STA      | (bp,SP),Y    | 1             | 2           |
| $83  | BRA      | relfar       | 2             | 3           |
| $84  | STY      | bp           | 1             | 2           |
| $85  | STA      | bp           | 1             | 2           |
| $86  | STX      | bp           | 1             | 2           |
| $87  | SMB0     | bp           | 1             | 2           |
| $88  | DEY      | imp          | 0             | 1           |
| $89  | BIT      | imm          | 1             | 2           |
| $8A  | TXA      | imp          | 0             | 1           |
| $8B  | STY      | abs,X        | 2             | 3           |
| $8C  | STY      | abs          | 2             | 3           |
| $8D  | STA      | abs          | 2             | 3           |
| $8E  | STX      | abs          | 2             | 3           |
| $8F  | BBS0     | bp+rel8      | 2             | 3           |
| $90  | BCC      | rel          | 1             | 2           |
| $91  | STA      | (bp),Y       | 1             | 2           |
| $92  | STA      | (bp),Z       | 1             | 2           |
| $93  | BCC      | relfar       | 2             | 3           |
| $94  | STY      | bp,X         | 1             | 2           |
| $95  | STA      | bp,X         | 1             | 2           |
| $96  | STX      | bp,Y         | 1             | 2           |
| $97  | SMB1     | bp           | 1             | 2           |
| $98  | TYA      | imp          | 0             | 1           |
| $99  | STA      | abs,Y        | 2             | 3           |
| $9A  | TXS      | imp          | 0             | 1           |
| $9B  | STX      | abs,Y        | 2             | 3           |
| $9C  | STZ      | abs          | 2             | 3           |
| $9D  | STA      | abs,X        | 2             | 3           |
| $9E  | STZ      | abs,X        | 2             | 3           |
| $9F  | BBS1     | bp+rel8      | 2             | 3           |
| $A0  | LDY      | imm          | 1             | 2           |
| $A1  | LDA      | (bp,X)       | 1             | 2           |
| $A2  | LDX      | imm          | 1             | 2           |
| $A3  | LDZ      | imm          | 1             | 2           |
| $A4  | LDY      | bp           | 1             | 2           |
| $A5  | LDA      | bp           | 1             | 2           |
| $A6  | LDX      | bp           | 1             | 2           |
| $A7  | SMB2     | bp           | 1             | 2           |
| $A8  | TAY      | imp          | 0             | 1           |
| $A9  | LDA      | imm          | 1             | 2           |
| $AA  | TAX      | imp          | 0             | 1           |
| $AB  | LDZ      | abs          | 2             | 3           |
| $AC  | LDY      | abs          | 2             | 3           |
| $AD  | LDA      | abs          | 2             | 3           |
| $AE  | LDX      | abs          | 2             | 3           |
| $AF  | BBS2     | bp+rel8      | 2             | 3           |
| $B0  | BCS      | rel          | 1             | 2           |
| $B1  | LDA      | (bp),Y       | 1             | 2           |
| $B2  | LDA      | (bp),Z       | 1             | 2           |
| $B3  | BCS      | relfar       | 2             | 3           |
| $B4  | LDY      | bp,X         | 1             | 2           |
| $B5  | LDA      | bp,X         | 1             | 2           |
| $B6  | LDX      | bp,Y         | 1             | 2           |
| $B7  | SMB3     | bp           | 1             | 2           |
| $B8  | CLV      | imp          | 0             | 1           |
| $B9  | LDA      | abs,Y        | 2             | 3           |
| $BA  | TSX      | imp          | 0             | 1           |
| $BB  | LDZ      | abs,X        | 2             | 3           |
| $BC  | LDY      | abs,X        | 2             | 3           |
| $BD  | LDA      | abs,X        | 2             | 3           |
| $BE  | LDX      | abs,Y        | 2             | 3           |
| $BF  | BBS3     | bp+rel8      | 2             | 3           |
| $C0  | CPY      | imm          | 1             | 2           |
| $C1  | CMP      | (bp,X)       | 1             | 2           |
| $C2  | CPZ      | imm          | 1             | 2           |
| $C3  | DEW      | bp           | 1             | 2           |
| $C4  | CPY      | bp           | 1             | 2           |
| $C5  | CMP      | bp           | 1             | 2           |
| $C6  | DEC      | bp           | 1             | 2           |
| $C7  | SMB4     | bp           | 1             | 2           |
| $C8  | INY      | imp          | 0             | 1           |
| $C9  | CMP      | imm          | 1             | 2           |
| $CA  | DEX      | imp          | 0             | 1           |
| $CB  | ASW      | abs          | 2             | 3           |
| $CC  | CPY      | abs          | 2             | 3           |
| $CD  | CMP      | abs          | 2             | 3           |
| $CE  | DEC      | abs          | 2             | 3           |
| $CF  | BBS4     | bp+rel8      | 2             | 3           |
| $D0  | BNE      | rel          | 1             | 2           |
| $D1  | CMP      | (bp),Y       | 1             | 2           |
| $D2  | CMP      | (bp),Z       | 1             | 2           |
| $D3  | BNE      | relfar       | 2             | 3           |
| $D4  | CPZ      | bp           | 1             | 2           |
| $D5  | CMP      | bp,X         | 1             | 2           |
| $D6  | DEC      | bp,X         | 1             | 2           |
| $D7  | SMB5     | bp           | 1             | 2           |
| $D8  | CLD      | imp          | 0             | 1           |
| $D9  | CMP      | abs,Y        | 2             | 3           |
| $DA  | PHX      | imp          | 0             | 1           |
| $DB  | PHZ      | imp          | 0             | 1           |
| $DC  | CPZ      | abs          | 2             | 3           |
| $DD  | CMP      | abs,X        | 2             | 3           |
| $DE  | DEC      | abs,X        | 2             | 3           |
| $DF  | BBS5     | bp+rel8      | 2             | 3           |
| $E0  | CPX      | imm          | 1             | 2           |
| $E1  | SBC      | (bp,X)       | 1             | 2           |
| $E2  | LDA      | (bp,SP),Y    | 1             | 2           |
| $E3  | INW      | bp           | 1             | 2           |
| $E4  | CPX      | bp           | 1             | 2           |
| $E5  | SBC      | bp           | 1             | 2           |
| $E6  | INC      | bp           | 1             | 2           |
| $E7  | SMB6     | bp           | 1             | 2           |
| $E8  | INX      | imp          | 0             | 1           |
| $E9  | SBC      | imm          | 1             | 2           |
| $EA  | EOM      | imp          | 0             | 1           |
| $EB  | ROW      | abs          | 2             | 3           |
| $EC  | CPX      | abs          | 2             | 3           |
| $ED  | SBC      | abs          | 2             | 3           |
| $EE  | INC      | abs          | 2             | 3           |
| $EF  | BBS6     | bp+rel8      | 2             | 3           |
| $F0  | BEQ      | rel          | 1             | 2           |
| $F1  | SBC      | (bp),Y       | 1             | 2           |
| $F2  | SBC      | (bp),Z       | 1             | 2           |
| $F3  | BEQ      | relfar       | 2             | 3           |
| $F4  | PHW      | imm16        | 2             | 3           |
| $F5  | SBC      | bp,X         | 1             | 2           |
| $F6  | INC      | bp,X         | 1             | 2           |
| $F7  | SMB7     | bp           | 1             | 2           |
| $F8  | SED      | imp          | 0             | 1           |
| $F9  | SBC      | abs,Y        | 2             | 3           |
| $FA  | PLX      | imp          | 0             | 1           |
| $FB  | PLZ      | imp          | 0             | 1           |
| $FC  | PHW      | abs          | 2             | 3           |
| $FD  | SBC      | abs,X        | 2             | 3           |
| $FE  | INC      | abs,X        | 2             | 3           |
| $FF  | BBS7     | bp+rel8      | 2             | 3           |

---

## EOM-Prefixed Instructions ($EA prefix — 32-bit flat pointer)

`$EA` (`EOM`) as a prefix activates 32-bit base-page pointer reads for the following instruction.
Assembly syntax uses `[nn],Z` (square brackets) instead of `(nn),Z`.

| Encoding      | Mnemonic | Mode       | Total bytes |
|---------------|----------|------------|-------------|
| EA 12 nn      | ORA      | [bp],Z     | 3           |
| EA 32 nn      | AND      | [bp],Z     | 3           |
| EA 52 nn      | EOR      | [bp],Z     | 3           |
| EA 72 nn      | ADC      | [bp],Z     | 3           |
| EA 92 nn      | STA      | [bp],Z     | 3           |
| EA B2 nn      | LDA      | [bp],Z     | 3           |
| EA D2 nn      | CMP      | [bp],Z     | 3           |
| EA F2 nn      | SBC      | [bp],Z     | 3           |

---

## Quad (32-bit Q-register) Instructions ($42 $42 prefix)

The double-NEG prefix `$42 $42` activates 32-bit Q-register mode (Q = {A, X, Y, Z}).
`[indirect]` variants additionally require a `$EA` byte between the prefix and the base opcode.

### ADCQ — Add with Carry, Quad

| Encoding            | Mode         | Total bytes |
|---------------------|--------------|-------------|
| 42 42 65 nn         | bp           | 4           |
| 42 42 6D nn nn      | abs          | 5           |
| 42 42 72 nn         | (bp),Z       | 4           |
| 42 42 EA 72 nn      | [bp],Z       | 5           |

### ANDQ — Logical AND, Quad

| Encoding            | Mode         | Total bytes |
|---------------------|--------------|-------------|
| 42 42 25 nn         | bp           | 4           |
| 42 42 2D nn nn      | abs          | 5           |
| 42 42 32 nn         | (bp),Z       | 4           |
| 42 42 EA 32 nn      | [bp],Z       | 5           |

### ASLQ — Arithmetic Shift Left, Quad

| Encoding            | Mode         | Total bytes |
|---------------------|--------------|-------------|
| 42 42 0A            | Q (acc)      | 3           |
| 42 42 06 nn         | bp           | 4           |
| 42 42 16 nn         | bp,X         | 4           |
| 42 42 0E nn nn      | abs          | 5           |
| 42 42 1E nn nn      | abs,X        | 5           |

### ASRQ — Arithmetic Shift Right, Quad

| Encoding            | Mode         | Total bytes |
|---------------------|--------------|-------------|
| 42 42 43            | Q (acc)      | 3           |
| 42 42 44 nn         | bp           | 4           |
| 42 42 54 nn         | bp,X         | 4           |

### BITQ — Bit Test, Quad

| Encoding            | Mode         | Total bytes |
|---------------------|--------------|-------------|
| 42 42 24 nn         | bp           | 4           |
| 42 42 2C nn nn      | abs          | 5           |

### CMPQ — Compare, Quad

| Encoding            | Mode         | Total bytes |
|---------------------|--------------|-------------|
| 42 42 C5 nn         | bp           | 4           |
| 42 42 CD nn nn      | abs          | 5           |
| 42 42 D2 nn         | (bp),Z       | 4           |
| 42 42 EA D2 nn      | [bp],Z       | 5           |

### DEQ — Decrement, Quad

| Encoding            | Mode         | Total bytes |
|---------------------|--------------|-------------|
| 42 42 3A            | Q (acc)      | 3           |
| 42 42 C6 nn         | bp           | 4           |
| 42 42 D6 nn         | bp,X         | 4           |
| 42 42 CE nn nn      | abs          | 5           |
| 42 42 DE nn nn      | abs,X        | 5           |

### EORQ — Exclusive OR, Quad

| Encoding            | Mode         | Total bytes |
|---------------------|--------------|-------------|
| 42 42 45 nn         | bp           | 4           |
| 42 42 4D nn nn      | abs          | 5           |
| 42 42 52 nn         | (bp),Z       | 4           |
| 42 42 EA 52 nn      | [bp],Z       | 5           |

### INQ — Increment, Quad

| Encoding            | Mode         | Total bytes |
|---------------------|--------------|-------------|
| 42 42 1A            | Q (acc)      | 3           |
| 42 42 E6 nn         | bp           | 4           |
| 42 42 F6 nn         | bp,X         | 4           |
| 42 42 EE nn nn      | abs          | 5           |
| 42 42 FE nn nn      | abs,X        | 5           |

### LDQ — Load Q, Quad

| Encoding            | Mode         | Total bytes |
|---------------------|--------------|-------------|
| 42 42 A5 nn         | bp           | 4           |
| 42 42 AD nn nn      | abs          | 5           |
| 42 42 B2 nn         | (bp),Z       | 4           |
| 42 42 EA B2 nn      | [bp],Z       | 5           |

### LSRQ — Logical Shift Right, Quad

| Encoding            | Mode         | Total bytes |
|---------------------|--------------|-------------|
| 42 42 4A            | Q (acc)      | 3           |
| 42 42 46 nn         | bp           | 4           |
| 42 42 56 nn         | bp,X         | 4           |
| 42 42 4E nn nn      | abs          | 5           |
| 42 42 5E nn nn      | abs,X        | 5           |

### ORQ — Logical OR, Quad

| Encoding            | Mode         | Total bytes |
|---------------------|--------------|-------------|
| 42 42 05 nn         | bp           | 4           |
| 42 42 0D nn nn      | abs          | 5           |
| 42 42 12 nn         | (bp),Z       | 4           |
| 42 42 EA 12 nn      | [bp],Z       | 5           |

### ROLQ — Rotate Left, Quad

| Encoding            | Mode         | Total bytes |
|---------------------|--------------|-------------|
| 42 42 2A            | Q (acc)      | 3           |
| 42 42 26 nn         | bp           | 4           |
| 42 42 36 nn         | bp,X         | 4           |
| 42 42 2E nn nn      | abs          | 5           |
| 42 42 3E nn nn      | abs,X        | 5           |

### RORQ — Rotate Right, Quad

| Encoding            | Mode         | Total bytes |
|---------------------|--------------|-------------|
| 42 42 6A            | Q (acc)      | 3           |
| 42 42 66 nn         | bp           | 4           |
| 42 42 76 nn         | bp,X         | 4           |
| 42 42 6E nn nn      | abs          | 5           |
| 42 42 7E nn nn      | abs,X        | 5           |

### SBCQ — Subtract with Carry, Quad

| Encoding            | Mode         | Total bytes |
|---------------------|--------------|-------------|
| 42 42 E5 nn         | bp           | 4           |
| 42 42 ED nn nn      | abs          | 5           |
| 42 42 F2 nn         | (bp),Z       | 4           |
| 42 42 EA F2 nn      | [bp],Z       | 5           |

### STQ — Store Q, Quad

| Encoding            | Mode         | Total bytes |
|---------------------|--------------|-------------|
| 42 42 85 nn         | bp           | 4           |
| 42 42 8D nn nn      | abs          | 5           |
| 42 42 92 nn         | (bp),Z       | 4           |
| 42 42 EA 92 nn      | [bp],Z       | 5           |

---

## Notes

- **Q register**: A 32-bit pseudo-register composed of `{A, X, Y, Z}` (A = bits 0–7, X = bits 8–15, Y = bits 16–23, Z = bits 24–31).
- **Base-page (bp)**: Equivalent to zero-page on 6502. The B register shifts the base-page window: effective address = `(B << 8) | nn`.
- **EOM ($EA)**: As a prefix it signals that the following `(bp),Z` instruction should use a 32-bit flat pointer from base-page.
- **BBR/BBS**: Branch on Bit Reset/Set. Operand is two bytes: `nn` (base-page address) + `rel` (signed 8-bit branch offset).
- **RMB/SMB**: Reset/Set Memory Bit. Single base-page operand; bit number encoded in the opcode high nibble.
- **BSR** ($63): Branch to Subroutine — pushes return address and branches via a signed 16-bit relative offset.
- **RTS imm** ($62): Returns from subroutine then adds an immediate byte to the return address (useful for parameter passing).
- **PHW** ($F4/$FC): Pushes a 16-bit value (immediate or from memory) onto the stack in two bytes, low byte first.
- **ASW/ROW** ($CB/$EB): 16-bit arithmetic/rotate operations on a word in memory.
- **DEW/INW** ($C3/$E3): 16-bit decrement/increment of a word in base-page memory.
