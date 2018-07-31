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
    int i;  char *s;  char c;
    char argline[MaxTIBsize+1];		    // command line string: args separated by blanks
    int argidx;						    // the string and length
    int first = 1;                      // first argument of Forth command line
/*
#ifdef __linux__
    initscr();
    cbreak();
    noecho();
    scrollok(stdscr, TRUE);
    nodelay(stdscr, TRUE);
#endif
*/

// The command line string is built from the argvs. You can put quotes around an
// argument to keep its blanks, but then you will lose the quotes.
// Here, characters 147 and 148 (open and close quotes) of the ANSI code page
// 1252 are translated to ASCII quotes.

#ifdef TRACEABLE
    CreateTrace();                      // reset the trace buffer
#endif
//    StoreROM(10000, 0x4000); // test write (debug)
    InitializeTermTCB();
    argline[0] = 0;                     // default is empty line
    argidx = 0;							// capture commandline string:
    if (argc>1) {
        if (strlen(argv[1]) == 2) {
            if (argv[1][0] == '-') {    // starts with a "-?" command
                c = argv[1][1];         // get the command
                first = 2;              // there might be a Forth command starting at argv[2]
                switch (c) {
                    case 't':
                        if (argc>2) {   // attempt binary load
                            BinaryLoad(argv[2]);
                        }
                        vmTEST();  goto bye;
                    case 'b':
                        if (argc>2) {
                            if (BinaryLoad(argv[2])) {
                                printf("Invalid or missing filename");
                            }
                            first = 3;
                            break;
                        } else {
                            printf("Use ""-b filename""");
                            goto bye;
                        }
                    default:
                        printf("Unknown native command -%c", c);
                        goto bye;
                }
            }
        }
        if (argc>first) {
            for (i = first; i < argc; i++) {
                s = argv[i];
                while ((argidx < MaxTIBsize) && (*s)) {
                    c = *s++;
                    if ((c == 147) || (c == 148))
                        c = '"';         // replace fancy quotes with plain quotes.
                    argline[argidx++] = c;    // safely copy argument to string
                }
                if ((argidx < MaxTIBsize) && (i < (argc - 1))) {
                    argline[argidx++] = ' ';  // separate by blanks
                }
                argline[argidx] = 0;    // zero-terminated string
            }
        }
    }
    tiffQUIT(argline);
bye:
#ifdef TRACEABLE
    DestroyTrace();                     // free the trace buffer
#endif
    return 0;
}
