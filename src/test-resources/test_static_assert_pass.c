_Static_assert(1, "this should pass");

int main() {
    _Static_assert(2 + 2 == 4, "math works");
    return 0;
}
