void test_ptr() {
    char* p_char = 1000;
    int* p_int = 2000;

    // char* arithmetic (scale 1)
    p_char = p_char + 1;  // Should be 1001
    p_char = p_char + 5;  // Should be 1006
    
    // int* arithmetic (scale 2)
    p_int = p_int + 1;    // Should be 2002
    p_int = p_int + 10;   // Should be 2022
    
    p_int = p_int - 1;    // Should be 2020
}
