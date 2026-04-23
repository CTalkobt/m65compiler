
int main() {
    // Mixed declarations (Compound statements)
    int a = 10;
    a = a + 5;
    int b = 20;
    
    // Arrays
    int arr[5];
    arr[0] = a; // 15
    arr[1] = b; // 20
    arr[2] = arr[0] + arr[1]; // 35
    arr[3] = 100;
    arr[4] = arr[3] - arr[2]; // 65
    
    if (arr[0] == 15 && arr[1] == 20 && arr[2] == 35 && arr[3] == 100 && arr[4] == 65) {
        return 0; // Success
    }
    return 1; // Failure
}
