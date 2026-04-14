#!/bin/bash

# Simple test script for cc45 compiler

CC="./bin/cc45"
AS="./bin/ca45"

TEST_FILES=(
    "src/test-resources/hello.c"
    "src/test-resources/test_proc.c"
    "src/test-resources/test_control_flow.c"
    "src/test-resources/test_for.c"
    "src/test-resources/test_char.c"
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
    $CC "$f" -o "$s_file"
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
