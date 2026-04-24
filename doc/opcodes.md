# 45GS02 Instruction Reference

This reference summarizes the instructions available in the `ca45` assembler for the 45GS02. Instructions are grouped by functional categories, blending hardware-native opcodes with powerful macro-like instructions provided by the assembler.

---

## Addressing Modes Glossary

- **`imp`**: Implied (no operand required).
- **`acc`**: Accumulator / Register (e.g., `A`, `X`, `Q`).
- **`imm` / `imm16`**: 8-bit immediate (`#nn`) or 16-bit immediate (`#nnnn`).
- **`bp`**: Base-page (equivalent to 6502 Zero-Page, offset by `B` register).
- **`bp,X` / `bp,Y`**: Base-page indexed by X or Y.
- **`abs`**: Absolute 16-bit address.
- **`abs,X` / `abs,Y`**: Absolute indexed by X or Y.
- **`(bp,X)` / `(bp),Y` / `(bp),Z`**: Base-page indirect pre-indexed, or post-indexed.
- **`(abs)` / `(abs,X)`**: Absolute indirect / indexed indirect.
- **`(bp,SP),Y`**: Stack-relative indirect indexed Y.
- **`rel` / `relfar`**: PC-relative 8-bit / 16-bit (used for branches).
- **`[bp],Z`**: 32-bit flat memory pointer (triggered by hardware `EOM` prefix).
- **`.ax` / `.ay` / `.az` / `.xy`**: 16-bit register pairs (used by high-level instructions).
- **`.q` / `.axyz`**: 32-bit Quad register.
- **`offset, s`**: Stack-relative offset (e.g., `4, s` means SP + 4).

---

## Flag Legend
- **`N`**: Negative (bit 7/15/31 of result).
- **`Z`**: Zero (set if result is 0).
- **`C`**: Carry (set if overflow in unsigned or shift-out).
- **`V`**: Overflow (set if overflow in signed arithmetic).
- **`I`**: Interrupt Disable.
- **`D`**: Decimal Mode.

---

## 1. Memory Access & Data Movement

* **`LDA`**
  * *Description*: Loads an 8-bit value into the Accumulator.
  * *Modes*: `imm`, `bp`, `abs`, `bp,X/Y`, `abs,X/Y`, `(bp),Y/Z`, `[bp],Z`, `offset, s`
  * *Flags*: `N`, `Z`

* **`LDX`, `LDY`, `LDZ`**
  * *Description*: Loads an 8-bit value into the specified register.
  * *Modes*: `imm`, `bp`, `abs`, `bp,X/Y`, `abs,X/Y`, `(bp),Y/Z`, `[bp],Z`, `offset, s` (simulated)
  * *Flags*: `N`, `Z`

* **`STA`**
  * *Description*: Stores an 8-bit value from the Accumulator into memory.
  * *Modes*: `bp`, `abs`, `bp,X/Y`, `abs,X/Y`, `(bp),Y/Z`, `[bp],Z`, `offset, s`
  * *Flags*: None

* **`STX`, `STY`, `STZ`**
  * *Description*: Stores an 8-bit value from the specified register into memory.
  * *Modes*: `bp`, `abs`, `bp,X/Y`, `abs,X/Y`, `(bp),Y/Z`, `[bp],Z`, `offset, s` (simulated)
  * *Flags*: None

* **`LDAX`, `LDAY`, `LDAZ` (Simulated)**
  * *Description*: Loads a 16-bit word into A (low) and X/Y/Z (high).
  * *Modes*: `imm16` (`#val`), `abs`, `offset, s`
  * *Registers Affected*: `.A`, `.X/.Y/.Z`
  * *Flags*: `N`, `Z` (based on high byte)

* **`STAX`, `STAY`, `STAZ` (Simulated)**
  * *Description*: Stores a 16-bit word from A (low) and X/Y/Z (high) into memory.
  * *Modes*: `abs`, `offset, s`
  * *Registers Affected*: `.A` (if stack)
  * *Flags*: `N`, `Z` (if stack)

