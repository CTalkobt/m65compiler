#!/bin/bash

# Simple test script for cc45 compiler

CC="./bin/cc45"
AS="./bin/ca45"

TEST_FILES=(
    "src/test-resources/hello.c"
    "src/test-resources/test_proc.c"
    "src/test-resources/test_control_flow.c"
    "src/test-resources/test_for.c"
    "src/test-resources/test_do_while.c"
    "src/test-resources/test_char.c"
    "src/test-resources/test_ops.c"
    "src/test-resources/test_recursion.c"
    "src/test-resources/test_complex_math.c"
    "src/test-resources/primes.c"
    "src/test-resources/hanoi.c"
    "src/test-resources/guess.c"
    "src/test-resources/hello_world.c"
    "src/test-resources/test_ptr_arith.c"
    "src/test-resources/test_constant_folding.c"
    "src/test-resources/test_ptr_ptr.c"
    "src/test-resources/test_inc_dec.c"
    "src/test-resources/test_char_flow.c"
    "src/test-resources/test_opt_struct.c"
    "src/test-resources/test_preprocessor_ext.c"
    "src/test-resources/test_preproc_final.c"
    "src/test-resources/test_preproc_macros.c"
    "src/test-resources/test_preproc_v3.c"
    "src/test-resources/test_static_assert_pass.c"
    "src/test-resources/test_break_continue.c"
    "src/test-resources/test_switch.c"
    "src/test-resources/test_switch_continue.c"
)

mkdir -p build/test

failed=0

for f in "${TEST_FILES[@]}"; do
    if [ ! -f "$f" ]; then
        echo "Skip $f (not found)"
        continue
    fi
    
    filename=$(basename "$f")
    s_file="build/test/${filename%.c}.s"
    bin_file="build/test/${filename%.c}.bin"
    
    echo "Testing $f..."
    
    # 1. Compile
    $CC -v "$f" -o "$s_file"
    if [ $? -ne 0 ]; then
        echo "FAIL: Compilation failed for $f"
        failed=$((failed + 1))
        continue
    fi
    
    # 2. Assemble
    $AS "$s_file" -o "$bin_file"
    if [ $? -ne 0 ]; then
        echo "FAIL: Assembly failed for $f"
        failed=$((failed + 1))
        continue
    fi
    
    echo "SUCCESS: $f"
done

if [ $failed -eq 0 ]; then
    echo "All tests passed!"
    exit 0
else
    echo "$failed tests failed."
    exit 1
fi
