#include <stdio.h>
#include <stdlib.h>
#include "vm.h"
#include "tiff.h"
#include "vmaccess.h"
#include <string.h>
#include <ctype.h>

////////////////////////////////////////////////////////////////////////////////
/// Header Structure
/// Begins with a WID block that starts out blank.

void tiffWORDLIST (void) {  // ( -- wid )
    uint32_t hp = FetchCell(HP);
    PushNum(hp);                            // structure is already blank
    StoreCell(hp + 4, HEAD);                // HEAD points to last forward link
    StoreCell(hp + ((WordlistStrands+2)*4), HP);
    printf("New Wordlist at %04X, ", hp);
}
void tiffHEADER (void) {  // ( c-addr len -- )
    uint32_t link = FetchCell(HEAD);
    uint32_t hp = FetchCell(HP);
    StoreROM(hp + 4, link);                 // resolve forward link
    StoreCell(hp + 4, HEAD);                // HEAD points to last forward link
    StoreCell(hp + 8, HP);                  // two blank cells
}

int tiffIOR = 0;

/// tiffWORD gets the next <delimiter> delimited string from TIBB.
/// Return value: Length of string.
/// *dest is a place to put the string that's big enough to hold it.

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
uint32_t tiffPARSE (void) { //  ( delimiter -- addr length )
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
uint32_t tiffPARSENAME (void) { //  ( -- addr length )
    SkipWhite();
    PushNum(' ');
    return tiffPARSE();
}
// Interpret the TIB inside the VM, on the VM data stack.
void tiffINTERPRET(void) {
    char mystr[256];
    char token[33];
    uint32_t address;
    uint32_t length;
    vmRAMfetchStr(mystr, FetchCell(TIBB), FetchCell(TIBS));
    printf("Interpret|%s|", mystr);
    while (tiffPARSENAME()) {   // get the next blank delimited keyword
        length = PopNum();
        if (length>31) length=32;
        address = PopNum();
        vmRAMfetchStr(token, address, length);
        printf("\n|%s|", token);
    }  PopNum();  PopNum();     // keyword is an empty string
}

// Keyboard input uses the default terminal cooked mode.
// Since getline includes the trailing LF (or CR), we lop it off.

void tiffQUIT (char *cmdline) {
    char argline[MaxTIBsize+1];
    size_t bufsize = MaxTIBsize;  char *buf;
    int length, source, TIBaddr;
    EraseROM();
    StoreCell(HeadPointerOrigin, HP);
    StoreCell(CodePointerOrigin, CP);
    StoreCell(DataPointerOrigin, DP);
    while (1){
        SetDbgReg(STATUS);                  // reset USER pointer
        DbgGroup(opDUP, opSetUP, opSKIP, opNOOP, opNOOP);
        SetDbgReg(TiffRP0);                 // reset return stack
        DbgGroup(opDUP, opSetRP, opSKIP, opNOOP, opNOOP);
        SetDbgReg(TiffSP0);                 // reset data stack
        DbgGroup(opDUP, opSetSP, opSKIP, opNOOP, opNOOP);
        StoreCell(TiffRP0, R0);             // USER vars in terminal task
        StoreCell(TiffSP0, S0);
        StoreCell(0, SOURCEID);     	    // input is keyboard
        StoreCell(TIB, TIBB);     	        // use TIB
        StoreCell(0, STATE);     	        // interpret
        do {
            StoreCell(0, TOIN);				// >IN = 0
            StoreCell(0, TIBS);

            source = FetchCell(SOURCEID);
            TIBaddr = FetchCell(TIBB);
            switch (source) {
                case 0: // load TIBB from keyboard
                    if (*cmdline) {
                        strcpy (argline, cmdline);
                        length = strlen(cmdline);
                        *cmdline = 0;       // clear cmdline
                    } else {
                        buf = argline;
                        length = getline(&buf, &bufsize, stdin);
                        if (length > 0) length--;   // remove LF or CR
                    }
                    StoreCell(length, TIBS);
                    vmRAMstoreStr(argline, TIBaddr);
                    tiffIOR = -99999;   // quit now for testing *******
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
        strcpy(ErrorString, "TestWord");
        ErrorMessage(tiffIOR);
        tiffIOR = 0;
    }
}
