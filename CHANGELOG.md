# Changelog

All notable changes to the cc45 / ca45 suite will be documented in this file.
## [Unreleased] - 2026-04-18

### Added
- **Compiler (cc45)**:
    - Added `-E` flag to run only the preprocessor and output to `stdout` or a file specified by `-o`.
    - Improved multi-call support: if the binary is invoked as `cp45`, it automatically runs in preprocessing-only mode.
- **Preprocessor (cp45)**:
    - Added `bin/cp45` as a new build target (symlink to `cc45`).
    - Defaults to `stdout` output when running in standalone mode.
    - Added support for function-like macros and general macro expansion.
    - Implemented macro operators: `#` (stringification) and `##` (token pasting).
    - Added macro expansion support within `#include` directives.
    - Implemented header guard optimization via `#pragma include_once`.
    - Added support for `#if` and `#elif` expression evaluation, including `defined()`, arithmetic, and logical operators.
    - Added support for line continuation using the backslash (`\`) character.
    - Added support for standard predefined macros: `__STDC__`, `__STDC_VERSION__`, and `__STDC_HOSTED__`.
    - Added support for `#undef`, `#line`, `#error`, `#warning`, and `#pragma` directives.
    - Implemented expansion of standard predefined macros: `__FILE__`, `__LINE__`, `__DATE__`, and `__TIME__`.
- **Documentation**:
    - Renamed preprocessor documentation to `doc/cp45.md` to align with toolchain naming conventions.
- **Assembler (ca45)**:
...
    - Implemented a suite of high-level simulated opcodes:
        - `ldax / lday / ldaz`: 16-bit word loads with support for immediate (`#`), stack-relative (`offset, s`), zero page, and absolute addressing.
        - `stax / stay / staz`: 16-bit word stores with support for stack-relative, zero page, and absolute addressing.
        - `phw`: 16-bit word push, now supports stack-relative `offset, s` using an optimized `tsx` sequence.
        - `add.16 / sub.16`: 16-bit addition/subtraction on the `.ax` register pair.
        - `and.16 / ora.16 / eor.16`: 16-bit bitwise logic on the `.ax` register pair.
        - `neg.16 / not.16`: 16-bit negation and bitwise NOT, now supports `.ax`, stack-relative, and memory operands.
        - `cpw`: 16-bit word comparison on the `.ax` register pair.
        - `branch.16`: High-level 16-bit zero-check branching (`beq`, `bne`).
        - `chkzero.8 / .16`: Truthiness check returning a boolean in `.a`.
        - `ptrstack`: Efficiently calculates and loads a stack variable address into `.ax`.
        - `ptrderef`: De-references a 16-bit pointer stored in Zero Page.
        - `ldw.f / stw.f / inc.f / dec.f`: Linear 28-bit memory access (using hardware `eom` prefixes).
        - `swap / zero`: Multi-register manipulation helpers.
    - Added `@` prefix for "don't care" labels to define local optimization windows.

### Changed
- **Compiler (cc45)**:
    - Updated all code generation to utilize the new simulated opcodes, resulting in significantly more compact assembly.
    - Implemented **Parameter Prefixing**: All function parameters are now prefixed with `_p_` to avoid collisions with CPU registers or flags.
    - Implemented **Local Variable Prefixing**: All local variables are now prefixed with `_l_`.
    - Integrated `ldax #value` for more efficient 16-bit constant and string address loading.
    - Changed default `cc45.zeroPageAvail` to `9`.
- **Assembler (ca45)**:
    - Renamed legacy `ldw .ax` syntax to preferred `ldax` mnemonic.

### Optimized
- **Compiler (cc45)**:
    - **Constant Initialization**: Uses native `phw #value` for declaring local variables with constant initializers, saving 3 instructions and 3 bytes per declaration.
    - **Constant Assignment**: Optimized 16-bit constant assignments to stack variables to use a "lda twice" sequence, avoiding clobbering the `x` register and reducing code size.
    - **Zero Optimization**: Integrated `zero a, x` simulated opcode for more efficient handling of zero literals.
    - **Surgical Loading**: Improved `VariableReference` loading to surgically update only the required registers if part of the word is already in the correct state.
- **Assembler (ca45)**:
    - **Stack-relative Logic**: Extended `stz`, `stx`, and `sty` with `tsx` absolute indexed addressing simulations where native stack-relative variants are missing.
    - **Word Load Tracking**: Enhanced the optimizer to track register states across high-level word load/store operations.

## [Unreleased] - 2026-04-18

### Added
- **Compiler (cc45)**:
    - Added support for pointers to pointers (multiple levels of indirection).
    - Implemented dereference (`*`) and address-of (`&`) unary operators.
    - Updated assignment logic to support arbitrary targets (e.g., `*ptr = value`).
- **Assembler (ca45)**:
    - Added support for `FLAT_INDIRECT_Z` (`[zp],Z`) addressing mode for `ADC`, `AND`, `CMP`, `EOR`, `LDA`, `ORA`, `SBC`, and `STA`.
    - Added support for `IDENTIFIER = expression` equates in `pass1`.
    - Improved `ACCUMULATOR` mode detection to correctly distinguish between registers and labels.

### Fixed
- **Assembler (ca45)**:
    - Fixed branch instruction emission order (opcode now correctly precedes the relative offset).
    - Fixed `SEE` instruction opcode (corrected to `0x03`).
    - Fixed `BSR` 16-bit relative offset calculation.
    - Fixed `pass1` re-evaluation loop to correctly handle `org` and `* =` directives, ensuring accurate address calculation for subsequent labels.
    - Fixed object file mismatch issues by ensuring clean builds.
- **Compiler (cc45)**:
    - Fixed 16-bit pointer arithmetic scaling for `char` and `int` pointer types.
    - Improved hex formatting for ZP scratchpad addresses in generated assembly.

## [Unreleased] - 2026-04-14
...
### Added
- **Compiler (cc45)**:
    - Added support for `do-while` loops.
    - Added support for inline assembly using `asm("...")` or `__asm__("...")`.
    - Added support for `if` and `else` statements.
    - Added support for `while` and `for` loops.
    - Added support for the `char` (8-bit unsigned) type.
    - Implemented full set of comparison operators: `==`, `!=`, `<`, `>`, `<=`, `>=`.
    - Added automated regression testing suite in `src/test/`.
- **Assembler (ca45)**:
    - Added support for `.cpu _45gs02` directive.
    - Added support for `* = $addr` ORG syntax (KickAssembler compatibility).
    - Added support for `//` and `/* ... */` style comments.
    - Added tiered branch strategy: automatically selects between 8-bit and 16-bit relative branches.
    - Implemented dead code removal: strips instructions following `RTS`, `RTN`, or `RTI` within a procedure.
    - Added optimization for `RTN #0` to `RTS` (`0x60`).
    - Added support for 16-bit relative branch opcodes (LBEQ, LBNE, etc. on 45GS02).

### Changed
- **Assembler (ca45)**: Hex literals ($) and binary literals (%) now retain their prefixes in token values for improved parsing.
- **Types**: All basic types (`int`, `char`) are now treated as unsigned by default.
- **Arithmetic**: Improved 16-bit unsigned arithmetic (addition and subtraction) using carry flag sequences.
- **Code Generation**: Variable references and assignments now correctly handle 16-bit stack-relative offsets.
- **Project Structure**: 
    - Moved `test_compiler.sh` to `src/test/`.
    - Renamed `src/test/resources` to `src/test-resources`.
- **Build System**: Updated `Makefile` with improved `test` and `clean` targets.

### Optimized
- `+1` and `-1` operations now use `INC A`, `DEC A`, `INX`, `DEX` when appropriate.
- Function returns with no parameters now use a single-byte `RTS` instead of `RTN #0`.
