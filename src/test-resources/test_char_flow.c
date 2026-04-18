void main() {
    char c = 1;
    if (c) { // Should be a single BNE
        c = 0;
    }
    
    while (c == 0) { // Should be optimized 8-bit comparison
        c = 1;
    }
    
    for (char i = 0; i < 10; i++) {
        // Test 8-bit loop
    }
}
