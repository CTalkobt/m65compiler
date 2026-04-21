int test_switch(int val) {
    int result = 0;
    switch (val) {
        case 1:
            result = 10;
            break;
        case 2:
            result = 20;
            // fallthrough
        case 3:
            result = result + 5;
            break;
        default:
            result = 100;
            break;
    }
    return result;
}

int main() {
    if (test_switch(1) != 10) return 1;
    if (test_switch(2) != 25) return 2;
    if (test_switch(3) != 5) return 3; // result was 0, then 0+5=5
    if (test_switch(4) != 100) return 4;
    return 0;
}
