#include <stdio.h>
#include "vm.h"
#include "tiff.h"
#include <string.h>

int main(int argc, char *argv[]) {
    int i;  char *s;  char c;
    char argline[MaxTIBsize+1];		    // command line string: args separated by blanks
    int argidx;						    // the string and length

#ifdef __linux__
    initscr();
    cbreak();
    noecho();
    scrollok(stdscr, TRUE);
    nodelay(stdscr, TRUE);
#endif

// The command line string is built from the argvs. You can put quotes around an
// argument to keep its blanks, but then you will lose the quotes.
// Here, characters 147 and 148 (open and close quotes) of the ANSI code page
// 1252 are translated to ASCII quotes.

    argline[0] = 0;                     // default is empty line
    argidx = 0;							// capture commandline string:
    if (argc) {
        for(i=1; i<argc; i++) {
            s = argv[i];
            while ((argidx < MaxTIBsize) && (*s)) {
                c = *s++;
                if ((c==147) || (c==148))
                    c = '"';			// replace fancy quotes with plain quotes.
                argline[argidx++] = c;	// safely copy argument to string
            }
            if ((argidx < MaxTIBsize) && (i < (argc-1))) {
                argline[argidx++] =' ';	// separate by blanks
            }
            argline[argidx] = 0;		// zero-terminated string
        }
    }
    if (argc>1) {
        if (strlen(argv[1]) == 2) {
            if (argv[1][0] == '-') {    // starts with a "-?" command
                c = argv[1][1];
                switch (c) {
                case 't': vmTEST(); return 0;
                default:
                    printf("Unknown native command -%c", c);
                    return 0;
                }
            }
        }
    }
    vmTEST(); // enter test anyway
    tiffQUIT(argline);
    return 0;
}
