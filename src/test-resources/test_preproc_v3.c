#define HEADER_FILE "test_preproc_v3.h"
#include HEADER_FILE
#include HEADER_FILE

#define LOG(fmt, ...) printf(fmt, ##__VA_ARGS__)

int main() {
    int success = 0;

    // 1. Check #pragma include_once
    once_func();
    success++;

    // 2. Check Variadic Macro
    LOG("test %d", 10);
    success++;

    // 3. Check Comma removal
    LOG("test");
    success++;

    if (success == 3) return 0;
    return 1;
}
