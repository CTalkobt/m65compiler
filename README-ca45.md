# ca45 — Mega65 45GS02 Assembler

`ca45` is a powerful, high-level assembler designed for the MEGA65 (45GS02). It combines traditional assembly with advanced procedural features and dynamic variable management.

## Syntax Overview
- **Comments**: Start with a semicolon (`;`) and continue to the end of the line.
- **Labels**: Defined by an identifier followed by a colon (e.g., `start:`).
- **Equates**: `NAME = value`. Supports arithmetic (`+`, `-`) and various literal formats.
- **Case Sensitivity**: Mnemonics and register names are case-insensitive.

## Literal Formats
- **Hexadecimal**: `$1234`
- **Decimal**: `123`
- **Binary**: `%1010`
- **Character**: `'A'` (converted to PETSCII in `.text` context)
- **String**: `"Hello"` (PETSCII or ASCII depending on directive)

## Supported Opcodes
`ca45` supports the full 45GS02 instruction set, including standard 6502/65CE02 instructions and MEGA65 enhancements:
- **Z Register**: `LDZ`, `STZ`, `INZ`, `DEZ`, `PHZ`, `PLZ`.
- **Word Instructions**: `PHW`, `INW`, `DEW`, `ASW`, `ROW`, `BSR`.
- **Extended Branches**: 16-bit relative branches (`BSR`, `BRA` relfar).
- **Stack-Relative Addressing**: `LDA offset, s`.

## Directives (Pseudo-ops)
- `.org <addr>`: Sets the current Program Counter (PC).
- `.byte <v1>[, ...]` : Emits 8-bit bytes.
- `.word <v1>[, ...]` : Emits 16-bit words (little-endian).
- `.text "<string>"` : Emits a PETSCII-encoded string (case-swapped).
- `.ascii "<string>"`: Emits a raw ASCII-encoded string.
- `.var <name> = <expr>`: Defines or updates an assembler-time variable.
- `.cleanup <n>`: Adjusts the internal stack tracking for the current procedure.
- `expr <target>, <expression>`: Generates machine code to evaluate an expression and store the result in `<target>`.
    - `<target>` can be a register (e.g., `.A`, `.AX`) or a memory location.

## Register and Flag References
`ca45` allows referencing CPU registers and status flags directly within expressions:
- **8-bit Registers**: `.A`, `.X`, `.Y`, `.Z`, `.SP` (low byte).
- **16-bit Registers**: `.AX`, `.AY`, `.AZ`, `.XY`, etc. (Combined register pairs).
- **32-bit Q-Register**: `.Q`.
- **Processor Status Flags**: `P.C` (Carry), `P.Z` (Zero), `P.I` (Interrupt), `P.D` (Decimal), `P.B` (Break), `P.V` (Overflow), `P.N` (Negative), `P.E` (Emulation).

## The Procedure System
`ca45` features a built-in procedure system that automates stack-based parameter passing.

### `PROC <name>[, [B#|W#]<param1>, ...]`
Defines a procedure and its named parameters.
- Parameters can be prefixed with `B#` (8-bit byte) or `W#` (16-bit word, default).
- Parameters are automatically mapped to the correct stack offset.
- `ARG1`, `ARG2`, etc., are also available as positional aliases.

### `CALL <name>[, <arg1>, ...]`
Invokes a procedure.
- Arguments are pushed onto the stack before the `JSR`.
- Supports immediate values (`#$10`), labels, or existing stack variables.
- Size of arguments is inferred from the symbol table or forced with `B#`/`W#`.

### `ENDPROC`
Completes a procedure block.
- Automatically calculates the total size of all parameters.
- Emits a `RTN #n` instruction to clean the stack and return to the caller.

## Assembler-Time Variables (`.var`)
Variables defined with `.var` are unique because they can be reassigned during the assembly process.
- `.var x = 10`
- `.var x = x + 2`
- `.var x++` / `.var x--`
These variables do not occupy memory in the final binary; they are used by the assembler to calculate offsets or repeated values dynamically.
