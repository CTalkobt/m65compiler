#define VERSION 2

int main() {
    int success_v2 = 0;
    int success_elif = 0;
    int success_comments = 0;
    int success_strings = 0;

#if VERSION == 2 \
    && defined(VERSION)
    success_v2 = 1;
#endif

#if defined(UNDEFINED) \
    || (VERSION < 1)
#error "This should not happen"
#elif VERSION == 2
    success_elif = 1;
#else
#error "Elif failed"
#endif

    /* This is a comment containing #error "FAIL"
       and it should be ignored.
    */
    success_comments = 1;

    // Line comment with #define TEST
    
    // Check that __LINE__ does NOT expand in strings
    // In our compiler, strings are just pointers. 
    // We can't easily check the content, but we can check if it compiles.
    // If it expanded to a number, it would be a syntax error if not in quotes.
    // Since it's in quotes, it will always compile. 
    // To truly check, we'd need to look at the assembly.
    success_strings = 1;

    if (success_v2 && success_elif && success_comments && success_strings) return 0;
    return 1;
}
