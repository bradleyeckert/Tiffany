#include <stdio.h>
#include "vm.h"

int main() {
    uint32_t PC = 0;
//    uint32_t tally=0;
    VMpor();
    while (PC < 32768) {
        uint32_t IR = FetchROM(PC);
        PC = VMstep(IR, 0);
//        tally++;
    }
//    printf("Terminated after %d instructions groups", tally);
}
