# [PRELIMINARY, PROPOSED FUTURE WORK] Minimal C Standard Library (stdlib) for MEGA65/45GS02

This document outlines the requirements and proposed file structure for a minimal `stdlib` implementation for the `cc45` compiler. It focuses on essential functionality for embedded systems while excluding large features like `printf`, `sprintf`, and floating-point math.

## 1. Core Headers & Types

These files define the fundamental types and constants required for C compliance and consistent memory layout.

- **`<stdint.h>`**: Fixed-width integer types (e.g., `uint8_t`, `int16_t`, `uint32_t`). This is critical for the 45GS02's 32-bit capabilities.
- **`<stddef.h>`**: Common definitions like `size_t`, `ptrdiff_t`, and `NULL`.
- **`<stdbool.h>`**: Boolean type definitions (`bool`, `true`, `false`).
- **`<limits.h>`**: Implementation-defined limits for integer types.

## 2. Memory Management (`stdlib.h`)

Minimal dynamic memory allocation. Given the MEGA65's memory banking, a simple heap manager is required.

- **`malloc(size_t size)`**: Allocate a block of memory from the heap.
- **`free(void *ptr)`**: Deallocate a previously allocated block.
- **`calloc(size_t nmemb, size_t size)`**: Allocate and zero-initialize memory.
- **`realloc(void *ptr, size_t size)`**: Resize an existing allocation.

## 3. String & Memory Operations (`string.h`)

Essential for data manipulation. These should ideally be implemented in optimized assembly using 45GS02 MAP/EOM or DMA operations if available.

- **`memcpy(void *dest, const void *src, size_t n)`**: Copy memory area.
- **`memset(void *s, int c, size_t n)`**: Fill memory with a constant byte.
- **`memmove(void *dest, const void *src, size_t n)`**: Copy memory area (handling overlaps).
- **`memcmp(const void *s1, const void *s2, size_t n)`**: Compare memory areas.
- **`strlen(const char *s)`**: Calculate string length.
- **`strcpy(char *dest, const char *src)`**: Copy string.
- **`strcmp(const char *s1, const char *s2)`**: Compare strings.

## 4. Character Utilities (`ctype.h`)

Simple lookup-table or logic-based character classification.

- **`isdigit(int c)`**, **`isalpha(int c)`**, **`isalnum(int c)`**
- **`isspace(int c)`**, **`isprint(int c)`**
- **`toupper(int c)`**, **`tolower(int c)`**

## 5. General Utilities (`stdlib.h`)

- **`abs(int j)`**: Absolute value.
- **`atoi(const char *nptr)`**: Convert string to integer.
- **`itoa(int value, char *str, int base)`**: Convert integer to string (essential for embedded display).
- **`rand()` / `srand(unsigned int seed)`**: Pseudo-random number generation.
- **`exit(int status)`**: Terminate program execution (usually a jump back to a monitor or a `Halt`).

## 6. Minimal I/O (`stdio.h`)

Excluding `printf` for now, but providing the building blocks for character output.

- **`putchar(int c)`**: Output a single character (via KERNAL or hardware UART).
- **`getchar()`**: Input a single character.
- **`puts(const char *s)`**: Output a string followed by a newline.

## 7. Runtime Startup (`crt0.s`)

The assembly entry point that prepares the environment for `main()`.

- **Stack Initialization**: Set the stack pointer (`SP`).
- **BSS Clearing**: Zero out uninitialized global variables.
- **Data Initialization**: Copy initialized globals from ROM to RAM.
- **Hardware Init**: Basic MEGA65 setup (e.g., IO speed, memory mapping).
- **Call `main`**: Transition to C code.

## 8. Proposed File Structure

```text
lib/
├── include/           # Standard C headers
│   ├── ctype.h
│   ├── limits.h
│   ├── stdbool.h
│   ├── stddef.h
│   ├── stdint.h
│   ├── stdio.h
│   ├── stdlib.h
│   └── string.h
├── src/
│   ├── crt0.s         # Startup assembly
│   ├── ctype.c        # Character classification
│   ├── heap.c         # malloc/free implementation
│   ├── stdio.c        # putchar/getchar/puts
│   ├── stdlib.c       # abs/atoi/rand/etc.
│   └── string.c       # memcpy/strlen/etc. (or string.s for optimized asm)
└── Makefile           # Build script to produce libc.lib (or libc.o45)
```
