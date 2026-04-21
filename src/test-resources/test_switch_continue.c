int test_switch_continue(int val) {
    int result = 0;
    switch (val) {
        case 1:
            result = result + 1;
            continue 2; // Jump to case 2
        case 2:
            result = result + 10;
            if (val == 1) break; // Should be 11
            continue default; // Jump to default
        case 3:
            result = 50;
            continue 1; // Jump back to case 1 (warning: potential infinite loop if not careful)
            // But here val is 3, so:
            // 3 -> result=50 -> case 1 -> result=51 -> case 2 -> result=61 -> default -> result=161
        default:
            result = result + 100;
            break;
    }
    return result;
}

int main() {
    if (test_switch_continue(1) != 11) return 1;
    if (test_switch_continue(2) != 110) return 2;
    if (test_switch_continue(3) != 161) return 3;
    if (test_switch_continue(4) != 100) return 4;
    return 0;
}
