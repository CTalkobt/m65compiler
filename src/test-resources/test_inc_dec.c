void main() {
    int x = 10;
    x++;       // Should use INW x, s
    ++x;       // Should use INW x, s
    x = x + 1; // Should use INW x, s
    
    char c = 5;
    c--;       // Should use DEC c, s
    --c;       // Should use DEC c, s
    c = c - 1; // Should use DEC c, s
    
    int y = x++; // Should NOT use INW directly (needs result)
}
