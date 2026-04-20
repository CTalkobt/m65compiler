# MEGA65 Linker & Library System Analysis

This document analyzes the requirements for implementing a linker (`ln45`) and library system for the `cc45`/`ca45` toolchain.

## 1. Problem Statement
The current toolchain uses a "Source-Level Inclusion" model. To share code between files, one must `#include` the source directly. This results in:
- **Redundant Assembly**: Every file in a project is re-parsed and re-assembled for every change.
- **Symbol Collisions**: Manual management of global scope is required.
- **No Pre-compiled Libraries**: High-level libraries (like `math.h` or `stdio.h`) cannot be distributed as binary objects.

## 2. Proposed Solution: Relocatable Objects
Implement a proper linking loader that follows the standard object-link-binary lifecycle.

### A. The Object File (`.o45`)
The `.o45` format will be an **extension of the standard `.o65` format**, a relocatable object format designed for 6502-based systems. While `.o65` is traditionally 16-bit, `.o45` adds support for the MEGA65's expanded capabilities:

- **24-bit / 28-bit / 32-bit Support**: Native handling of the MEGA65's 28-bit linear address space and 32-bit data types.
- **Expanded Header**: Updated mode bits to signal 45GS02 architecture and 32-bit width capability.
- **New Relocation Types**: Support for patching linear addresses and 32-bit values.
    - **R_M65_WORD**: 16-bit address.
    - **R_M65_LOW / R_M65_HIGH**: 8-bit partial address.
    - **R_M65_LINEAR24**: 24-bit linear address for long instructions (e.g., `LDAL`).
    - **R_M65_LINEAR32**: 32-bit linear address for pointers and dwords (addressing the full 256MB space).
- **Machine Code**: The raw bytes with holes for external addresses.
- **Export Table**: A list of symbols defined in this file (e.g., `_main`, `_printf`).
- **Import Table**: A list of symbols used but not defined (e.g., hardware registers, external functions).

### B. Linker Logic (`ln45`)
The linker will perform a two-pass process:
1.  **Symbol Collection**: Scan all input objects and libraries to build a master symbol map.
2.  **Layout & Patching**: Assign final memory addresses to every section and apply relocations.

## 3. Library System (`ar45`)
To support libraries, an archiver tool is needed to:
- Combine multiple `.o45` files into a single `.lib` index.
- Allow the linker to selectively pull only the required object files from the archive.

## 4. Impact on Current Tools
- **Assembler (`ca45`)**: Needs a new mode (`-c`) to emit `.o45` files. Needs new directives: `.section`, `.global`, `.extern`. Must implement the `.o65` extended header and relocation logic.
- **Compiler (`cc45`)**: Should default to emitting `.extern` for standard functions instead of including their full source.

## 5. Implementation Roadmap
1.  **Specification**: Define the `.o45` binary specification (derived from `.o65` but with 32-bit extensions).
2.  **ca45 Update**: Implement section-based assembly and object emission using the new format.
3.  **ln45 Prototype**: Create a linker that can aggregate multiple `.o45` files and resolve 28/32-bit relocations.
4.  **Update the Makefile** and project structure to leverage the new multi-file workflow.
