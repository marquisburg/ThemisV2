#include <iostream>
#include "defs.h"

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    InitAll();
    UciLoop();
    return 0;
}
