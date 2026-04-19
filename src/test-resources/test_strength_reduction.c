int main() {
    int a = 10;
    int b = a * 2;  // Should be ASL
    int c = a * 4;  // Should be ASL x 2
    int d = a * 3;  // Should be MUL.16
    int e = a / 2;  // Should be LSR
    int f = a % 4;  // Should be AND
    int g = a << 2; // Should be ASL x 2
    int h = a >> 1; // Should be LSR
    
    return b + c + d + e + f + g + h;
}
