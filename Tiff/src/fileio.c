#include <stdio.h>
#include <stdlib.h>
#include "vm.h"
#include "vmaccess.h"
#include <string.h>

// Load a ROM image file

char* LoadedFilename;                   // saved filename and format
int LoadedFileType;

int BinaryLoad (char* filename) {       // Load ROM from binary file
    FILE *fp;
    uint8_t data[4];
    int length, i;
    uint32_t n;
    uint32_t addr = 0;
    fp = fopen(filename, "rb");
    if (!fp) { return -1; }             // bad filename
    LoadedFilename = filename;          // save in case we want to reload the file
    LoadedFileType = 1;
    do {
        memset(data, 255, 4);
        length = fread(data, 1, 4, fp); // get 4 bytes of data
        n = 0;
        for (i = 0; i < 4; i++) {       // make little-endian word
            n += data[i] << (8 * i);
        }
        WriteROM(n, addr);              // ignore ior
        addr += 4;
    } while (length == 4);
    fclose(fp);
    return 0;
}

void ReloadFile (void) {                // Reload known ROM image file
    switch(LoadedFileType) {
        case 1: BinaryLoad(LoadedFilename);
        default: break;
    }
}

uint32_t ROMwords (uint32_t *rom) {     // read ROM image to local memory
    uint32_t i;
    for (i=0; i<ROMsize; i++) {         // fill rom for local processing
        rom[i] = FetchCell(i);          // read through the VM's debug interface
    }
    i = ROMsize;
    while (--i) {
        if (rom[i]) break;              // find the last non-zero word
    }
    return i+1;
}

void SaveROMasC (char *filename) {      // save ROM in C format
    uint32_t *rom = malloc(ROMsize);
    uint32_t length = ROMwords(rom);

    printf("#include <stdint.h>\n");
    free(rom);
}

