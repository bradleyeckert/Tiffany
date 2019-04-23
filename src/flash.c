#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "vm.h"
#include "flash.h"
//`0`#define ROMsize `3`
//`0`#define RAMsize `4`
//`0`#define SPIflashBlocks `5`
//`0`#define NOERRORMESSAGES
//`0`#define InhibitFlashSave 0
#define BASEADDR   (RAMsize+ROMsize)
#define FLASHCELLS ((SPIflashBlocks<<10) - BASEADDR)
#define FILENAME   "flash.bin"

/*
   Exports: FlashInit, SPIflashXfer, FlashRead, FlashWrite, FlashBye
   Addresses are VM byte addresses
*/

#if (FLASHCELLS < 0)
#error SPIflashBlocks is too small.
#endif

static uint32_t FlashMem[FLASHCELLS];

uint32_t FlashRead (uint32_t addr) {
    int32_t a = (addr >> 2) - BASEADDR;
    if (a < 0) {
        return -1;
    }
    if (a >= FLASHCELLS) {
        return -1;
    }
    return FlashMem[a];
};

FILE *fp;

// If FILENAME exists, load it into flash

void FlashInit (void) {
    memset(FlashMem, -1, FLASHCELLS*sizeof(uint32_t));
    fp = fopen(FILENAME, "rb");
    if (fp) {
        for (int i=0; i<FLASHCELLS; i++) {
            int n = fread(&FlashMem[i], sizeof(uint32_t), 1, fp);
            if (n != 4) break;
        }
        fclose(fp);
    }
};

// Save flash image to filename, creating if necessary

void FlashBye (void) {
    int p = FLASHCELLS;
    if (InhibitFlashSave) return;
    while ((p) && (0xFFFFFFFF == FlashMem[--p])) {}
    if (!p) return;             // nothing to save
    fp = fopen(FILENAME, "wb");
    if (fp) {  // save non-blank to file
        for (int i=0; i<p; i++) {
            fwrite(&FlashMem[i], sizeof(uint32_t), 1, fp);
        }
        fclose(fp);
    }
};

// ---------------------
// old  new  pgm  error
//   0    0    -    yes
//   0    1    0     no
//   1    0    0     no
//   1    1    1     no

int FlashWrite (uint32_t x, uint32_t addr) {
    int32_t a = (addr >> 2) - BASEADDR;
    if (a < 0) {
        return -9;
    }
    if (a >= FLASHCELLS) {
        return -9;
    }
    uint32_t old = FlashMem[a];
    if (~(old|x)) {
#ifndef NOERRORMESSAGES
        printf("\nFlash not erased: Addr=%X, old=%X, new=%X ", addr, old, x);
#endif // NOERRORMESSAGES
        return -60;              	// not erased
    }
    FlashMem[a] = old & x;
    return 0;
};

/*
| Name   | Hex | Command                           |
|:-------|-----|----------------------------------:|
| FR     | 0Bh | Read Data Bytes from Memory       |
| PP     | 02h | Page Program Data Bytes to Memory |
| SER    | 20h | Sector Erase 4KB                  |
| WREN   | 06h | Write Enable                      |
| WRDI   | 04h | Write Disable                     |
| RDSR   | 05h | Read Status Register              |
| RDJDID | 9Fh | Read 3-byte JEDEC ID              |
*/

static int Erase4K(uint32_t address) {
	uint32_t addr = address / 4;
	if (address & 0xFFF) return -23;    // alignment problem
	for (int i=0; i<1024; i++) {        // erase 4KB sector
		int ior = FlashWrite(-1, addr);
		addr += 4;
		if (ior) return ior;
	}
	return 0;
}

// Use SPI transfer (user function 5) to write to the ROM space.
// This simulates SPI flash.

static uint8_t state = 0;				// FSM state
static uint8_t command = 0;				// current command
static uint8_t wen;						// write enable
static uint32_t addr;

// n bits: 11:10 = bus width (ignored, assumed 0)
// 9 = falling starts a command if it's not yet started
// 8 = finishes a command if it's not yet finished
// 7:0 = SPI data to transmit
// Return value if the SPI return byte

// RDJDID (9F command) is custom: 0xAA, 0xHH, 0xFF number of 4K blocks

uint32_t SPIflashXfer (uint32_t n) {    /*EXPORT*/
	uint8_t cin = (uint8_t)(n & 0xFF);
	uint8_t cout = 0xFF;
	uint32_t word, data;
//	printf("%02X ", n);
	if (n & 0x200) {                 					// inactive bus
		state = 0;
		return cout;                    				// is floating hi
	} else {
		if (state) {                    				// continue previous command
			int shift = 8*(addr&3);
			switch (state) {
				case 1: break;							// wait for trailing CS
				case 2: cout = wen;   state=1;  break;  // status = WEN, never busy
				case 3: cout = 0xAA;  state++;  break;	// 3-byte RDJDID
				case 4: cout = 0xFF & (SPIflashBlocks>>8);  state++;  break;
				case 5: cout = 0xFF & SPIflashBlocks;       state=1;  break;
				case 6: addr = cin<<16;  					state++;  break;
				case 7: addr += cin<<8;  					state++;  break;
				case 8: addr += cin;
                    if (addr < (SPIflashBlocks<<12)) {
                        switch (command) {
                            case 0x20: if (wen) tiffIOR = Erase4K(addr); // erase sector
                                wen=0; /* 4K erase */		state=1;  break;
                            case 0x0B: /* fast read */		state++;  break;
                            case 0x02: /* page write */		state=11; break;
                            default: 					    state = 0;
                        } break;
                    } else {                            // invalid address, ignore
                        state = 0;
                    }
				case 9:	state++;  break;				// dummy byte before read
				case 10:								// read as long as you want
					word = FlashRead(addr);
					cout = (uint8_t)(word >> shift);
					addr++;  break;
				case 11:								// write byte to flash
					data = ~((~cin & 0xFF) << shift);
					FlashWrite(data, addr);
					addr++;
					if (((addr & 0xFF) == 0) && ((n & 0x100) == 0)) {
						tiffIOR = -60;              	// page overflow
#ifndef NOERRORMESSAGES
						printf("\nFlash Page Programming Overflow: Addr=%X ", addr);
#endif // NOERRORMESSAGES
						state=0;
					}
					break;
				default: state = 0;
			}
		} else {                        // start new command
			command = cin;
			switch (command) {
				case 0x05: /* RDSR   opcd -- n1 */  				state=2;  break;
				case 0x0B: /* FR     opcd A2 A1 A0 xx -- data... */
				case 0x02: /* PP     opcd A2 A1 A0 d0 d1 d2 ... */
				case 0x20: /* SER4K  opcd A2 A1 A0 */  				state=6;  break;
				case 0x06: /* WREN   opcd */  						state=1;  wen=2;  break;
				case 0x04: /* WRDI   opcd */  						state=1;  wen=0;  break;
				case 0x9F: /* RDJDID opcd -- n1 n2 n3 */  			state=3;  break;
				default: break;
			}
		}
	}
	if (n & 0x100) {                  	// inactive bus
		state = 0;
	}
	return cout;
}
