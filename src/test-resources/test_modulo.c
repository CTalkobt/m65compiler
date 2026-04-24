int main() {
    int a = 10;
    int b = 3;
    int c = a % b; // 1
    
    if (c != 1) return 1;
    
    int d = 15 % 4; // 3
    if (d != 3) return 2;
    
    int e = 20 % 5; // 0
    if (e != 0) return 3;
    
    // Test power of 2 optimization
    int f = 17 % 8; // 1
    if (f != 1) return 4;
    
    // Test with variables
    int x = 100;
    int y = 7;
    int z = x % y; // 100 / 7 = 14, remainder 2
    if (z != 2) return 5;

    return 0; // PASS
}
