#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "vm.h"

/* Imports: ROM[]
   Exports: SPIflashXfer
*/

//
#define SPIflashSize 65536
//
extern uint32_t ROM[SPIflashSize];

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
	int i;
	if (address & 3) return -23;        // alignment problem
	if (addr > (SPIflashSize-1024)) return -9;   // out of range
	for (i=0; i<1024; i++) {            // erase 4KB sector
		ROM[addr+i] = 0xFFFFFFFF;
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

uint32_t SPIflashXfer (uint32_t n, uint32_t dummy) {    /*EXPORT*/
	uint8_t cin = (uint8_t)(n & 0xFF);
	uint8_t cout = 0xFF;
	int shift;
	uint32_t word, data, mask;
//	printf("%02X ", n);
	if (n & 0x200) {                 					// inactive bus
		state = 0;
		return cout;                    				// is floating hi
	} else {
		if (state) {                    				// continue previous command
			shift = 8*(addr&3);
			switch (state) {
				case 1: break;							// wait for trailing CS
				case 2: cout = wen;   state=1;  break;  // status = WEN, never busy
				case 3: cout = 0xAA;  state++;  break;	// 3-byte RDJDID
				case 4: cout = 0xFF & (SPIflashSize >> 18); state++;  break;
				case 5: cout = 0xFF & (SPIflashSize >> 10); state=1;  break;
				case 6: addr = cin<<16;  					state++;  break;
				case 7: addr += cin<<8;  					state++;  break;
				case 8: addr += cin;
                    if (addr < SPIflashSize*4) {
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
					word = ROM[addr/4];
					cout = (uint8_t)(word >> shift);
					addr++;  break;
				case 11:								// write byte to flash
					word = ROM[addr/4];
					mask = ~(0xFF << shift);
					data = mask | (cin << shift);
					if (~(word|data)) {
						tiffIOR = -60;              	// not erased
#ifdef ERRORMESSAGES
						printf("\nFlash not erased: Addr=%X, old=%X, new=%X ", addr, word, data);
#endif // ERRORMESSAGES
						state=0;
						break;
					}
					ROM[addr/4] = word & data;
					addr++;
					if (((addr & 0xFF) == 0) && ((n & 0x100) == 0)) {
						tiffIOR = -60;              	// page overflow
#ifdef ERRORMESSAGES
						printf("\nFlash Page Overflow: Addr=%X ", addr);
#endif // ERRORMESSAGES
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
