# Mega65 C Compiler Suite

This project provides a modern, object-oriented development toolchain for the MEGA65 (45GS02) home computer. It consists of two primary tools: **cc45** (a C compiler) and **ca45** (a 45GS02-optimized assembler).

## Project Intent
The goal of this suite is to provide a C-based development environment that leverages the advanced features of the MEGA65, such as its 32-bit registers (Q-mode), 28-bit flat memory addressing, and stack-relative addressing. Unlike traditional 6502 compilers, this toolchain is designed from the ground up to support modern calling conventions and procedural abstractions directly at the assembly level.

## Architecture
The compilation process follows a multi-pass pipeline:
1.  **cc45 (Source to Assembly)**: Translates C source code into high-level 45GS02 assembly. It performs lexical analysis, builds an Abstract Syntax Tree (AST), and generates assembly code using an optimized procedure system.
2.  **ca45 (Assembly to Binary)**: Processes the assembly output, resolves symbols, manages stack offsets for procedures, and emits the final machine code binary.

## Design Philosophy
- **Procedural Abstraction**: Function calls are treated as first-class citizens with named parameters and automated stack cleanup.
- **MEGA65 First**: Special emphasis is placed on supporting the 45GS02 instruction set enhancements, including a high-level expression engine (`EXPR`) that handles constant folding and register arithmetic.
- **Extensibility**: The toolchain uses a visitor-based architecture in C++, making it easy to add new optimizations, language features, or hardware targets.

## General Usage
The tools are designed to work together seamlessly. A typical workflow involves compiling a C file directly to a binary:
```bash
./bin/cc45 -c -o output.s input.c
```
This command invokes `cc45` to generate `output.s`, and then automatically calls `ca45` to produce `output.s.bin`.

For detailed information on each tool, refer to:
- [README-cc45.md](README-cc45.md) — Compiler Usage and Features
- [README-ca45.md](README-ca45.md) — Assembler Syntax and Reference
