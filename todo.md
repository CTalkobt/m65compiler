# TODO: cc45 Suite Roadmap
Legend:
[ ] - Todo
[W] - Won't do
[I] - In progress
[X] - Done.


## Current Optimizations
- [X] **Dead Code Removal**: Detect and remove code following a `return` statement.
- [X] **Redundant RTN Removal**: Optimize `RTN #0` to `RTS`.
- [X] **Constant Folding**: Evaluate `1 + 2` at compile time.
- [X] **Strength Reduction**: Convert `x * 2` to `x << 1`, etc.
- [X] **Register Allocation**: Better usage of A, X, Y, Z to reduce stack traffic. (Tracking implemented).
- [X] **Increment Optimization**: Use `INC A`, `INX`, `INW`, etc. for `+ 1`.
- [X] **Tiered Branching**: Automatic selection of short/long branches.
- [X] **Logical Short-circuiting**: Implement `&&` and `||` short-circuiting in control flow and expressions.
- [X] **Parameter/Local Mangling**: Prefix variables with `_p_` and `_l_` to avoid CPU register collisions.
- [X] **16 bit operators**: Add 16 bit simulated opcodes (or stack modes) for ASW (asl), ROW (ror), and AND. 

- [X] **Extended Register Tracking**: Track register contents more closely (e.g. variable affinity beyond just value).
- [X] **Constant Propagation**: Substitute variables with known constant values into expressions.
- [X] **Volatile Keyword Support**: Correctly parse 'volatile' and prevent dead store elimination for volatile variables.

---

## Roadmap - Compiler (cc45)
Steps required to bring the C compiler closer to C11 standards.

