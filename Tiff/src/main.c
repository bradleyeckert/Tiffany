#include <stdio.h>
#include "vm.h"
#include "tiff.h"
#include "accessvm.h"
#include "fileio.h"
#include <string.h>

int main(int argc, char *argv[]) {
    int Arg = 1;

// You can put quotes around the Forth command line text to allow spaces in it,
// but you will lose the quotes. Characters 147 and 148 (open and close quotes)
// of the ANSI code page 1252 are translated to ASCII quotes.

#ifdef TRACEABLE
    CreateTrace();                      // reset the trace buffer
#endif
    InitializeTermTCB();                // by default, ROM and SPI flash are blank (all '1's)
nextarg:
    while (argc>Arg) {                  // spin through the 2-character arguments
        if (strlen(argv[Arg]) == 2) {
            if (argv[Arg][0] == '-'){   // starts with a "-?" command
                char c = argv[Arg][1];  // get the command
                Arg++;
                switch (c) {
                    case 't':           // low level debugger with default blank memory
                        if (argc>Arg) { // attempt binary load into ROM
                            BinaryLoad(argv[Arg]);
                        }
                        vmTEST();  goto bye;
                    case 'b':           // load ROM and (optionally) SPI flash image from file
                        if (argc>Arg) {
                            if (BinaryLoad(argv[Arg++])) {
                                printf("Invalid or missing filename");
                                goto bye;
                            } else {    // successful binary load clears the load filename
                                DefaultFile = NULL;
                            }
                            goto nextarg;
                        } else {
                            printf("Use \"-b filename\"");
                            goto bye;
                        } break;
                    case 'f':           // change the load filename from the default `tiff.f`
                        if (argc>Arg) {
                            DefaultFile = argv[Arg++];
                            goto nextarg;
                        }
						printf("Use \"-f filename\"");
                        goto bye;
                    default:
                        printf("Unknown native command -%c", c);
                        goto bye;
                }
            } else goto go;
        } else goto go;
    }
go:
    if (argc>Arg) {
        tiffQUIT(argv[Arg]);            // command line is next argument
    } else {
        tiffQUIT(NULL);                 // empty command line
    }
bye:
#ifdef TRACEABLE
    DestroyTrace();                     // free the trace buffer
#endif
    return 0;
}
