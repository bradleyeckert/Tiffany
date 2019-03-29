#include <stdio.h>
#include "vm.h"

int main() {
    uint32_t PC = 0;
    VMpor();
    while (PC < 4096) {
        uint32_t IR = FetchROM(PC);
        PC = VMstep(IR, 0);
    }

//    VMrun(0);
//    VMrun(10000);
}
