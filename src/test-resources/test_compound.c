int test_compound() {
    int a = 10;
    a += 5;   // 15
    a -= 2;   // 13
    a *= 3;   // 39
    a /= 4;   // 9
    a %= 7;   // 2
    a <<= 3;  // 16
    a >>= 2;  // 4
    a |= 8;   // 12
    a &= 13;  // 12 & 1101 = 1100 & 1101 = 1100 = 12
    a ^= 15;  // 12 ^ 15 = 1100 ^ 1111 = 0011 = 3
    return a;
}

int main() {
    if (test_compound() == 3) {
        return 0; // PASS
    }
    return 1; // FAIL
}
