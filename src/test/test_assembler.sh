#!/bin/bash

# Test script for ca45 assembler specific features (directives, etc.)

AS="./bin/ca45"
mkdir -p build/test

failed=0
passed=0

test_file() {
    local f=$1
    local bin_file="build/test/$(basename "$f" .s).bin"
    
    echo "Testing $f..."
    if $AS "$f" -o "$bin_file"; then
        echo "SUCCESS: $f"
        passed=$((passed + 1))
    else
        echo "FAIL: Assembly failed for $f"
        failed=$((failed + 1))
    fi
}

# List of assembler feature tests
AS_TEST_FILES=(
    "src/test-resources/test_basic_upstart.s"
    "src/test-resources/test_data_types.s"
    "src/test-resources/test_mul.s"
    "src/test-resources/test_div.s"
    "src/test-resources/test_expr.s"
    "src/test-resources/hello_call.s"
    "src/test-resources/test_var.s"
    "src/test-resources/test_45gs02.s"
)

for f in "${AS_TEST_FILES[@]}"; do
    test_file "$f"
done

# Specific check for basicUpstart output format
echo "Verifying .basicUpstart output bytes..."
# Expecting: 0b 08 0a 00 9e 32 30 36 31 00 00 00 (for START at $080D = 2061)
ACTUAL=$(hexdump -v -n 12 -e '1/1 "%02x "' build/test/test_basic_upstart.bin | xargs)
EXPECTED="0b 08 0a 00 9e 32 30 36 31 00 00 00"

if [ "$ACTUAL" == "$EXPECTED" ]; then
    echo "SUCCESS: .basicUpstart output matches expected format."
else
    echo "FAIL: .basicUpstart output mismatch."
    echo "  Expected start: $EXPECTED"
    echo "  Actual:         $ACTUAL"
    failed=$((failed + 1))
fi

if [ $failed -eq 0 ]; then
    echo "All assembler feature tests passed!"
    exit 0
else
    echo "$failed tests failed."
    exit 1
fi
