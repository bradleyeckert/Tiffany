
#include <stdio.h>
#include <stdlib.h>
#include "vm.h"
#include "accessvm.h"
#include <string.h>
#include <time.h>

char * GetTime(void) {                  // get time/date string
    time_t current_time;
    char* c_time_string;
    current_time = time(NULL);              /* Obtain current time. */
    c_time_string = ctime(&current_time);   /* Convert to local time format. */
    return c_time_string;
}

// rom image may include all of memory space: ROM, RAM, and SPI flash
// When output, it's read into the rom image, then processed.

uint32_t * rom;

static int32_t ROMwords (uint32_t size) {     // read ROM image to local memory
    if (NULL == rom) {
        rom = (uint32_t*) malloc(MaxROMsize * sizeof(uint32_t));
    }
    uint32_t i;
    for (i=0; i<size; i++) {            // fill rom for local processing
        rom[i] = FetchCell(i*4);        // read through the VM's debug interface
    }
    i = size;
    while (--i) {
        if (rom[i] != 0xFFFFFFFF) break; // find the last non-blank word
    }
    return i+1;
}

// Copy from file to file while translating embedded macros to output text.
// The format for an embedded macro is `n` where n is a decimal number.
// If n is not a number, the macro is treated as plain text.
// This is used with template files.

void MakeFromTemplate(char *infile, char *outfile) {
    FILE *ifp;
    ifp = fopen(infile, "r");
    if (ifp == NULL) {
        tiffIOR = -199;                 // Can't open input file
        return;
    }
    FILE *ofp;
    ofp = fopen(outfile, "w");
    if (ofp == NULL) {
        tiffIOR = -198;                 // Can't create output file
        fclose(ifp);
        return;
    }
    uint32_t length;
    char buffer[256];
    int C_Columns;
    while(fgets(buffer, 255, (FILE*) ifp)) {
        char *s = buffer;
        char c;
        int seek = 0;
        char *rem = s;
        int n = 0;
        while ((c = *s++)) {
            if (seek) {                 // getting number
                if ((c >= '0') && (c <= '9')) {
                    n = n*10 + (c-'0');
                } else {
                    seek = 0;
                    if (c == '`') {     // found terminator
                        switch (n) {    // execute the macro function
case 0: fprintf(ofp, "\n");             // 0: newline
    break;
case 1: fprintf(ofp, "%s", GetTime());  // 1: time and date
    break;
case 2:                                 // 2: non-blank words in ROM
    fprintf(ofp, "%d", ROMwords(ROMsize));
    free(rom);  rom = NULL;
    break;
case 3: fprintf(ofp, "%d", ROMsize);    // 3: words in ROM space
    break;
case 4: fprintf(ofp, "%d", RAMsize);    // 4: words in RAM space
    break;
case 5: fprintf(ofp, "%d", SPIflashBlocks);  // 5: 4K sectors in SPI flash space
    break;
case 10:                                // 10: C syntax internal ROM dump
    C_Columns = 6;
    length = ROMwords(ROMsize);
    for (int i=0; i<length; i++) {
        if (i % C_Columns) {
            fprintf(ofp, " ");
        } else {
            fprintf(ofp, "\n/*%04X*/ ", i);
        }
        fprintf(ofp, "0x%08X", rom[i]);
        if (i != (length-1)) {
            fprintf(ofp, ",");
        }
    }
    free(rom);  rom = NULL;
    break;
case 11:                                // 11: Assembler syntax internal ROM dump
    length = ROMwords(ROMsize);
    char * directive = ".word";
    char * format = "0x%08X";
    C_Columns = 6;
//dumpasm:
    for (int i=0; i<length; i++) {
        int col = i % C_Columns;
        if (!col) {
            fprintf(ofp, "\n%s ", directive);
        }
        fprintf(ofp, format, rom[i]);
        if ((i != (length-1)) && (col != (C_Columns-1))) {
            fprintf(ofp, ",");
        }
    }
    fprintf(ofp, "\n");
    free(rom);  rom = NULL;
    break;
case 12:                                // 12: Assembler syntax for 8051
    length = ROMwords(ROMsize);
    directive = "DW";
    format = "0%04XH,0%04XH";           // no Keil support for 32-bit words
    C_Columns = 6;
    for (int i=0; i<length; i++) {
        int col = i % C_Columns;
        if (!col) {
            fprintf(ofp, "\n%s ", directive);
        }
        fprintf(ofp, format, rom[i]>>16, rom[i]&0xFFFF);
        if ((i != (length-1)) && (col != (C_Columns-1))) {
            fprintf(ofp, ", ");
        }
    }
    fprintf(ofp, "\n");
    free(rom);  rom = NULL;
    break;
case 13:                                // 13: VHDL syntax internal ROM dump
    length = ROMwords(ROMsize);
    for (int i=0; i<length; i++) {
        fprintf(ofp, "when %d => ROMdata <= x""%08X"";\n", i, rom[i]);
    }
    fprintf(ofp, "when others => ROMdata <= x""FFFFFFFF"";\n");
    free(rom);  rom = NULL;
    break;
case 20:                                // 20: C syntax stepping
    MakeTestVectors(ofp, PopNum(), 1);
    break;
case 21:                                // 21: VHDL syntax stepping
    MakeTestVectors(ofp, PopNum(), 2);
    break;

default: fprintf(ofp, "/*%d*/", n);
    break;
                        }
                    } else {            // bogus macro
                        while (rem != s) {
                            fputc(*rem++, ofp);
                        }
                    }
                }
            } else {
                if (c == '`') {
                    rem = s;            // might have to pass it later
                    seek = 1;
                    n = 0;
                } else fputc(c, ofp);   // not a macro, pass it through
            }
        }
    }
    fclose(ifp);
    fclose(ofp);
}

/*
The raw image includes all internal ROM, RAM, and SPI flash (ROM space)
Use the -b directive to load it instead of (or before) a Forth file.
*/

void SaveImage (char *filename) {
    WipeTIB();                          // don't need to see TIB contents
    int32_t length = ROMwords(SPIflashBlocks<<10); // end of ROM data
    FILE *ofp;
    ofp = fopen(filename, "wb");
    if (ofp == NULL) {
        tiffIOR = -198;                 // Can't create output file
        return;
    }
    // output length 32-bit rom/ram/etc words to binary file
    for (uint32_t i=0; i<length; i++) {
        uint32_t x = rom[i];
        for (int i = 0; i < 4; i++) {   // unpack little-endian
            fputc((uint8_t)(x >> (8 * i)), ofp);
        }
    }
    fclose(ofp);
}
