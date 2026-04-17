#!/bin/bash

CA="./bin/ca45"
TEST_S="build/full_opcode_test.s"
TEST_BIN="build/full_opcode_test.bin"
META_FILE="build/full_opcode_meta.txt"

mkdir -p build

# 1. Generate the test assembly and metadata
python3 - << 'EOF'
import re
import os

def parse_opcodes(md_file):
    with open(md_file, 'r') as f:
        lines = f.readlines()

    opcodes = []
    current_section = None
    in_table = False
    
    for line in lines:
        line = line.strip()
        if not line: continue
        if line.startswith('## '):
            current_section = line
            in_table = False
            continue
        if line.startswith('|') and 'Byte' in line:
            in_table = True
            continue
        if in_table and line.startswith('|') and '---' not in line:
            parts = [p.strip() for p in line.split('|')]
            if len(parts) >= 4:
                if 'Standard Opcode Table' in current_section:
                    byte = parts[1].replace('$', '')
                    mnemonic = parts[2]
                    mode = parts[3]
                    opcodes.append({'mnemonic': mnemonic, 'mode': mode, 'bytes': [byte]})
                elif 'EOM-Prefixed Instructions' in current_section:
                    encoding = parts[1].split()
                    mnemonic = parts[2]
                    mode = parts[3]
                    bytes_list = [b.replace('$', '') for b in encoding if b not in ('nn', 'nnnn')]
                    opcodes.append({'mnemonic': mnemonic, 'mode': mode, 'bytes': bytes_list})

    content = "".join(lines)
    quad_sections = re.findall(r'### (.*?) — .*?\n\n.*?\|(.*?)\|', content, re.DOTALL)
    for mnemonic_base, table_head in quad_sections:
        section_re = r'### ' + re.escape(mnemonic_base) + r'.*?\|---\|.*?\|\n(.*?)(?:\n\n|\n---|\Z)'
        section_match = re.search(section_re, content, re.DOTALL)
        if section_match:
            table_content = section_match.group(1)
            for line in table_content.strip().split('\n'):
                parts = [p.strip() for p in line.split('|')]
                if len(parts) >= 3:
                    encoding = parts[1].split()
                    mode = parts[2]
                    bytes_list = [b.replace('$', '') for b in encoding if b not in ('nn', 'nnnn')]
                    opcodes.append({'mnemonic': mnemonic_base, 'mode': mode, 'bytes': bytes_list})
    return opcodes

def get_asm(mnemonic, mode):
    if mode == 'imp': return f"{mnemonic}"
    if mode == 'acc' or mode == 'Q (acc)': return f"{mnemonic} A"
    if mode == 'imm': return f"{mnemonic} #$12"
    if mode == 'imm16': return f"{mnemonic} #$1234"
    if mode == 'bp': return f"{mnemonic} $12"
    if mode == 'bp,X': return f"{mnemonic} $12,X"
    if mode == 'bp,Y': return f"{mnemonic} $12,Y"
    if mode == 'abs': return f"{mnemonic} $1234"
    if mode == 'abs,X': return f"{mnemonic} $1234,X"
    if mode == 'abs,Y': return f"{mnemonic} $1234,Y"
    if mode == '(bp,X)': return f"{mnemonic} ($12,X)"
    if mode == '(bp),Y': return f"{mnemonic} ($12),Y"
    if mode == '(bp),Z': return f"{mnemonic} ($12),Z"
    if mode == '[bp],Z': return f"{mnemonic} [$12],Z"
    if mode == '(bp,SP),Y': return f"{mnemonic} ($12,SP),Y"
    if mode == '(abs)': return f"{mnemonic} ($1234)"
    if mode == '(abs,X)': return f"{mnemonic} ($1234,X)"
    if mode == 'rel': return f"L_{mnemonic}_{mode.replace(',','_')}:\n{mnemonic} L_{mnemonic}_{mode.replace(',','_')}"
    if mode == 'relfar': return f"L_{mnemonic}_{mode.replace(',','_')}:\n{mnemonic} L_{mnemonic}_{mode.replace(',','_')}"
    if mode == 'bp+rel8': return f"L_{mnemonic}_{mode.replace('+','_').replace(',','_')}:\n{mnemonic} $12, L_{mnemonic}_{mode.replace('+','_').replace(',','_')}"
    return None

