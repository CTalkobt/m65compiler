* = $2000
// Test 32-bit types
.dword $12345678, 1000000
.long  $AABBCCDD, 2000000

// Test Float (1.0 should be $81 $00 $00 $00 $00)
// Test Float (0.5 should be $80 $00 $00 $00 $00)
// Test Float (2.0 should be $82 $00 $00 $00 $00)
.float 1.0, 0.5, 2.0, -1.0
