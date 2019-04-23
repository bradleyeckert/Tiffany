#include <stdio.h>
#include "vm.h"

int main() {
    uint32_t PC = 0;
    VMpor();
    while (1) {
        uint32_t IR = FetchCell(PC);
        PC = VMstep(IR, 0) << 2;
    }
}
