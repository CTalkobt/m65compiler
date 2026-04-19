#define INC_FILE "test_inc_macros.h"
#include INC_FILE
#include INC_FILE // Should be ignored due to #pragma include_once

#define STR(x) #x
#define GLUE(a, b) a##b

#define ADD(a, b) ((a) + (b))

int main() {
    int success = 0;
    
    // Test #pragma include_once (indirectly by checking if HEADER_INCLUDED exists)
#ifdef HEADER_INCLUDED
    success++;
#endif
    
    // Test function-like macro
    if (ADD(10, 20) == 30) success++;
    
    // Test ## (pasting)
    int GLUE(my, var) = 5;
    if (myvar == 5) success++;
    
    // Test # (stringification)
    char* s = STR(hello);

    if (success == 3) return 0;
    return 1;
}
