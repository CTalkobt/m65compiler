# 45GS02 Instruction Reference

This reference summarizes the instructions available in the `ca45` assembler for the 45GS02. Instructions are grouped by functional categories, blending hardware-native opcodes with powerful macro-like instructions provided by the assembler. Because some high-level instructions combine operations, they may appear in multiple categories.

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

## 1. Memory Access & Data Movement

* **`LDA`, `LDX`, `LDY`, `LDZ`**
  * *Description*: Loads an 8-bit value into the specified register.
  * *Modes*: `imm`, `bp`, `abs`, `bp,X/Y`, `abs,X/Y`, `(bp),Y/Z`, `[bp],Z`

* **`STA`, `STX`, `STY`, `STZ`**
  * *Description*: Stores an 8-bit value from the specified register into memory.
  * *Modes*: `bp`, `abs`, `bp,X/Y`, `abs,X/Y`, `(bp),Y/Z`, `[bp],Z`

* **`LDAX`, `LDAY`, `LDAZ`**
  * *Description*: Loads a 16-bit word into the A register (low byte) and the X/Y/Z register (high byte).
  * *Modes*: `imm16` (`#val`), `abs`, `offset, s`
  * *Example*: `ldax #$1000` (Loads $00 into A, $10 into X)

* **`STAX`, `STAY`, `STAZ`**
  * *Description*: Stores a 16-bit word from A (low) and X/Y/Z (high) into memory.
  * *Modes*: `abs`, `offset, s`

* **`LDW.SP`, `STW.SP`**
  * *Description*: Explicit stack-relative 16-bit load and store. Safely manipulates 16-bit variables on the stack.
  * *Modes*: `<dest/src>, <offset>`
  * *Example*: `stw.sp .ax, 4` (Stores the `.AX` register pair to SP+4)

* **`LDW.F`, `STW.F`**
  * *Description*: Loads or stores a 16-bit word from/to linear 28-bit memory using hardware `EOM` prefixes.
  * *Modes*: `<addr28>` (28-bit absolute address)

* **`LDQ`, `STQ`**
  * *Description*: Loads or stores the 32-bit Quad register (`A`, `X`, `Y`, `Z` combined).
  * *Modes*: `bp`, `abs`, `(bp),Z`, `[bp],Z`

* **`TAX`, `TAY`, `TAZ`, `TXA`, `TYA`, `TZA`, `TAB`, `TBA`**
  * *Description*: Transfers an 8-bit value directly between CPU registers.
  * *Modes*: `imp`

* **`SWAP`**
  * *Description*: Swaps the values of any two 8-bit registers without needing a temporary memory location.
  * *Modes*: `<reg1>, <reg2>`
  * *Example*: `swap a, x`

* **`ZERO`**
  * *Description*: Clears one or more registers to zero using the most efficient instructions.
  * *Modes*: `<reg1>[, <reg2>...]`
  * *Example*: `zero a, x, y`

* **`PTRDEREF`**
  * *Description*: De-references a 16-bit pointer stored in Zero Page and loads the 16-bit value it points to into `.AX`.
  * *Modes*: `<zp_addr>`
  * *Example*: `ptrderef $02` (Reads the pointer at $02, then loads the word at that pointer into `.AX`)

---

## 2. Arithmetic Operations

* **`ADC`, `SBC`**
  * *Description*: 8-bit Addition/Subtraction with Carry.
  * *Modes*: `imm`, `bp`, `abs`, `bp,X`, `abs,X/Y`, `(bp),Y/Z`, `[bp],Z`

* **`NEG`**
  * *Description*: Performs a two's complement negation of the Accumulator (A = -A).
  * *Modes*: `acc`

* **`ADD.16`, `SUB.16`, `CMP.16`**
  * *Description*: 16-bit addition, subtraction, and comparison on the `.AX` register pair.
  * *Modes*: `.AX, <src>` (src can be immediate, memory, or register)
  * *Example*: `cmp.16 .ax, #1000`
  * *Note*: `CPW` is an alias for `CMP.16`.

