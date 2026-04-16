* = $2000

// 8-bit multiply
mul.8 .A, 10
mul.8 $2100, .X

// 16-bit multiply
mul.16 .AX, 500
mul.16 $2100, $2200

// 32-bit multiply
mul.32 .Q, $12345678
mul.32 $2100, .Q
