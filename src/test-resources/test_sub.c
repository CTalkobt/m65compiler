int sub(int a, int b) {
    return a - b;
}

int main() {
    int res = sub(10, 3);
    if (res == 7) {
        return 0; // PASS
    } else {
        return 1; // FAIL
    }
}
