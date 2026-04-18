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
    "src/test-resources/test_expr_advanced.s"
    "src/test-resources/hello_call.s"
    "src/test-resources/test_var.s"
    "src/test-resources/test_45gs02.s"
    "src/test-resources/test_equates_advanced.s"
    "src/test-resources/test_flat_z_all.s"
    "src/test-resources/test_accumulator_labels.s"
    "src/test-resources/test_multi_org.s"
    "src/test-resources/test_peephole.s"
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

echo "Testing -Dcc45.zeroPageStart=\$10..."
# Use a dereference which is guaranteed to use ZP scratch $02/$03
echo -e ".var ptr = \$20\nexpr .A, *ptr" > build/test_zp_shift.s
$AS "-Dcc45.zeroPageStart=\$10" -o build/test_zp_shift.bin build/test_zp_shift.s
# 85 10 (STA $10) should be in there (saving eval'd ptr address to ZP scratch)
ACTUAL_ZP=$(hexdump -v -e '1/1 "%02x "' build/test_zp_shift.bin)
if [ -f build/test_zp_shift.bin ] && echo "$ACTUAL_ZP" | grep -q "85 10"; then
    echo "SUCCESS: -Dcc45.zeroPageStart correctly shifted ZP addresses."
else
    echo "FAIL: -Dcc45.zeroPageStart did not shift ZP addresses correctly."
    echo "Actual bytes: $ACTUAL_ZP"
    failed=$((failed + 1))
fi

if [ $failed -eq 0 ]; then
    echo "All assembler feature tests passed!"
    exit 0
else
    echo "$failed tests failed."
    exit 1
fi
