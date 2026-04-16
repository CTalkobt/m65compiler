void test_ops() {
    int a = 10;
    int b = 20;
    int c = 0;
    
    // Bitwise
    c = a & b;
    c = a | b;
    c = a ^ b;
    c = ~a;
    
    // Shifts
    c = a << 2;
    c = b >> 1;
    
    // Logical
    if (a && b) {
        c = 1;
    }
    
    if (c || 0) {
        c = 2;
    }
    
    if (!c) {
        c = 3;
    }
    
    // Unary Minus
    c = -a;
}
