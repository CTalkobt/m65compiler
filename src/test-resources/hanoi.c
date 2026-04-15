void move_disk(int disk, int from, int to) {}

void hanoi(int n, int from, int to, int aux) {
    if (n == 1) {
        move_disk(1, from, to);
        return;
    }
    hanoi(n - 1, from, aux, to);
    move_disk(n, from, to);
    hanoi(n - 1, aux, to, from);
}

int main() {
    hanoi(3, 1, 3, 2);
    return 0;
}
