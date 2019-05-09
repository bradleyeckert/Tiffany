#include <stdio.h>
#include <stdint.h>

// This version is "No Flash Present"

uint32_t FlashRead (uint32_t addr) {
    return 0;
};

void FlashInit (char * s) {
};

void FlashBye (char * s) {
};

int FlashWrite (uint32_t x, uint32_t addr) {
    return 0;
};

// your code can test for flash with this

uint32_t SPIflashXfer (uint32_t n) {
    return -1;
}
