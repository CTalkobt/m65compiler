int complex_math(int a, int b, int c) {
    int res = 0;
    // Test all new operators and nesting
    res = ((a + b) * (c - 5)) / ((a & 0x0F) | (b ^ c)) << 2;
    
    // Test logical short-circuiting and boolean results
    if ((a > b && b > c) || (a == c)) {
        res = res + 100;
    }
    
    return res;
}
