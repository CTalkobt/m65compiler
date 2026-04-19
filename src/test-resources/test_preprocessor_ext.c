#define TEST_VALUE
#undef TEST_VALUE

#ifdef TEST_VALUE
#error "Undef failed"
#endif

#define TEST_VALUE

#ifdef UNDEFINED_SYM
#error "This should not happen"
#endif

int main() {
    int success = 0;
#line 200 "preproc_test.c"
    int line = __LINE__;
    if (line == 200) success++;
    
#ifdef TEST_VALUE
    success++;
#endif

    if (success == 2) return 0;
    return 1;
}
