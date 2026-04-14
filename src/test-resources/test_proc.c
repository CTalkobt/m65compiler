int add_stuff(int val) {
    int x = 1;
    return val + x + 2;
}

int main() {
    printf("Result: %d", add_stuff(10));
    return 0;
}
