
int global_arr[10];
int global_val;

int main() {
    global_arr[0] = 42;
    global_val = global_arr[0];
    if (global_val == 42) return 0;
    return 1;
}
