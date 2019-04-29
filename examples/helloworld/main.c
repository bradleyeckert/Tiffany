#include <stdio.h>
#include "vm.h"

uint32_t FetchROM(uint32_t addr);

int main() {
    uint32_t PC = 0;
    VMpor();
    while (1) {
        uint32_t IR = FetchROM(PC);
        PC = VMstep(IR, 0);
    }
}