* **`LDW.SP`, `STW.SP` (Simulated)**
  * *Description*: Explicit stack-relative 16-bit load/store.
  * *Modes*: `<reg_pair>, offset`
  * *Registers Affected*: Pair registers, `.A` (temp)
  * *Flags*: `N`, `Z`

* **`LDW.F`, `STW.F` (Simulated)**
  * *Description*: 16-bit word load/store from 28-bit linear memory.
  * *Modes*: `<addr28>`
  * *Registers Affected*: `.AX`
  * *Flags*: `N`, `Z`

* **`LDQ`, `STQ`**
  * *Description*: Loads or stores the 32-bit Quad register (`A`, `X`, `Y`, `Z`).
  * *Modes*: `bp`, `abs`, `(bp),Z`, `[bp],Z`
  * *Flags*: `N`, `Z` (on Load)

* **`TAX`, `TAY`, `TAZ`, `TXA`, `TYA`, `TZA`, `TAB`, `TBA`, `TSX`, `TXS`, `TSY`, `TYS`**
  * *Description*: Register transfers.
  * *Modes*: `imp`
  * *Flags*: `N`, `Z` (except `TXS`, `TYS`)

* **`SWAP` (Simulated)**
  * *Description*: Swaps two 8-bit registers (`A`, `X`, `Y`, `Z`).
  * *Modes*: `<reg1>, <reg2>`
  * *Registers Affected*: `reg1`, `reg2`
  * *Flags*: None (preserved via stack)

* **`ZERO` (Simulated)**
  * *Description*: Clears registers to zero.
  * *Modes*: `<reg1>[, <reg2>...]`
  * *Flags*: `Z` (1), `N` (0)

* **`SXT.8` (Simulated)**
  * *Description*: Sign-extends `.A` to `.AX`.
  * *Modes*: `imp`
  * *Registers Affected*: `.X` (set to $00 or $FF)
  * *Flags*: `N`, `Z` (based on X)

* **`PTRDEREF` (Simulated)**
  * *Description*: De-references a 16-bit pointer in ZP into `.AX`.
  * *Modes*: `<zp_addr>`
  * *Registers Affected*: `.AX`, `.Y`
  * *Flags*: `N`, `Z`

---

## 2. Arithmetic Operations

* **`ADC`, `SBC`**
  * *Description*: 8-bit Add/Subtract with Carry.
  * *Modes*: `imm`, `bp`, `abs`, `bp,X`, `abs,X/Y`, `(bp),Y/Z`, `[bp],Z`
  * *Flags*: `N`, `Z`, `C`, `V`

* **`NEG`**
  * *Description*: 8-bit Two's Complement Negation of `.A`.
  * *Modes*: `acc`
  * *Flags*: `N`, `Z`, `C`

* **`ADD.16`, `SUB.16`, `ADD.S16`, `SUB.S16` (Simulated)**
  * *Description*: 16-bit unsigned or signed addition/subtraction.
  * *Modes*: `dest: .AX, .AY, .AZ, abs`; `src: #imm, abs, reg_pair`
  * *Registers Affected*: `dest` pair, `.A` (temp)
  * *Flags*: `N`, `Z`, `C`, `V`

* **`MUL.<width>`, `DIV.<width>` (Simulated)**
  * *Description*: Hardware-accelerated math.
  * *Widths*: `.8`, `.16`, `.24`, `.32`
  * *Modes*: `<dest>, <src>` (Registers or `abs` memory)
  * *Registers Affected*: `.A` (transfer), `dest` registers
  * *Flags*: `N`, `Z`

* **`INC`, `DEC`**
  * *Description*: 8-bit Increment/Decrement.
  * *Modes*: `acc`, `bp`, `bp,X`, `abs`, `abs,X`
  * *Flags*: `N`, `Z`

* **`INX`, `INY`, `INZ`, `DEX`, `DEY`, `DEZ`**
  * *Description*: 8-bit register increment/decrement.
  * *Modes*: `imp`
  * *Flags*: `N`, `Z`

* **`INW`, `DEW`**
  * *Description*: 16-bit word increment/decrement in Base-page.
  * *Modes*: `bp`
  * *Flags*: `Z` (Sets Z if the *entire* 16-bit result is 0)

