// Test advanced expressions in ca45
.org $2000

.var ptr = $10

main:
    // Bitwise ops
    expr .A, $0F & %1010    // Should be $0A
    expr .A, $F0 | $0F      // Should be $FF
    expr .A, $AA ^ $FF      // Should be $55
    
    // Shifts
    expr .A, $01 << 3       // Should be $08
    expr .A, $80 >> 4       // Should be $01
    
    // Relational (Result should be 1 or 0)
    expr .A, 10 == 10       // 1
    expr .A, 5 != 5         // 0
    expr .A, 10 > 5         // 1
    expr .A, 2 <= 2         // 1
    
    // Logical
    expr .A, 1 && 1         // 1
    expr .A, 1 || 0         // 1
    expr .A, !0             // 1
    
    // Dereferencing
    expr .A, *ptr           // LDA ($10), Z
    expr .A, [ptr]          // EOM : LDA ($10), Z
    
    // Nested
    expr .AX, (1 << 4) | (1 << 0) // Should be 17
    
    RTN
