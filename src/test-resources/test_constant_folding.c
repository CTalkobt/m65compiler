int main() {
    // Basic arithmetic
    int a = 1 + 2;             // Fold to 3
    int b = 10 - 5;            // Fold to 5
    int c = 3 * 4;             // Fold to 12
    int d = 20 / 2;            // Fold to 10
    
    // Complex arithmetic
    int e = (1 + 2) * (10 / 2); // Fold to 15
    
    // Bitwise
    int f = 1 << 4;            // Fold to 16
    int g = 128 >> 2;          // Fold to 32
    int h = 0xFF & 0x0F;       // Fold to 0x0F (15)
    int i = 0xF0 | 0x0F;       // Fold to 0xFF (255)
    int j = 0xAA ^ 0xFF;       // Fold to 0x55 (85)
    
    // Comparison
    int k = (1 == 1);          // Fold to 1
    int l = (1 != 1);          // Fold to 0
    int m = (5 > 3);           // Fold to 1
    int n = (2 <= 1);          // Fold to 0
    
    // Logical
    int o = (1 && 0);          // Fold to 0
    int p = (1 || 0);          // Fold to 1
    
    // Control Flow folding
    if (0) {
        a = 99;                // Removed
    }
    
    if (1) {
        b = 42;                // Just generate body
    } else {
        b = 0;                 // Removed
    }
    
    while (0) {
        c = 0;                 // Removed
    }

    for (int x = 0; 0; x = x + 1) {
        d = 0;                 // Removed
    }

    return a + b + c + d + e + f + g + h + i + j + k + l + m + n + o + p;
}
