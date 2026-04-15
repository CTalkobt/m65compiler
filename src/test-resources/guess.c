void print_str(char *s) {}
int get_input() { return 0; }

int main() {
    int secret = 42;
    int guess = 0;
    
    print_str("Guess the secret number (1-100):\n");
    
    while (guess != secret) {
        guess = get_input();
        if (guess < secret) {
            print_str("Too low!\n");
        } else {
            if (guess > secret) {
                print_str("Too high!\n");
            }
        }
    }
    
    print_str("Correct! You won.\n");
    return 0;
}
