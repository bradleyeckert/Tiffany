#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include "vmConsole.h"
#include "flash.h"

// To facilitate FPGA/ASIC implementation, console I/O uses a peripheral bus.
// There is no need for a 32-bit data bus, 16-bit is fine. Upper half is the address.
// Even addresses are reads, odd are writes (or write+read)

static uint32_t vmIO (uint32_t dout) {
    unsigned int address = dout >> 16;      // typically a 4-bit address
    uint32_t data = dout & 0xFFFF;          // and 16-bit data
    switch (address) {
        case 0: return vmQkey(data);        // 0: # of keyboard chars waiting in buffer
        case 1: return vmEmit(data);        // 1: write char to UART
        case 2: return vmKey(data);         // 2: read keyboard char from buffer
        case 3: return(SPIflashXfer(data)); // 3: SPI transfer
        case 4: return(vmKeyFormat(data));  // 4: keyboard cursor keys format {win32, xterm}
        case 6: return 1;                   // 6: EMIT buffer ready?
        case 8: return 0;                   // 8: flash busy?
        default: break;
    }
    return 0;
}

uint32_t vmUserParm = 0;        // global

/**
* Returns the current time in microseconds.
*/
static long getMicrotime(){
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);
    return currentTime.tv_sec * (int)1e6 + currentTime.tv_usec;
}

// SAM3 can use SLCK to clock a timer at 32768 Hz from either XTAL or internal RC.
// This seems like a good rate to standardize on. COUNTER increments at 32768 Hz.
// It rolls over every 36.4 hours.

static uint32_t Counter (uint32_t dummy) {
    return (uint32_t) (getMicrotime() * 0.032768);
}

static uint32_t Bye(uint32_t dummy) {
    exit(10);  return 0;
}

static uint32_t yo, divisor;

static uint32_t SetDiv (uint32_t parm) {
    divisor = parm;
    return yo;
}

static uint32_t Multiply (uint32_t parm) {
    uint64_t x = (uint64_t)parm * (uint64_t)vmUserParm;
    yo = x >> 32;
    return (x & 0xFFFFFFFF);
}

static uint32_t Divide (uint32_t parm) {
    uint64_t x = ((uint64_t)parm << 32) + vmUserParm;
    uint32_t quotient;
    if (parm < divisor) {
      quotient = x / divisor;
      yo = x % divisor;
    } else { // overflow
      quotient = ~0;
      yo = 0;
    }
    return quotient;
}

uint32_t UserFunction (uint32_t T, uint32_t N, int fn ) {
    vmUserParm = N;
    static uint32_t (* const pf[])(uint32_t) = {
        vmIO, Bye, Counter, SetDiv, Divide, Multiply
// add your own here...
    };
    if (fn < sizeof(pf) / sizeof(*pf)) {
        return pf[fn](T);
    } else {
        return 0;
    }
}
