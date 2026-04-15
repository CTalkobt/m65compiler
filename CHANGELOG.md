# Changelog

All notable changes to the cc45 / ca45 suite will be documented in this file.

## [Unreleased] - 2026-04-14

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
