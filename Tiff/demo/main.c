#include <stdio.h>
#include "vm.h"

#define LIMIT 2000

int main() {
    uint32_t PC = 0;
    int tally=0;
    VMpor();
    while ((PC < 4096) && (tally < LIMIT)) {
        uint32_t IR = FetchROM(PC);
//      printf("%d: PC=%04Xh, IR=%08Xh\n", tally, PC*4, IR);
        PC = VMstep(IR, 0);
        tally++;
    }
//  printf("Terminated after %d instructions groups", tally);
}
