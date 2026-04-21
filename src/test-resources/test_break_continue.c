int main() {
    int i = 0;
    int sum = 0;

    // Test while break
    while (i < 10) {
        if (i == 5) break;
        sum = sum + i;
        i = i + 1;
    }
    // sum should be 0+1+2+3+4 = 10

    // Test for continue
    int j;
    int sum2 = 0;
    for (j = 0; j < 10; j = j + 1) {
        if (j % 2 == 0) continue;
        sum2 = sum2 + j;
    }
    // sum2 should be 1+3+5+7+9 = 25

    // Test do-while break
    int k = 0;
    int sum3 = 0;
    do {
        if (k == 3) break;
        sum3 = sum3 + k;
        k = k + 1;
    } while (k < 10);
    // sum3 should be 0+1+2 = 3

    // Test nested break
    int x;
    int y;
    int nested_sum = 0;
    for (x = 0; x < 3; x = x + 1) {
        for (y = 0; y < 10; y = y + 1) {
            if (y == 2) break;
            nested_sum = nested_sum + 1;
        }
    }
    // nested_sum should be 3 * 2 = 6

    return sum + sum2 + sum3 + nested_sum; 
    // 10 + 25 + 3 + 6 = 44
}
