enum Color {
    RED,
    GREEN,
    BLUE = 5,
    YELLOW
};

int main() {
    enum Color c = RED;
    if (c != 0) return 1;
    
    c = GREEN;
    if (c != 1) return 2;
    
    c = BLUE;
    if (c != 5) return 3;
    
    c = YELLOW;
    if (c != 6) return 4;
    
    // Anonymous enum
    enum {
        X = 10,
        Y
    };
    
    if (X != 10) return 5;
    if (Y != 11) return 6;

    return 0; // PASS
}
