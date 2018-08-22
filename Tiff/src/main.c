#include <stdio.h>
#include "vm.h"
#include "tiff.h"
#include "vmaccess.h"
#include "fileio.h"
#include <string.h>

#ifdef __linux__
#include <ncurses.h>
#endif

int main(int argc, char *argv[]) {
    int Arg = 1;

/*
#ifdef __linux__
    initscr();
    cbreak();
    noecho();
    scrollok(stdscr, TRUE);
    nodelay(stdscr, TRUE);
#endif
*/

// You can put quotes around the Forth command line text to allow spaces in it,
// but you will lose the quotes. Characters 147 and 148 (open and close quotes)
// of the ANSI code page 1252 are translated to ASCII quotes.

#ifdef TRACEABLE
    CreateTrace();                      // reset the trace buffer
#endif
    InitializeTermTCB();
    while (argc>Arg) {                  // spin through the 2-character arguments
        if (strlen(argv[Arg]) == 2) {
            if (argv[Arg][0] == '-'){   // starts with a "-?" command
                char c = argv[Arg][1];  // get the command
                Arg++;
                switch (c) {
                    case 't':
                        if (argc>Arg) { // attempt binary load
                            BinaryLoad(argv[Arg]);
                        }
                        vmTEST();  goto bye;
                    case 'b':
                        if (argc>Arg) {
                            if (BinaryLoad(argv[Arg++])) {
                                printf("Invalid or missing filename");
                            }
                            break;
                        } else {
                            printf("Use \"-b filename\"");
                            goto bye;
                        }
                    case 'f':
                        if (argc>Arg) {
                            DefaultFile = argv[Arg++];
                            break;
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
        tiffQUIT(argv[Arg]);
    } else {
        tiffQUIT(NULL);
    }
bye:
#ifdef TRACEABLE
    DestroyTrace();                     // free the trace buffer
#endif
    return 0;
}
