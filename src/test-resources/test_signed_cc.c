int zero = 0;

int main() {
    volatile signed int a = -100 + zero;
    volatile signed int b = 50 + zero;
    
    // Signed: -100 < 50 is TRUE
    if (a < b) {
        // Correct
    } else {
        return 1;
    }
    
    volatile unsigned int ua = 65436 + zero; // -100 if interpreted as signed
    volatile unsigned int ub = 50 + zero;
    
    // Unsigned: 65436 < 50 is FALSE
    if (ua < ub) {
        return 2;
    } else {
        // Correct
    }
    
    return 0;
}