### 1. Keyword & Syntax Support
- [X] **Control Flow**: Implement `if`, `else`, `while`, `do-while`, and `for` statements.
- [X] **Logical Operators**: Full support for `&&`, `||`, `!` with short-circuiting.
- [X] **Comparison Operators**: Support `==`, `!=`, `<`, `>`, `<=`, `>=`.
- [X] **Regression Testing**: Implement automated build and test runner (`test_compiler.sh`).
- [X] **Static Assertions**: Implement `_Static_assert(const-expr, string)` parsing and validation.
- [ ] **Generic Selections**: Implement `_Generic` expressions for type-based dispatch.
- [ ] **Function Specifiers**: Support `_Noreturn` (enabling optimization to skip return opcodes).
- [ ] **Alignment**: Implement `_Alignas` and `_Alignof` to manage data alignment.
- [X] **Inline Assembly**: Support `asm("...")` or `__asm__("...")` for direct assembly insertion.
- [X] **Preprocessor**: Support `#include`, `#define`, `#undef`, `#if`, `#ifdef`, `#ifndef`, `#elif`, `#else`, `#endif`, `#line`, `#error`, `#warning`, `#pragma`.
- [X] **Macro Expansion**: Support object-like, function-like, and variadic macros, rescanning, and `#` / `##` operators.
- [X] **Variadic Macros**: Support `__VA_ARGS__` and `##__VA_ARGS__` comma removal.
- [X] **Pragma Operator**: Support `_Pragma("...")`.
- [X] **Header Guards**: Support `#pragma include_once` for file inclusion optimization.
- [X] **Expression Evaluation**: Support `defined()`, arithmetic, and logic in `#if` / `#elif`.
- [X] **Line Continuation**: Support `\` at end of line.
- [X] **Predefined Macros**: Support `__FILE__`, `__LINE__`, `__DATE__`, `__TIME__`, `__STDC__`, `__STDC_VERSION__`, `__STDC_HOSTED__`.
- [ ] **Break/Continue**: Implement `break` and `continue` for loop control.
- [ ] **Switch/Case**: Implement `switch`, `case`, and `default`.

## Roadmap - Linker & Libraries (ln45)
- [ ] **Object Format**: Define the `.o45` relocatable object format as an extension of the `.o65` specification.
    - [ ] **Extended Header**: Support 45GS02 architecture bits and 32-bit width flags.
    - [ ] **Linear Relocations**: Support for patching linear 28-bit addresses and 32-bit data types.
- [ ] **Sections**: Update `ca45` to support named segments (`.text`, `.data`, `.bss`).
- [ ] **External Symbols**: Implement `.global` and `.extern` in the assembler.
- [ ] **Relocations**: Support standard (`WORD`, `LOW`, `HIGH`) and linear (`LINEAR24`, `LINEAR32`) relocations.
- [ ] **Linker Tool**: Build the `ln45` binary to aggregate objects and resolve symbols.
- [ ] **Archiver**: Build `ar45` to create static `.lib` archives.

### 2. Type System Enhancements
- [X] **Structures**: Support `struct` definitions, members, and dot/arrow operators.
- [X] **Pointers**: Multi-level indirection, dereferencing (`*`), and address-of (`&`).
- [ ] **Anonymous Structures & Unions**: Support nested structs/unions without names.
- [ ] **Atomic Types**: Support `_Atomic` type qualifier (requires assembler primitives for locking/atomic ops).
- [ ] **Variable Length Arrays**: Support C99/C11 VLAs (requires dynamic stack allocation logic).
- [ ] **Global Variables**: Support top-level variable declarations.
- [ ] **Type Definitions**: Implement `typedef`.
- [ ] **Signed Integers**: Support `signed` types and signed arithmetic/comparisons.

---

## Roadmap - Assembler (ca45)

### 1. Registers
- [X] **Mega65 Multiplication**: Simulated `mul.<width> <dest>, <src>` opcode leveraging hardware multiplier.
- [X] **Mega65 Division**: Simulated `div.<width> <dest>, <src>` opcode leveraging hardware divider.
- [X] **Stack-Relative Word Ops**: Simulated `INW/DEW offset, s` leveraging `TSX`.
- [X] **Other 16 bit registers**: Full support for `.AX`, `.AY`, `.AZ`, `.XY` in simulated high-level opcodes.
- [X] **Mega65 Memory**: High-speed memory FILL and MOVE (copy) leveraging the Mega65 DMA controller.
- [ ] **PC Register**: Treat current program counter as a register named .PC similar to how .A, .X etc are defined. 

### 2. Segments
- [X] **Local Optimization Windows**: Implemented `@` labels to define boundaries for register/flag tracking.
- [ ] **Segment handling**: Ability to define segments to enforce local scope. 
      Also, allow anonymous segments where scope is merely defined. 
- [ ] **Segment Address**: For named segments, allow mapping to various regions of memory. (eg: .segment "READONLY", .segment "CODE", etc. ). Have certain built-in segments pre-defined. Allow usage of other custom segments however. 

### 3. Memory & Alignment
- [X] **Stack-relative Simulation**: Extended `STX/STY/STZ offset, s` with `TSX` sequences for thread-safety.
- [ ] **Alignment Directive**: Implement `.align <n>` or `.balign <n>` to support C11 `_Alignas`.
- [ ] **Segment Management**: Implement `.section` or `.segment` to support `_Thread_local` storage and separate data/text areas.


### 4. Expanded Literals
- [X] **Dword/Long**: Support `.dword` and `.long` for 32-bit unsigned data.
- [X] **Float Support**: Implement `.float` for Commodore 40-bit floating point format.
- [W] **Unicode Support**: Support UTF-8/UTF-16 character and string constants if emitted by the compiler.

### 5. Atomic Primitives
- [ ] **Synchronization Macros**: Provide built-in macros for atomic exchange or compare-and-swap if targeting multi-core or interrupt-safe code.

### 6. KickAssembler Compatibility
- [X] **CPU Selection**: Support `.cpu _45gs02` directive.
- [X] **Comments**: Support `//` and `/* ... */` style comments.
- [X] **ORG Syntax**: Support `* = $addr` for compatibility.
- [X] **BasicUpstart**: Implement `.basicUpstart <addr>` for C64 BASIC stubs.
- [ ] **Binary Import**: Implement `.import binary "file.bin"`.

### 7. Missing Syntax & Instructions
- [W] **Addressing Modes**: Support Absolute Indirect Indexed `($1234),y` (Not supported by 45GS02 hardware).
- [ ] **Native Quad Mode**: Add full native support for `adcq`, `sbcq` etc. (currently prefixed).
- [ ] **Macros**: Implement `.macro` system.
- [X] **Preprocessor**: Implement `#include`, `#define`, `#undef`, `#if`, `#ifdef`, `#ifndef`, `#elif`, `#else`, `#endif`, `#line`, `#error`, `#warning`, `#pragma`. Support function-like macros and operators.
- [ ] **Standard Library**: Add built-in functions like `sin()`, `cos()`, `round()`.
