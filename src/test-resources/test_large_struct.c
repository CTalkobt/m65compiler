struct Large {
    int m1;
    int m2;
    int m3;
    int m4;
    int m5;
};

void test() {
    struct Large a;
    struct Large b;
    
    // Test memset (part 1)
    memset(&a, 65, 10);
    
    // Test memcpy (part 1)
    memcpy(&b, &a, 10);
    
    // Test struct assignment (part 2)
    a = b;
}
