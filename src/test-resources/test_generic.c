
int select_int() { return 1; }
int select_char() { return 2; }
int select_ptr() { return 3; }
int select_default() { return 0; }

int main() {
    int i = 10;
    char c = 'a';
    int* p = &i;
    
    // Select constant based on type
    int v1 = _Generic(i, int: 10, char: 20, default: 0);
    int v2 = _Generic(c, int: 10, char: 20, default: 0);
    
    // Select value based on type
    int r1 = _Generic(i, int: 1, char: 2, default: 0);
    int r2 = _Generic(c, int: 1, char: 2, default: 0);
    int r3 = _Generic(p, int*: 3, default: 0);

    if (v1 == 10 && v2 == 20 && r1 == 1 && r2 == 2 && r3 == 3) {
        return 0; // Success
    }
    return 1; // Failure
}
