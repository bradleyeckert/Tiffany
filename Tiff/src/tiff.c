#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "vm.h"
#include "tiff.h"
#include "vmaccess.h"
#include <string.h>
#include <ctype.h>
#define MaxKeywords 50

int tiffIOR = 0;                        // Interpret error detection when not 0

/// Primordial brain keyword support for host functions.
/// These are interpreted after the word is not found in the Forth dictionary.
/// They prevent dead heads from being put into the dictionary.

void tiffCOMMENT (void) {
    StoreCell(FetchCell(TIBS), TOIN);   // ignore the rest of the TIB
}
void tiffBYE (void) {
    tiffIOR = -99999;                   // exit Forth
    tiffCOMMENT();
}
void Hello (void) {                     // test for "hi"
    printf("\nHello World");
}
void tiffDOT (void) {                   // pop and print
    printf("%d ", PopNum());
}
// void tiffHEX (void) {                   // base 16
//     StoreCell(16, BASE);
// }
// void tiffDECIMAL (void) {               // base 16
//     StoreCell(10, BASE);
// }

int keywords = 0;                       // # of keywords added at startup
typedef void (*VoidFn)();

struct Keyword {                        // host keywords only have
    char  name[24];                     // a name and
    VoidFn Function;                    // a C function
};
struct Keyword HostWord[MaxKeywords];

void AddKeyword(char *name, void (*func)()) {
    if (keywords<MaxKeywords){          // ignore new keywords if full
        strcpy (HostWord[keywords].name, name);
        HostWord[keywords++].Function = func;
    }
}

void LoadKeywords(void) {               // populate the list of gator brain functions
    keywords = 0;                       // start empty
    AddKeyword("bye", tiffBYE);
    AddKeyword("\\", tiffCOMMENT);
    AddKeyword(".", tiffDOT);
//    AddKeyword("hex", tiffHEX);
//    AddKeyword("decimal", tiffDECIMAL);
    AddKeyword("hi", Hello);
}

int NotKeyword (char *key) {            // do a command, return 0 if found
    int i = 0;
    for (i=0; i<keywords; i++) {        // scan the list
        if (strcmp(key, HostWord[i].name) == 0) {
            HostWord[i].Function();
            return 0;
        }
    }
    return -1;
}

////////////////////////////////////////////////////////////////////////////////
/// tiffPARSE gets the next <delimiter> delimited string from TIBB.
/// tiffPARSENAME gets the next blank delimited token.
/// Both leave their parameters on the stack for compatibility with the Forth
/// versions that will replace them.

uint32_t TIBhere (void) {               // get the current TIB address
    return (FetchCell(TIBB) + FetchCell(TOIN));
}
uint8_t TIBchar (void) {                // get the current TIB byte
    uint32_t tibb = FetchCell(TIBB);
    uint32_t toin = FetchCell(TOIN);
    return FetchByte(tibb + toin);
}
int TIBnotExhausted (void) {            // test for input not exhausted
    uint32_t tibs = FetchCell(TIBS);
    uint32_t toin = FetchCell(TOIN);
    if (TIBchar()==0) return 0;         // terminator found early
    if (toin < tibs) return 1;
    return 0;
}
void TOINbump (void) {                  // point to next TIB byte
    uint32_t toin = FetchCell(TOIN);
    StoreCell(toin+1, TOIN);
}
void SkipWhite (void){                  // skip whitespsce (blanks)
    while (TIBnotExhausted()) {
        if (TIBchar() != ' ') break;
        TOINbump();
    }
}
// Parse without skipping delimiter, return string on stack
uint32_t tiffPARSE (void) {             // ( delimiter -- addr length )
    uint32_t length = 0;                // returns a string
    uint8_t delimiter = (uint8_t) PopNum();
    PushNum(TIBhere());
    while (TIBnotExhausted()) {
        if (TIBchar() == delimiter) break;
        TOINbump();                     // get string
        if (length==31) break;          // token size limit
        length++;
    }
    PushNum(length);
    return length;
}
// Parse skipping blanks, then parsing for the next blank
uint32_t tiffPARSENAME (void) {         // ( -- addr length )
    SkipWhite();
    PushNum(' ');
    return tiffPARSE();
}
// Interpret the TIB inside the VM, on the VM data stack.
void tiffINTERPRET(void) {
    char token[33];  char *eptr;
    uint32_t address, length;
    long int x;
    while (tiffPARSENAME()) {           // get the next blank delimited keyword
        // dictionary search using ROM head space not implemented yet.
        // Assume it falls through with ( c-addr len ) on the stack.
        length = PopNum();
        if (length>31) length=32;       // sanity check
        address = PopNum();
        vmRAMfetchStr(token, address, (uint8_t)length);
        if (NotKeyword(token)) {        // try to convert to number
            x = (int32_t) strtol(token, &eptr, 0); // automatic base (C format)
            if ((x == 0) && ((errno == EINVAL) || (errno == ERANGE))) {
                strcpy(ErrorString, token); // not a number
                tiffIOR = -13;  tiffCOMMENT();
            } else {
                PushNum((uint32_t)x);
            }
        }
    }
    PopNum();  PopNum();                // keyword is an empty string
}

// Keyboard input uses the default terminal cooked mode.
// Since getline includes the trailing LF (or CR), we lop it off.

void tiffQUIT (char *cmdline) {
    char argline[MaxTIBsize+1];
    size_t bufsize = MaxTIBsize;            // for getline
    char *buf;                              // text from stdin (keyboard)
    int length, source, TIBaddr;
    LoadKeywords();
    while (1){
        InitializeTIB();
        StoreCell(0, SOURCEID);     	    // input is keyboard
        StoreCell(0, STATE);     	        // interpret
        do {
            StoreCell(0, TOIN);				// >IN = 0
            StoreCell(0, TIBS);
            source = FetchCell(SOURCEID);
            TIBaddr = FetchCell(TIBB);
            switch (source) {
                case 0:                     // load TIBB from keyboard
                    if (*cmdline) {         // first time through use command line
                        strcpy (argline, cmdline);
                        length = strlen(cmdline);
                        cmdline = NULL;     // clear cmdline
                    } else {
                        buf = argline;
                        length = getline(&buf, &bufsize, stdin);   // get input line
                        printf("\033[A\033[%dC", strlen(argline)); // undo the newline
                        if (length > 0) length--;   // remove LF or CR
                    }
                    StoreCell((uint32_t)length, TIBS);
                    vmRAMstoreStr(argline, (uint32_t)TIBaddr);
                    break;
                case 1: // load TIBB using getline(&buffer, &buffer_size, file)
                    break;
                default:	// unexpected
                    StoreCell(0, SOURCEID);
            }
            tiffINTERPRET(); // interpret the TIBB until it?s exhausted
            switch (FetchCell(SOURCEID)) {
                case 0: printf(" ok\n"); break;
                case 1: StoreCell(0, SOURCEID); break; // next input is keyboard
                default: break;
            }
        } while (tiffIOR == 0);
        // display error type
        if (tiffIOR == -99999) break;       // produced by BYE
        ErrorMessage(tiffIOR);
        tiffIOR = 0;
    }
}
