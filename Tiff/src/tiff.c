#include <stdio.h>
#include <stdlib.h>
#include "vm.h"
#include "tiff.h"
#include <string.h>
#include <ctype.h>

/// Stacks, RAM and ROM are inside the VM, accessed through a narrow channel,
/// see vm.c. This abstraction allows you to put the VM anywhere, such as in
/// a DLL or at the end of a JTAG cable.
/// Access is through executing an instruction group in the VM:
/// VMstep() and DebugReg.

uint32_t DbgPC; // last PC returned by VMstep

uint32_t DbgGroup (uint32_t op0, uint32_t op1,
                   uint32_t op2, uint32_t op3, uint32_t op4) {
    DbgPC = VMstep(op0<<26 | op1<<20 | op2<<14 | op3<<8 | op4<<2, 1);
    return GetDbgReg();
}

uint32_t FetchSP (void) {   // Pop from the stack
    VMstep(opSP*4, 1); // get stack pointer into A
    return DbgGroup(opA, opPORT, opDROP, opSKIP, opNOOP);
}
uint32_t PopNum (void) {   // Pop from the stack
    return DbgGroup(opPORT, opDROP, opSKIP, opNOOP, opNOOP);
}
void PushNum (uint32_t N) {   // Push to the stack
    SetDbgReg(N);
    DbgGroup(opDUP, opPORT, opSKIP, opNOOP, opNOOP);
}
uint32_t FetchCell (uint32_t addr) {  // Read from RAM or ROM
    SetDbgReg(addr);
    DbgGroup(opDUP, opPORT, opSetA, opSKIP, opNOOP);
    return DbgGroup(opFetchA, opPORT, opDROP, opSKIP, opNOOP);
}
void StoreCell (uint32_t N, uint32_t addr) {  // Write to RAM
    SetDbgReg(addr);
    DbgGroup(opDUP, opPORT, opSetA, opSKIP, opNOOP);
    SetDbgReg(N);
    DbgGroup(opDUP, opPORT, opStoreA, opSKIP, opNOOP);
}
uint8_t FetchByte (uint32_t addr) {  // Read from RAM or ROM
    SetDbgReg(addr);
    DbgGroup(opDUP, opPORT, opSetA, opSKIP, opNOOP);
    return (uint8_t) DbgGroup(opCfetchA, opPORT, opDROP, opSKIP, opNOOP);;
}
uint16_t FetchHalf (uint32_t addr) {  // Read from RAM or ROM
    SetDbgReg(addr);
    DbgGroup(opDUP, opPORT, opSetA, opSKIP, opNOOP);
    return (uint16_t) DbgGroup(opWfetchA, opPORT, opDROP, opSKIP, opNOOP);
}
void StoreByte (uint8_t N, uint32_t addr) {  // Write to RAM
    SetDbgReg(addr);
    DbgGroup(opDUP, opPORT, opSetA, opSKIP, opNOOP);
    SetDbgReg(N);
    DbgGroup(opDUP, opPORT, opCstoreA, opSKIP, opNOOP);
}
void EraseROM (void) {  // Erase internal ROM
    VMstep(opUSER*0x4000L + 10000, 1);
}
void StoreROM (uint32_t N, uint32_t addr) {  // Store cell to internal ROM
    SetDbgReg(N);
    DbgGroup(opDUP, opPORT, opSKIP, opNOOP, opNOOP);
    SetDbgReg(addr);
    DbgGroup(opDUP, opPORT, opSKIP, opNOOP, opNOOP);
    VMstep(opUSER*0x4000L + 10001, 1);
    DbgGroup(opDROP, opDROP, opSKIP, opNOOP, opNOOP);
}
void vmRAMfetchStr(char *s, unsigned int address, uint8_t length){
    int i;  char c;                         // Get a string from RAM
    for (i=0; i<length; i++) {
        c = FetchByte(address++);
        *s++ = c;
    }   *s++ = 0;                           // end in trailing zero
}
void vmRAMstoreStr(char *s, unsigned int address){
    char c;                                 // Store a string to RAM,
    while ((c = *s++)){                     // not including trailing zero
        StoreByte(c, address++);
    }
}
// use the native dump as a sanity check
void CellDump(int length, int addr) {
    SetDbgReg(length);
    DbgGroup(opDUP, opPORT, opSKIP, opNOOP, opNOOP);
    SetDbgReg(addr);
    DbgGroup(opDUP, opPORT, opSKIP, opNOOP, opNOOP);
    VMstep(opUSER*0x4000L + 10002, 1);
    DbgGroup(opDROP, opDROP, opSKIP, opNOOP, opNOOP);
}

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

/// Keyboard input is raw, from tiffEKEY().
/// To start with, we use single keystrokes to dump various parameters.
/// Note: CLion cooks keyboard input, you must run from the command line.

uint32_t Param = TIB;     // parameter

void ShowParam(void){
    printf("\015%08X  ", Param);
}

void Quick (void) {
    int c;
    EraseROM();
    while (1) {
        ShowParam();
        c = tiffEKEY();
        if (isxdigit(c)) {
            if (c>'9') c-=('A'-10); else c-='0';
            Param = Param*16 + (c&0x0F);
        } else {
            switch (c) {
                case 27:           return;                 // ESC = bye
                case 3:  Param=0;  break;                  // ^C = clear
                case 4:  CellDump(32, Param);   break;     // ^D = dump
                case 16: PushNum(Param);    break;         // ^P = Push
                case 15: Param = PopNum();  break;         // ^O = Pop
                case 18: Param = FetchCell(Param); break;  // ^R = Read
                case 23: StoreCell(Param, 1024);   break;  // ^W = Write
                case 24: StoreByte(Param, 100);    break;  // ^X = Write8
                case 25: Param = FetchByte(Param); break;  // ^Y = Read
                case 26: StoreByte(Param, 1024);   break;  // ^Z = Write
                case 19: Param = FetchSP();        break;  // ^S = Stack Pointer
                default:
                    printf("%d", c);
            }
        }
    }
}

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
