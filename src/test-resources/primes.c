void print_num(int n) {}
void print_str(char *s) {}

int main() {
    int n = 2;
    int limit = 100;
    
    print_str("Prime numbers up to 100:\n");
    
    while (n < limit) {
        int i = 2;
        int is_prime = 1;
        while (i * i <= n) {
            if (n / i * i == n) {
                is_prime = 0;
            }
            i = i + 1;
        }
        if (is_prime == 1) {
            print_num(n);
            print_str(" ");
        }
        n = n + 1;
    }
    return 0;
}
