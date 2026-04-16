# STYLEGUIDE.md - mmsim (Multi-Machine Simulator)

This document outlines the coding style and naming conventions for the mmsim project.

## 1. Naming Conventions

The project prefers **camelCase** for local variables, function parameters, and member variables.

- **Classes and Structs:** Use `PascalCase` (e.g., `MachineDescriptor`, `FlatMemoryBus`).
- **Functions and Methods:** Use `camelCase` (e.g., `readByte()`, `onReset()`).
- **Variables and Parameters:** Use `camelCase` (e.g., `addrBits`, `expectedSize`).
- **Member Variables:** Use `camelCase` with an optional `m_` prefix (e.g., `m_addrMask`, `m_role`).
- **Constants and Macros:** Use `UPPER_SNAKE_CASE` (e.g., `REGFLAG_INTERNAL`, `ISA_CAP_6502`).
- **Files:** Use `snake_case` for filenames (e.g., `machine_desc.h`, `cpu_6502.cpp`).

## 2. Formatting

- **Indentation:** Use 4 spaces for indentation. No tabs.
- **Braces:** Use K&R style (braces on the same line as the statement).
    ```cpp
    void reset() {
        if (m_isInitialized) {
            // ...
        }
    }
    ```
- **Line Length:** Aim for a maximum of 100-120 characters per line.
- **New Lines:** For if statements with no block { } usage, it's ok to keep on a single
  line if it's short (roughly less than 80 characters). Otherwise, use multi-line. 

## 3. C++ Standards

- **Standard:** C++17.
- **Header Guards:** Prefer `#pragma once`.
- **Typing:** Use explicit fixed-width integer types from `<cstdint>` (e.g., `uint8_t`, `uint16_t`, `uint32_t`).

## 4. Documentation

- Use `//` for single-line comments.
- Use `/** ... */` for Doxygen-style documentation on public interfaces and class definitions.
- Document complex logic or non-obvious design decisions.
- Document all classes and any methods that are over 5 lines or are not obvious at first glance. 

## 5. Plugin ABI

- Functions exported by plugins MUST follow the C calling convention and be declared `extern "C"`.
- The plugin entry point is `mmemuPluginInit` — camelCase, consistent with the rest of the codebase.
