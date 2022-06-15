#include <iostream>
#include "debug.h"

int main() {
    std::cout << "test two." << std::endl;
    printf("\e[31m");
    printf("hello world.\n");
    printf("\e[32m");
    printf("hello world.\n");
    printf("\e[0m");

    DebugError("dsfjsdfjjdsfa\n");
    printf("\e[31m");
    printf("dfjkasfj\n");
    printf("\e[0m");



    return 0;
}