int main() {
    int sum = 0;
    for (int i = 0; i < 10; i = i + 1) {
        sum = sum + i;
    }
    // i should not be accessible here in strict C99, 
    // but our compiler might still allow it if we didn't fully implement scoping.
    // However, our implementation should have cleaned it up from the stack.
    
    if (sum == 45) {
        return 0; // PASS
    }
    return 1; // FAIL
}
