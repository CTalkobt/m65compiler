# Mega65 C Compiler Suite (ca45, cc45)
*CONSIDER YOURSELF WARNED:* This project is currently undergoing heavy development


This project provides a modern, object-oriented development toolchain for the MEGA65 (45GS02) home computer. It consists of two primary tools: **cc45** (a C compiler) and **ca45** (a 45GS02-optimized assembler).


## Project Intent
The goal of this suite is to provide a C-based development environment that leverages the advanced features of the MEGA65, such as its 32-bit registers (Q-mode), 28-bit flat memory addressing, and stack-relative addressing. Unlike traditional 6502 compilers, this toolchain is designed from the ground up to support modern calling conventions and procedural abstractions directly at the assembly level.

## Architecture
The compilation process follows a multi-pass pipeline:
1.  **cc45 (Source to Assembly)**: Translates C source code into high-level 45GS02 assembly. It performs lexical analysis, builds an Abstract Syntax Tree (AST), and generates assembly code using an optimized procedure system.
2.  **ca45 (Assembly to Binary)**: Processes the assembly output, resolves symbols, manages stack offsets for procedures, and emits the final machine code binary.

## Design Philosophy
- **Procedural Abstraction**: Function calls are treated as first-class citizens with named parameters and automated stack cleanup.
- **Hierarchical Scoping**: Supports nested local scopes for labels and variables within procedures and `{}` blocks, preventing namespace pollution and allowing safe label reuse.
- **MEGA65 First**: Special emphasis is placed on supporting the 45GS02 instruction set enhancements, including a high-level expression engine (`expr`) that handles constant folding and register arithmetic.
- **Compatibility**: Supports KickAssembler-style syntax (comments, `* =`, `.cpu`) for easier porting of existing MEGA65 codebases.
- **Inline Assembly**: Standard C `asm()` and `__asm__()` support for direct hardware control from C source.
- **Extensibility**: The toolchain uses a visitor-based architecture in C++, making it easy to add new optimizations, language features, or hardware targets.

## General Usage
The tools are designed to work together seamlessly. A typical workflow involves compiling a C file directly to a binary:
```bash
./bin/cc45 -c -o output.s input.c
```
This command invokes `cc45` to generate `output.s`, and then automatically calls `ca45` to produce `output.s.bin`.

For a full list of command-line options, use the `-?` flag:
```bash
./bin/cc45 -?
./bin/ca45 -?
```

## Testing & Regressions
The suite includes a set of automated tests to verify the compiler and assembler. You can run all tests from the project root:
```bash
make test
```
This script compiles C source files from `src/test-resources/` and assembles them to ensure stability and correctness. Outputs are stored in the `build/test/` directory.

For detailed information on each tool, refer to:
- [doc/cc45.md](doc/cc45.md) — Compiler Usage and Features
- [doc/ca45.md](doc/ca45.md) — Assembler Syntax and Reference
