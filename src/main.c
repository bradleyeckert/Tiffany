#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "vm.h"
#include "flash.h"
#include "tiff.h"
#include "accessvm.h"
#include "fileio.h"
#include <string.h>
#include "vmhost.h"
#define HP0max  (MaxROMsize - 0x1000)

/*global*/ int HeadPointerOrigin = (ROMsizeDefault + RAMsizeDefault)*4;
/*global*/ char * LoadFlashFilename = NULL;
static     char * SaveFlashFilename = NULL;
static     char * BootFilename = NULL;
static     int  testmode = 0;

void TidyUp (void) {                    // stuff to do at exit
    ROMbye();
    FlashBye(SaveFlashFilename);
#ifdef TRACEABLE
    DestroyTrace();                     // free the trace buffer
#endif
}

uint32_t Number (char * s, uint32_t limit, char cmd) {
    char *eptr;
    long long int x = strtoll(s, &eptr, 0);
    if ((x == 0) && ((errno == EINVAL) || (errno == ERANGE))) {
nan:    fprintf(stderr,"Malformed number for -%c\n", cmd);
        exit(1);
    } else {
        if (*eptr) goto nan;
    }
    if (x < limit) {
        return x;
    }
    fprintf(stderr,"Argument out of range for -%c, max is 0x%X\n", cmd, limit);
    exit(2);
}

int main(int argc, char *argv[]) {
    int Arg = 1;

// You can put quotes around the Forth command line text to allow spaces in it,
// but you will lose the quotes. Characters 147 and 148 (open and close quotes)
// of the ANSI code page 1252 are translated to ASCII quotes.

#ifdef TRACEABLE
    CreateTrace();                      // reset the trace buffer
#endif
    vmMEMinit(NULL);
    vmHostInit();
    atexit(TidyUp);

nextarg:
    while (argc>Arg) {                  // spin through the 2-character arguments
        if ((strlen(argv[Arg]) == 2) && (argv[Arg][0] == '-')) {   // starts with a "-?" command
            char c = argv[Arg][1];      // get the command
            Arg++;
            switch (c) {
                case 'f':               // change the load filename from the default `mf.f`
                    if (argc == Arg) goto splain;
                    DefaultFile = argv[Arg++];
                    goto nextarg;
                case 'h':
                    if (argc == Arg) goto splain;
                    HeadPointerOrigin = Number(argv[Arg++], HP0max, 'h');
                    goto nextarg;
                case 'r':
                    if (argc == Arg) goto splain;
                    RAMsize = Number(argv[Arg++], MaxRAMsize, 'r');
                    if (RAMsize & (RAMsize-1)) {
                        printf("RAM size must be an exact power of 2.\n");
                        // because of VM sandboxing. See SDUP and friends.
                        goto bye;
                    }
                    goto nextarg;
                case 'm':
                    if (argc == Arg) goto splain;
                    ROMsize = Number(argv[Arg++], MaxROMsize, 'm');
                    goto nextarg;
                case 's':
                    if (argc == Arg) goto splain;
                    StackSpace = Number(argv[Arg++], RAMsize/2, 'r');
                    goto nextarg;
                case 'b':
                    if (argc == Arg) goto splain;
                    SPIflashBlocks = Number(argv[Arg++], MaxFlashCells >> 10, 's');
                    goto nextarg;
                case 'i':
                    if (argc == Arg) goto splain;
                    LoadFlashFilename = argv[Arg++];
                    goto nextarg;
                case 'o':
                    if (argc == Arg) goto splain;
                    SaveFlashFilename = argv[Arg++];
                    goto nextarg;
                case 'c':
                    if (argc == Arg) BootFilename = "mf.hex";
                    else             BootFilename = argv[Arg++];
                    goto nextarg;
                case 't':
                    testmode = 1;
                    goto nextarg;
                case 'T':
                    InitializeTermTCB();            // Test the basics
//                    vmTEST();
                    goto bye;
                default:
splain:             printf("Format: [cmds] [\"Forth Command Line\"]\n");
                    printf("cmds are optional 2-char commands starting with '-':\n");
                    printf("-f <filename>  Change startup INCLUDE filename from {%s}\n", DefaultFile);
                    printf("-h <addr>      Change header start address from {0x%X}\n",   HeadPointerOrigin);
                    printf("-r <n>         Change RAM size from {0x%X} cells\n",         RAMsizeDefault);
                    printf("-m <n>         Change ROM size from {0x%X} cells\n",         ROMsizeDefault);
                    printf("-s <n>         Change stack region from {0x%X} cells\n",     StackSpace);
                    printf("-b <n>         Change SPI flash 4k block count from {%d}\n", FlashBlksDefault);
                    printf("-i <filename>  Initialize flash image from file\n");
                    printf("-o <filename>  Save flash image upon exit\n");
                    printf("-c [filename]  Hex file for cold booting (note save-hex)\n");
                    printf("-t             Enable test mode if cold booting\n");
                    goto bye;
            }
        } else goto go;                 // exhausted options, there could be a Forth command line remaining
    }
go:
    if (BootFilename) {
        vmMEMinit(NULL);                // clear ROM and flash
        LoadHexImage(BootFilename);     // load ROM and flash from Hex file
        if (testmode) {
            vmTEST();                   // optional startup with debugger dashboard
        }
        iword_COLD();                   // cold boot
    } else {
        InitializeTermTCB();            // by default, ROM and SPI flash are blank (all '1's)
        if (argc>Arg) {
            tiffQUIT(argv[Arg]);        // command line is next argument
        } else {
            tiffQUIT(NULL);             // empty command line
        }
    }
bye:
    return 0;
}
