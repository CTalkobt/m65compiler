# cc45 — Mega65 C Compiler

`cc45` is a C compiler designed for the MEGA65 (45GS02). It translates a subset of the C language into optimized assembly code.

## Features
- **Function Declarations**: Full support for function definitions with parameters.
- **Local Variables**: Support for local variables allocated on the stack.
- **Nested Expressions**: Robust handling of complex arithmetic expressions and nested function calls.
- **String Pooling**: Automatic identification and pooling of string literals into a data section.
- **Procedure System**: Translates C functions into `PROC` blocks for `ca45`, leveraging high-level procedure syntax.

## Supported Language Dialect
The compiler currently supports a subset of standard C:
- **Types**: `int` (treated as 16-bit word).
- **Control Flow**: `return` statements.
- **Arithmetic**: Addition (`+`) and Subtraction (`-`) with operator precedence.
- **Functions**: Function definitions and calls with variable numbers of arguments.
- **Strings**: String literals used in function calls (e.g., `printf`).
- **Register Access**: Support for referencing CPU registers (`.A`, `.AX`, etc.) and processor status flags (`P.C`, `P.Z`, etc.) within the compiler's intermediate logic and future inline assembly.

## Command-Line Usage
```bash
cc45 [options] <input_file.c>
```

### Options
- `-o <file>`: Specify the output assembly file name (default is `out.s`).
- `-c`: Compile and then invoke `ca45` to generate a binary object file (`.bin`).
- `-v`: Verbose mode. Prints lexical tokens, the Abstract Syntax Tree (AST), and assembly generation logs.

## Compilation Process
1.  **Lexical Analysis**: Strips comments (both `//` and `/* */`) and tokenizes the source code.
2.  **Parsing**: Builds an AST using recursive descent, ensuring correct operator precedence and statement structure.
3.  **Code Generation**: Traverses the AST and emits high-level `ca45` assembly.
    - Function arguments and local variables are managed via the stack.
    - The compiler uses the `.var` and `.cleanup` assembler directives to maintain correct stack offsets as the stack depth changes during expression evaluation.

## Calling Convention
`cc45` follows a **Callee-Cleanup** convention:
1.  **Caller**: Pushes arguments onto the stack (low byte first for 16-bit values).
2.  **JSR**: Invokes the procedure.
3.  **Callee**: Accesses arguments relative to the Stack Pointer (SP).
4.  **Return**: The callee executes a `RTN #n` instruction (via `ENDPROC`), which cleans the stack and returns to the caller in one operation.
5.  **Return Value**: The result of a function is typically returned in the Accumulator (A) and X register (for 16-bit values).