* **`INQ`, `DEQ`**
  * *Description*: 32-bit Quad register increment/decrement.
  * *Modes*: `acc`
  * *Flags*: `N`, `Z`

* **`NEG.16`, `ABS.16`, `NEG.S16`, `ABS.S16` (Simulated)**
  * *Description*: 16-bit Negation / Absolute Value.
  * *Modes*: `.ax`, `abs`, `offset, s`
  * *Registers Affected*: `dest` pair, `.A`, `.X` (if stack)
  * *Flags*: `N`, `Z`, `C`

* **`EXPR` (Simulated)**
  * *Description*: Generates code for complex math formulas.
  * *Modes*: `<target>, <expression>`
  * *Flags*: `N`, `Z`, `C`, `V`

---

## 3. Logical Operations & Bit Manipulation

* **`AND`, `ORA`, `EOR`**
  * *Description*: 8-bit bitwise logic.
  * *Modes*: `imm`, `bp`, `abs`, `bp,X/Y`, `abs,X/Y`, `(bp),Y/Z`, `[bp],Z`
  * *Flags*: `N`, `Z`

* **`BIT`**
  * *Description*: Bit test (Mem AND Acc).
  * *Modes*: `imm`, `bp`, `bp,X`, `abs`, `abs,X`
  * *Flags*: `N` (bit 7), `V` (bit 6), `Z`

* **`AND.16`, `ORA.16`, `EOR.16`, `NOT.16` (Simulated)**
  * *Description*: 16-bit bitwise logic.
  * *Modes*: `.ax`, `abs` (NOT also supports `offset, s`)
  * *Registers Affected*: `.AX`, `.A` (temp)
  * *Flags*: `N`, `Z`

* **`SMB0-7`, `RMB0-7`**
  * *Description*: Set/Reset specific bit in memory.
  * *Modes*: `bp`
  * *Flags*: None

* **`TSB`, `TRB`**
  * *Description*: Test and Set/Reset bits via Accumulator mask.
  * *Modes*: `bp`, `abs`
  * *Flags*: `Z` (based on Mem AND Acc)

* **`SELECT` (Simulated)**
  * *Description*: Conditional assignment based on Zero flag.
  * *Modes*: `<reg>, <val_if_true>, <val_if_false>`
  * *Flags*: `N`, `Z`

---

## 4. Shifts and Rotates

* **`ASL`, `LSR`, `ROL`, `ROR`**
  * *Description*: 8-bit shifts and rotates.
  * *Modes*: `acc`, `bp`, `bp,X`, `abs`, `abs,X`
  * *Flags*: `N`, `Z`, `C`

* **`ASR`**
  * *Description*: 8-bit Arithmetic Shift Right (preserves bit 7).
  * *Modes*: `acc`, `bp`, `bp,X`
  * *Flags*: `N`, `Z`, `C`

* **`ASW`, `ROW`**
  * *Description*: 16-bit Arithmetic Shift Word Left, Rotate Word Right.
  * *Modes*: `abs`, `.ax` (assembler simulated for `.ax`)
  * *Flags*: `N`, `Z`, `C`

* **`LSL.16`, `LSR.16`, `ASR.16`, `ROL.16`, `ROR.16` (Simulated)**
  * *Description*: 16-bit Logical/Arithmetic Shifts and Rotates.
  * *Modes*: `.ax`, `abs`
  * *Flags*: `N`, `Z`, `C`

* **`LSL.S16`, `LSR.S16`, `ASR.S16`, `ROL.S16`, `ROR.S16` (Simulated)**
  * *Description*: Signed 16-bit shifts and rotates (equivalents).
  * *Flags*: `N`, `Z`, `C`

---

## 5. Control Flow & Comparison

* **`CMP`, `CPX`, `CPY`, `CPZ`**
  * *Description*: 8-bit comparison.
  * *Modes*: `imm`, `bp`, `abs`, `bp,X/Y`, `abs,X/Y`, `(bp),Y/Z`, `[bp],Z`
  * *Flags*: `N`, `Z`, `C`

