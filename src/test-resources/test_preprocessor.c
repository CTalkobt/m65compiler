#define _TEST_C_
#ifdef _TEST_C_
int preproc_success() {
    return 1;
}
#else
int preproc_failure() {
    return 0;
}
#endif

#!ifdef _ASSEMBLER_SYMBOL_
void assembler_symbol_active() {
    // This will be inside #ifdef in the .s file
    return;
}
#!else
void assembler_symbol_inactive() {
    // This will be inside #else in the .s file
    return;
}
#!endif

#include "test_inc.h"

int main() {
    return preproc_success() + included_func();
}
