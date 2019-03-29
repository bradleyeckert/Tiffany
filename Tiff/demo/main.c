#include <stdio.h>
#include "vm.h"
//#include <string.h>


int main() {
    uint32_t DbgPC = 0;
    VMpor();
    while (DbgPC < 4096){
        uint32_t ir = FetchROM(DbgPC);
//        printf("PC=%X, IR=%08X\n", DbgPC, ir);
        DbgPC = VMstep(ir, 0);
    }
}
