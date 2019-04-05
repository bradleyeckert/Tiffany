
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

uint32_t rom[ROMsize+SPIflashSize];

int32_t ROMwords (uint32_t size) {     // read ROM image to local memory
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
                        switch (n) {        // execute the macro function
case 0: fprintf(ofp, "\n");             // 0: newline
    break;
case 1: fprintf(ofp, "%s", GetTime());  // 1: time and date
    break;
case 2:                                 // 2: non-blank words in ROM
    fprintf(ofp, "%d", ROMwords(ROMsize));
    break;
case 3: fprintf(ofp, "%d", ROMsize);    // 3: words in ROM space
    break;
case 4: fprintf(ofp, "%d", RAMsize);    // 4: words in RAM space
    break;
case 5:                                 // 5: C syntax internal ROM dump
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
    break;
case 6:                                 // 6: Assembler syntax internal ROM dump
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
    break;
case 7:                                 // 7: Assembler syntax for 8051
    length = ROMwords(ROMsize);
    directive = "DW";
    format = "0%04XH,0%04XH";             // no support for 32-bit words
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
    break;
case 8:                                 // 8: VHDL syntax internal ROM dump
    length = ROMwords(ROMsize);
    for (int i=0; i<length; i++) {
        fprintf(ofp, "when %d => ROMdata <= x""%08X"";\n", i, rom[i]);
    }
    fprintf(ofp, "when others => ROMdata <= x""FFFFFFFF"";\n");
    break;
case 10:                                // 10: C syntax stepping
    MakeTestVectors(ofp, PopNum(), 1);
    break;
case 11:                                // 10: VHDL syntax stepping
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

// Alignment must be a power of two.
// The array will be padded to be sized as a multiple of alignment in cells.

/*
SPI ROM is either simulated, as in the case of a PC app, or in MCU hardware.
In the latter case, the MCU can keep it internal or external.
If it's internal, you would place a static array with linker file tricks.
If it's external, you need a way to program it into SPI flash in production.
*/


void SaveAXIasC (char *filename, int alignment) {      // save Flash in C format
    int32_t length = ROMwords(SPIflashSize); // end of AXI data
            length = (length + alignment - 1) & (-alignment);
    int32_t i;  int32_t offset;
    FILE *ofp;
    ofp = fopen(filename, "w");
    if (ofp == NULL) {
        tiffIOR = -198;                 // Can't create output file
        return;
    }
    // Generate C text
    fprintf(ofp, "#include <stdint.h>\n");
#ifdef BootFromSPI
    fprintf(ofp, "#define FlashSize %d\n", length);
    fprintf(ofp, "#define FlashOrigin 0 /* Boot+Headers */\n");
    offset = 0;
#else
    fprintf(ofp, "#define FlashSize %d\n", length);
    fprintf(ofp, "#define FlashOrigin 0x%X /* Headers */\n", HeadPointerOrigin);
    offset = HeadPointerOrigin / 4;
#endif
    fprintf(ofp, "#define HeaderOrigin 0x%X\n\n", HeadPointerOrigin);
    fprintf(ofp, "// SPI flash image, padded to 4KB sector size\n");
    fprintf(ofp, "// Generated by tiff.exe, save-flash, %s\n", GetTime());
    fprintf(ofp, "static uint32_t FlashData[] = {");

    for (i=offset; i<length; i++) {
        if (i % 6) {
            fprintf(ofp, " ");
        } else {
            fprintf(ofp, "\n");
        }
        fprintf(ofp, "0x%08X", rom[i]);
        if (i != (length-1)) {
            fprintf(ofp, ",");
        }
    }
    fprintf(ofp, "};\n");
    fclose(ofp);
}
