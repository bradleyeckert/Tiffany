#include <stdio.h>
#include "vm.h"

int main() {
    uint32_t PC = 0;
    VMpor();
    while (PC < 4096) {
        uint32_t IR = FetchROM(PC);
        printf("PC=%04Xh, IR=%08Xh\n", PC*4, IR);
        PC = VMstep(IR, 0);
    }
}