* **`CMP.16`, `CMP.S16` (Simulated)**
  * *Description*: 16-bit word comparison (Unsigned / Signed).
  * *Modes*: `.AX, <src>` (`#imm`, `abs`)
  * *Registers Affected*: `.A` (temp)
  * *Flags*: `N`, `Z`, `C` (`CMP.S16` uses high-byte `EOR #$80` logic)

* **`BCC`, `BCS`, `BEQ`, `BNE`, `BMI`, `BPL`, `BVC`, `BVS`, `BRA`**
  * *Description*: Conditional branches. Selects `rel` or `rel16` automatically.
  * *Flags Affected*: None

* **`BBS0-7`, `BBR0-7`**
  * *Description*: Branch if bit Set/Reset.
  * *Modes*: `bp, rel`
  * *Flags Affected*: None

* **`JMP`, `JSR`, `BSR`**
  * *Description*: Jump and Subroutine calls.
  * *Modes*: `abs`, `(abs)`, `(abs,X)`, `rel16` (for BSR)

* **`CALL` (Simulated)**
  * *Description*: Procedural call with parameter passing.
  * *Registers Affected*: `.A`, `.X`, `.Y`, `.Z` (depending on args), plus stack.

* **`CHKZERO.8/16`, `CHKNONZERO.8/16` (Simulated)**
  * *Description*: Evaluation to boolean result in `.A`.
  * *Registers Affected*: `.A`, `.X` (if 16-bit)
  * *Flags*: `N`, `Z`

* **`BRANCH.16` (Simulated)**
  * *Description*: 16-bit `BEQ/BNE` branching.
  * *Registers Affected*: `.X` (temp)

---

## 6. Stack Operations

* **`PHA`, `PHP`, `PHX`, `PHY`, `PHZ`, `PHW`**
  * *Description*: Pushes onto stack.
  * *Modes*: `imp` (`PHW` supports `imm16`, `abs`, `offset, s`)
  * *Flags Affected*: None (`PHP` pushes flags)

* **`PLA`, `PLP`, `PLX`, `PLY`, `PLZ`**
  * *Description*: Pulls from stack.
  * *Modes*: `imp`
  * *Flags Affected*: `N`, `Z` (`PLP` pulls all flags)

* **`PUSH`, `POP` (Simulated)**
  * *Description*: Multi-register push/pop.
  * *Registers Affected*: As specified.
  * *Flags Affected*: None (Pop sequence preserves P)

* **`PTRSTACK` (Simulated)**
  * *Description*: Calculates SP-relative address into `.AX`.
  * *Registers Affected*: `.AX`
  * *Flags*: `N`, `Z`, `C`

---

## 7. DMA and Block Operations

* **`FILL`, `FILL.SP` (Simulated)**
  * *Description*: DMA High-speed fill with `.A`.
  * *Modes*: `abs`, `(zp)`, reg_pair, `offset, s`
  * *Registers Affected*: None (preserved)
  * *Flags*: None

* **`MOVE`, `MOVE.SP` (Simulated)**
  * *Description*: DMA High-speed copy. Length in `.XY`.
  * *Modes*: `abs`, `.AZ`, `offset, s`
  * *Registers Affected*: None (preserved)
  * *Flags*: None

---

## 8. CPU State & System Control

* **`CLC`, `SEC`, `CLI`, `SEI`, `CLV`, `CLD`, `SED`**
  * *Description*: Flag management.
  * *Flags Affected*: Corresponding flag.

* **`CLE`, `SEE`**
  * *Description*: Clear/Set EOM flag (Enables 32-bit flat mode).
  * *Flags Affected*: Emulation/EOM flag.

* **`MAP`**
  * *Description*: Configure Memory Mapper.
  * *Registers Affected*: None (uses current CPU state)

* **`EOM`**
  * *Description*: End of Message / Instruction Prefix ($EA).
  * *Flags Affected*: None

> **Note**: The literal `NOP` mnemonic is disallowed in `ca45`. Use `EOM` for the $EA byte, or `CLV` ($B8) as a safe 1-byte alternative.