opcodes = parse_opcodes('refs/45gs02_opcodes.md')
with open('build/full_opcode_test.s', 'w') as f_s, open('build/full_opcode_meta.txt', 'w') as f_m:
    f_s.write(".org $2000\n")
    for op in opcodes:
        asm = get_asm(op['mnemonic'], op['mode'])
        if asm:
            f_s.write(f"; {op['mnemonic']} {op['mode']}\n{asm}\n")
            f_m.write(f"{op['mnemonic']}|{op['mode']}|{' '.join(op['bytes'])}\n")
EOF

# 2. Compile each instruction one by one for validation (to match test_opcodes.py behavior)
# This is slow but ensures we are testing the same thing.
passed=0
failed=0
total=0

while IFS='|' read -r mnemonic mode expected_bytes; do
    total=$((total + 1))
    
    asm_code=""
    case $mode in
        "imp") asm_code="${mnemonic}" ;;
        "acc"|"Q (acc)") asm_code="${mnemonic} A" ;;
        "imm") asm_code="${mnemonic} #\$12" ;;
        "imm16") asm_code="${mnemonic} #\$1234" ;;
        "bp") asm_code="${mnemonic} \$12" ;;
        "bp,X") asm_code="${mnemonic} \$12,X" ;;
        "bp,Y") asm_code="${mnemonic} \$12,Y" ;;
        "abs") asm_code="${mnemonic} \$1234" ;;
        "abs,X") asm_code="${mnemonic} \$1234,X" ;;
        "abs,Y") asm_code="${mnemonic} \$1234,Y" ;;
        "(bp,X)") asm_code="${mnemonic} (\$12,X)" ;;
        "(bp),Y") asm_code="${mnemonic} (\$12),Y" ;;
        "(bp),Z") asm_code="${mnemonic} (\$12),Z" ;;
        "[bp],Z") asm_code="${mnemonic} [\$12],Z" ;;
        "(bp,SP),Y") asm_code="${mnemonic} (\$12,SP),Y" ;;
        "(abs)") asm_code="${mnemonic} (\$1234)" ;;
        "(abs,X)") asm_code="${mnemonic} (\$1234,X)" ;;
        "rel") asm_code="${mnemonic} target\ntarget:" ;;
        "relfar") asm_code="${mnemonic} target\n.org \$2200\ntarget:" ;;
        "bp+rel8") asm_code="${mnemonic} \$12, target\ntarget:" ;;
    esac

    echo ".org \$2000" > build/single_op.s
    printf '%b\n' "$asm_code" >> build/single_op.s
    
    $CA -o build/single_op.bin build/single_op.s > /dev/null 2>&1
    if [ $? -ne 0 ]; then
        echo "FAIL (Assemble): $mnemonic $mode"
        failed=$((failed + 1))
        continue
    fi
    
    # Extract actual bytes
    actual_bytes=$(hexdump -v -e '1/1 "%02x " ' build/single_op.bin)
    # Standardize spaces and case
    expected_bytes_lower=$(echo "$expected_bytes" | tr '[:upper:]' '[:lower:]' | xargs)
    actual_bytes_clean=$(echo "$actual_bytes" | tr '[:upper:]' '[:lower:]' | xargs)
    
    # We only care about the first few bytes (the opcode and prefix)
    count=$(echo "$expected_bytes" | wc -w)
    actual_bytes_truncated=$(echo "$actual_bytes_clean" | cut -d' ' -f1-"$count")

    if [ "$actual_bytes_truncated" == "$expected_bytes_lower" ]; then
        passed=$((passed + 1))
    else
        echo "FAIL (Bytes): $mnemonic $mode"
        echo "  Expected: $expected_bytes_lower"
        echo "  Actual:   $actual_bytes_truncated"
        failed=$((failed + 1))
    fi
done < "$META_FILE"

echo ""
echo "Summary: $passed passed, $failed failed, 0 skipped."

if [ $failed -eq 0 ]; then
    exit 0
else
    exit 1
fi
