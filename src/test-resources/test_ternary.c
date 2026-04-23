
int main() {
    int a = 15;
    int b = 20;
    
    // Ternary operator
    int h = (a > b) ? a : b; // 20
    int i = (a < b) ? a : b; // 15
    
    // Bitwise operators (with verification)
    int c = a & 7;  // 15 & 7 = 7
    int d = b | 8;  // 20 | 8 = 28
    int e = (a + b) ^ 35; // 35 ^ 35 = 0
    int f = 1 << 3; // 8
    int g = 16 >> 2; // 4
    
    if (h == 20 && i == 15 && c == 7 && d == 28 && e == 0 && f == 8 && g == 4) {
        return 0; // Success
    }
    return 1; // Failure
}