* **`MUL.<width>`, `DIV.<width>`**
  * *Description*: Hardware-accelerated multiplication and division using the Mega65 math unit.
  * *Modes*: `<dest>, <src>`. Width can be `.8`, `.16`, `.24`, `.32`. Target can be a register or memory. Source can be a register, memory, or constant.
  * *Example*: `mul.16 .ax, #10` (Multiplies `.AX` by 10)

* **`INC`, `INX`, `INY`, `INZ`, `DEC`, `DEX`, `DEY`, `DEZ`**
  * *Description*: Increments or decrements an 8-bit register or memory location by 1.
  * *Modes*: `acc`, `imp`, `bp`, `bp,X`, `abs`, `abs,X`

* **`INW`, `DEW`**
  * *Description*: Increments or decrements a 16-bit word directly in base-page memory.
  * *Modes*: `bp`

* **`INQ`, `DEQ`**
  * *Description*: Increments or decrements the 32-bit Quad register.
  * *Modes*: `acc`

* **`INC.F`, `DEC.F`**
  * *Description*: Increments or decrements an 8-bit byte in linear 28-bit memory.
  * *Modes*: `<addr28>`

* **`NEG.16`, `ABS.16`**
  * *Description*: 16-bit Negate (Two's Complement) and Absolute Value.
  * *Modes*: `.ax`, `abs`, `stack`
  * *Example*: `abs.16 .ax`

* **`EXPR`**
  * *Description*: Evaluates a complex mathematical formula and generates the code to store the result. Handles constant folding at assembly time and dynamic hardware math at runtime.
  * *Modes*: `<target>, <expression>`
  * *Example*: 
    ```asm
    // Calculates (.X + 5) * 2 and stores the result in .A
    expr .a, (.x + 5) * 2
    ```

---

## 3. Logical Operations & Bit Manipulation

* **`AND`, `ORA`, `EOR`**
  * *Description*: 8-bit bitwise AND, OR, and Exclusive-OR.
  * *Modes*: `imm`, `bp`, `abs`, `bp,X/Y`, `abs,X/Y`, `(bp),Y/Z`, `[bp],Z`

* **`BIT`**
  * *Description*: Tests bits in memory against the Accumulator without modifying the Accumulator. Sets N, V, and Z flags.
  * *Modes*: `imm`, `bp`, `bp,X`, `abs`, `abs,X`

* **`AND.16`, `ORA.16`, `EOR.16`**
  * *Description*: 16-bit bitwise logic operations on the `.AX` register pair.
  * *Modes*: `.AX, <src>`

* **`NOT.16`**
  * *Description*: 16-bit bitwise NOT (one's complement / inversion).
  * *Modes*: `.AX`, `offset, s`, `abs`

* **`ANDQ`, `ORQ`, `EORQ`, `BITQ`**
  * *Description*: 32-bit Quad register bitwise logic operations.
  * *Modes*: `bp`, `abs`, `(bp),Z`, `[bp],Z`

* **`SMB0` - `SMB7`, `RMB0` - `RMB7`**
  * *Description*: Set or Reset a specific bit (0-7) directly in base-page memory.
  * *Modes*: `bp`
  * *Example*: `smb3 $02` (Sets bit 3 of the byte at base-page $02)

* **`TSB`, `TRB`**
  * *Description*: Test and Set/Reset Bit. Applies the Accumulator as a mask to set or reset bits in memory.
  * *Modes*: `bp`, `abs`

* **`SELECT`**
  * *Description*: Conditional assignment. If the CPU Zero flag is CLEAR (result was non-zero), it assigns `val_if_true`; otherwise it assigns `val_if_false`.
  * *Modes*: `<reg>, <val_if_true>, <val_if_false>`
  * *Example*: `select .a, #1, #0`

---

## 4. Shifts and Rotates

* **`ASL`, `LSR`, `ROL`, `ROR`**
  * *Description*: 8-bit Arithmetic Shift Left, Logical Shift Right, Rotate Left, Rotate Right.
  * *Modes*: `acc`, `bp`, `bp,X`, `abs`, `abs,X`

* **`ASR`**
  * *Description*: 8-bit Arithmetic Shift Right. Shifts right but preserves the sign bit (bit 7).
  * *Modes*: `acc`, `bp`, `bp,X`

* **`ASW`, `ROW`**
  * *Description*: 16-bit Arithmetic Shift Word Left, Rotate Word Right.
  * *Modes*: `abs`, `.ax`
  * *Example*: `asw $1000` (Shifts the 16-bit value at $1000-$1001 left by 1)

* **`LSL.16`, `LSR.16`, `ASR.16`, `ROL.16`, `ROR.16`**
  * *Description*: 16-bit Logical/Arithmetic Shifts and Rotates.
  * *Modes*: `abs`, `.ax`
  * *Example*: `lsr.16 .ax` (Logically shifts the `.AX` register pair right by 1)

* **`ASLQ`, `LSRQ`, `ROLQ`, `RORQ`, `ASRQ`**
  * *Description*: 32-bit Quad register shifts and rotates.
  * *Modes*: `acc`, `bp`, `bp,X`, `abs`, `abs,X`

---

## 5. Control Flow & Comparison

* **`CMP`, `CPX`, `CPY`, `CPZ`**
  * *Description*: 8-bit Compare register with memory/immediate. Sets flags for branching.
  * *Modes*: `imm`, `bp`, `abs`, `bp,X/Y`, `abs,X/Y`, `(bp),Y/Z`, `[bp],Z`

* **`CPW`, `CMP.16`**
  * *Description*: 16-bit word compare (See Section 2).

* **`CMPQ`**
  * *Description*: 32-bit Quad register compare.
  * *Modes*: `bp`, `abs`, `(bp),Z`, `[bp],Z`

* **`CHKZERO.8` / `.16`, `CHKNONZERO.8` / `.16`**
  * *Description*: Evaluates target and returns a boolean value (`1` or `0`) in the `.A` register.
  * *Modes*: `<target>`
  * *Example*: `chkzero.16 .ax` (If `.AX` is 0, `.A` becomes 1. Otherwise `.A` becomes 0).

* **`JMP`, `JSR`, `BSR`**
  * *Description*: Jump, Jump to Subroutine, and Branch to Subroutine. `BSR` uses a relative 16-bit offset making code relocatable.
  * *Modes*: `abs`, `(abs)`, `(abs,X)`, `relfar` (for BSR)

* **`CALL`**
  * *Description*: High-level procedural call. Automatically pushes arguments onto the stack before issuing a `JSR`.
  * *Modes*: `<name>[, <args>...]`
  * *Example*: `call print_string, #str_pointer, #length`

* **`RTS`, `RTI`, `ENDPROC`**
  * *Description*: Returns from subroutines or interrupts. `ENDPROC` automatically handles stack frame cleanup for procedures. `RTS imm` adds an immediate value to the return address to skip over inline parameters.

* **`BCC`, `BCS`, `BEQ`, `BNE`, `BMI`, `BPL`, `BVC`, `BVS`, `BRA`**
  * *Description*: Conditional branches. The assembler automatically selects between 8-bit (`rel`) and 16-bit (`relfar`) branches based on distance to the target label.

* **`BRANCH.16`**
  * *Description*: High-level 16-bit conditional branch. Performs a zero check on a 16-bit register and branches appropriately.
  * *Modes*: `<condition>, <target>`
  * *Example*: `branch.16 beq, loop_end`

* **`BBS0`-`BBS7`, `BBR0`-`BBR7`**
  * *Description*: Branch on Bit Set / Branch on Bit Reset. Checks a specific bit in memory and branches in a single instruction.
  * *Modes*: `bp+rel8`
  * *Example*: `bbs7 $d011, wait_vblank` (Branches to `wait_vblank` if bit 7 of base-page $d011 is set)

* **`PROC`**
  * *Description*: Defines a procedural scope. Maps named arguments to stack offsets automatically.
  * *Example*:
    ```asm
    proc add_numbers, w#arg1, w#arg2
        ldax arg1
        add.16 .ax, arg2
    endproc // Automatically cleans the stack and returns
    ```

---

## 6. Stack Operations

* **`PHA`, `PHP`, `PHX`, `PHY`, `PHZ`**
  * *Description*: Pushes an 8-bit register or Processor Status flags onto the stack.
  * *Modes*: `imp`

* **`PLA`, `PLP`, `PLX`, `PLY`, `PLZ`**
  * *Description*: Pulls an 8-bit value from the stack into a register or Processor Status flags.
  * *Modes*: `imp`

* **`PHW`**
  * *Description*: Pushes a 16-bit word onto the stack.
  * *Modes*: `imm16`, `abs`, `offset, s`
  * *Example*: `phw 4, s` (Copies the word currently at SP+4 and pushes it to the top of the stack)

* **`TSX`, `TXS`, `TSY`, `TYS`**
  * *Description*: Transfers the 8-bit Stack Pointer to/from the X or Y registers.
  * *Modes*: `imp`

* **`PTRSTACK`**
  * *Description*: Calculates the absolute memory address of a stack-relative variable and loads that address into `.AX`.
  * *Modes*: `<offset>`
  * *Example*: `ptrstack 4` (Calculates SP + 4 and places the resulting address pointer in `.AX`)

---

## 7. DMA and Block Operations

* **`FILL`, `FILL.SP`**
  * *Description*: Performs a high-speed memory fill using the Mega65 F018 DMA controller. The destination is filled with the byte currently in the `.A` register.
  * *Modes*: `<dest>, <length>`
  * *Example*:
    ```asm
    lda #$FF         // Byte to fill with
    fill $1000, #256 // Fill 256 bytes at $1000 with $FF
    ```

* **`MOVE`, `MOVE.SP`**
  * *Description*: Performs a high-speed memory copy using the Mega65 F018 DMA controller. The 16-bit length must be pre-loaded into the `.XY` register pair.
  * *Modes*: `<source>, <dest>`
  * *Example*:
    ```asm
    ldx #$00
    ldy #$01         // Set .XY to $0100 (256 bytes length)
    move $1000, $2000 // Copy 256 bytes from $1000 to $2000
    ```

---

## 8. CPU State & System Control

* **`CLC`, `SEC`, `CLI`, `SEI`, `CLV`, `CLD`, `SED`**
  * *Description*: Clear/Set CPU Status Flags (Carry, Interrupt Disable, Overflow, Decimal Mode).
  * *Modes*: `imp`

* **`CLE`, `SEE`**
  * *Description*: Clear/Set the EOM (End of Memory) flag. Setting EOM enables 32-bit flat memory access for the next indirect pointer instruction.
  * *Modes*: `imp`
  * *Example*:
    ```asm
    see              // Set EOM
    lda ($02), z     // Evaluated as [bp],z : Loads A from 32-bit address pointed to by ZP $02-$05 + Z
    ```

* **`BRK`**
  * *Description*: Triggers a software interrupt.
  * *Modes*: `imp`

* **`MAP`**
  * *Description*: Memory Map Configuration. Remaps memory banks based on registers.
  * *Modes*: `imp`

* **`EOM`**
  * *Description*: End of Message / Instruction Prefix. On 45GS02, `$EA` is used as a prefix for extended instructions. If used alone, it acts as a 1-cycle NOP.
  * *Modes*: `imp`

> **Note**: The literal `NOP` mnemonic is disallowed in `ca45` to avoid confusion with its role as a prefix. Use `EOM` if you specifically want the `$EA` byte, or `CLV` (`$B8`) as a safe 1-byte NOP alternative.
