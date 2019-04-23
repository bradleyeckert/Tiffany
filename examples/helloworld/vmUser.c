#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include "vmConsole.h"
#include "flash.h"

uint32_t vmUserParm = 0;        // global

/**
* Returns the current time in microseconds.
*/
static long getMicrotime(){
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);
    return currentTime.tv_sec * (int)1e6 + currentTime.tv_usec;
}

static uint32_t tiffBye(uint32_t dummy) {
    exit(10);  return 0;
}

static uint32_t Counter (uint32_t dummy) {
    return (uint32_t) getMicrotime() / 100;
}

uint32_t UserFunction (uint32_t T, uint32_t N, int fn ) {
    vmUserParm = N;
    static uint32_t (* const pf[])(uint32_t) = {
        vmQkey, vmKey, vmEmit, vmQemit,
        Counter, SPIflashXfer, tiffBye, vmKeyFormat
// add your own here...
    };
    if (fn < sizeof(pf) / sizeof(*pf)) {
        return pf[fn](T);
    } else {
        return 0;
    }
}
