int main() {
    volatile int x = 10;
    int y = 20;
    volatile int z = x + y; // z is volatile, should not be optimized away
    int a = x * 2; // x is volatile, so this expression cannot be folded or optimized away based on x's value
    
    // Even though x and z are not 'used' in the return, they should still be present in the assembly
    return 0;
}
