#include <stdio.h>
#include <stdlib.h>
#if _WIN32
#include <windows.h>
#endif

#include "vm.h"
#include "tiff.h"
#include "fileio.h"
#include "accessvm.h"
#include "compile.h"
#include "colors.h"
#include <string.h>
#include <ctype.h>

#ifdef __linux__
#include <sys/ioctl.h>
#include <unistd.h>

int TermWidth(void) {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return w.ws_col;
}

#elif _WIN32
// Get width of console
int TermWidth(void) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    int columns;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    columns = csbi.srWindow.Right - csbi.srWindow.Left + 1;
//     rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    return columns;
}
#endif

/// Stacks, RAM and ROM are inside the VM, accessed through a narrow channel,
/// see vm.c. This abstraction allows you to put the VM anywhere, such as in
/// a DLL or at the end of a JTAG cable.
/// Access is through executing an instruction group in the VM:
/// VMstep() and DebugReg.
/// Even if TRACEABLE allows easier access, it isn't used here.

/// Bytes are read and written using cell operations so that the VM needn't
/// implement byte memory operations.

int CaseInsensitive = 1;

uint32_t DbgPC; // last PC returned by VMstep

uint32_t DbgGroup (uint32_t op0, uint32_t op1,
                   uint32_t op2, uint32_t op3, uint32_t op4) {
    DbgPC = 4 * VMstep(op0<<26 | op1<<20 | op2<<14 | op3<<8 | op4<<2, 1);
    return GetDbgReg();
}
uint32_t PopNum (void) {                // Pop from the data stack
    return DbgGroup(opPORT, opDROP, opSKIP, opNOP, opNOP);
}
void PushNum (uint32_t N) {             // Push to the data stack
    SetDbgReg(N);
    DbgGroup(opDUP, opPORT, opSKIP, opNOP, opNOP);
}
uint32_t PopNumR (void) {               // Pop from the return stack
    return DbgGroup(opPOP, opPORT, opDROP, opSKIP, opNOP);
}
void PushNumR (uint32_t N) {            // Push to the return stack
    SetDbgReg(N);
    DbgGroup(opDUP, opPORT, opPUSH, opSKIP, opNOP);
}
uint32_t FetchCell (uint32_t addr) {    // Read from RAM or ROM
    SetDbgReg(addr);
    return DbgGroup(opDUP, opPORT, opFetch, opPORT, opDROP);
}
uint8_t FetchByte (uint32_t addr) {     // Read from RAM or ROM
    return (uint8_t) 0xFF & (FetchCell(addr) >> (8*(addr&3)));
}
uint16_t FetchHalf (uint32_t addr) {    // Read from RAM or ROM
    return (uint16_t) 0xFFFF & (FetchCell(addr) >> (8*(addr&2)));
}
void StoreCell (uint32_t N, uint32_t addr) {  // Write to RAM
    SetDbgReg(N);
    DbgGroup(opDUP, opPORT, opDUP, opSKIP, opNOP);
    SetDbgReg(addr);
    DbgGroup(opPORT, opStorePlus, opDROP, opSKIP, opNOP);
}
void StoreHalf (uint16_t N, uint32_t addr) {  // Write half with cell rd/wr
    SetDbgReg(N);
    DbgGroup(opDUP, opPORT, opDUP, opSKIP, opNOP);
    SetDbgReg(addr);
    DbgGroup(opPORT, opWstorePlus, opDROP, opSKIP, opNOP);
}
void StoreByte (uint8_t N, uint32_t addr) {  // Write byte with cell rd/wr
    SetDbgReg(N);
    DbgGroup(opDUP, opPORT, opDUP, opSKIP, opNOP);
    SetDbgReg(addr);
    DbgGroup(opPORT, opCstorePlus, opDROP, opSKIP, opNOP);
}
void SetPCreg (uint32_t PC) {           // Set new PC
    SetDbgReg(PC);
    DbgGroup(opDUP, opPORT, opPUSH, opEXIT, opNOP);
}

// Write to AXI through SetDbgReg and DbgGroup.
// accessing this way allows target hardware to write
// to SPI flash through the debug interface when brain dead
// Store data to a temporary cell at the top of RAM, which gets trashed.
static void WriteAXI(uint32_t data, uint32_t address) {
    VMstep((uint32_t)opLIT*0x100000 + ((ROMsize+RAMsize-1)*4), 1);
    SetDbgReg(data);
    DbgGroup(opDUP, opPORT, opOVER, opStorePlus, opDROP); // save in temp
    SetDbgReg(address);
    DbgGroup(opDUP, opPORT, opStoreAS, opNOP, opNOP); // 1 word to AXI
    DbgGroup(opDROP, opDROP, opSKIP, opNOP, opNOP);
}

void FetchString(char *s, unsigned int address, uint8_t length){
    int i;  char c;                     // Get a string from RAM
    for (i=0; i<length; i++) {
        c = FetchByte(address++);
        *s++ = c;
    }   *s++ = 0;                       // end in trailing zero
}
void StoreString(char *s, unsigned int address){
    char c;                             // Store a string to RAM,
    while ((c = *s++)){                 // not including trailing zero
        StoreByte(c, address++);
    }
}

// Access VM's SPI flash through USER opcode
void SPIaddressCmd (uint32_t addr, int command, int ending) {
    SetDbgReg(command);
    VMstep((uint32_t)opDUP*0x100000 + (uint32_t)opPORT*0x4000 + opUSER*0x100 + 5, 1);
    SetDbgReg((addr>>16) & 0xFF);
    VMstep((uint32_t)opPORT*0x4000 + opUSER*0x100 + 5, 1);
    SetDbgReg((addr>>8) & 0xFF);
    VMstep((uint32_t)opPORT*0x4000 + opUSER*0x100 + 5, 1);
    SetDbgReg((addr & 0xFF) + ending);
    VMstep((uint32_t)opPORT*0x4000 + opUSER*0x100 + 5, 1);
    DbgGroup(opDROP, opSKIP, opNOP, opNOP, opNOP);
}

void SPIonesie (int command) {
    SetDbgReg(command + 0x100);
    VMstep((uint32_t)opDUP*0x100000 + (uint32_t)opPORT*0x4000 + opUSER*0x100 + 5, 1);
    DbgGroup(opDROP, opSKIP, opNOP, opNOP, opNOP);
}

int EraseSPI4K (uint32_t addr) {        // erase VM's SPI flash
    SPIonesie(6);
    SPIaddressCmd(addr, 32, 0);
    SPIonesie(4);
    // should insert SPIwait here
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Dictionary Traversal Words

// Search the thread whose head pointer is at WID. Return ht if found, 0 if not.
uint32_t SearchWordlist(char *name, uint32_t WID) {
    unsigned int length = strlen(name);
    if (length>31) tiffIOR = -19;
    do {
        int len = FetchByte(WID + 4) & ~0xC0;      // mask off jumpok and public
        if (len == length) {                  // likely candidate: lengths match
            int i = 0;                                 // starting index in name
            uint32_t k = WID+5;                                   // RAM address
            while (len--) {
                char c1 = name[i++];
                char c2 = FetchByte(k++);
                if (CaseInsensitive) {
                    c1 = tolower(c1);
                    c2 = tolower(c2);
                }
                if (c1 != c2) goto next;
            }
            return WID;
        }
next:   WID = FetchCell(WID) & 0xFFFFFF;
    } while (WID);
    return 0;
}

static char str[33];

uint32_t tiffFIND (void) {  // ( addr len -- addr len | 0 ht )
    uint8_t length = (uint8_t)PopNum();
    uint32_t addr = PopNum();
    uint8_t wids = FetchByte(WIDS);
    while (wids--) {
        uint32_t wid = FetchCell(CONTEXT + wids*4);  // search the first list
        FetchString(str, addr, length);
        uint32_t ht = SearchWordlist(str, FetchCell(wid));
        if (ht) {
            PushNum(0);
            PushNum(ht);
            StoreCell(ht, HEAD);    // copy to HEAD
            return ht;
        }
    }
    PushNum(addr);
    PushNum(length);
    return 0;
}

uint32_t WordFind (char *name) {        // a more C-friendly version}
    uint8_t wids = FetchByte(WIDS);
    while (wids--) {
        uint32_t wid = FetchCell(CONTEXT + wids*4);  // search the first list
        uint32_t ht = SearchWordlist(name, FetchCell(wid));
        if (ht) {
            return ht;
        }
    }
    return 0;
}

// WORDS primitive

/*
| Cell | \[31:24\]                        | \[23:0\]                           |
| ---- |:---------------------------------| ----------------------------------:|
| -3   | Source File ID                   | List of words that reference this  |
| -2   | Source Line, Low byte            | xtc, Execution token for compile   |
| -1   | Source Line, High byte           | xte, Execution token for execute   |
| 0    | # of instructions in definition  | Link                               |
| 1    | Name Length                      | Name Text, first 3 characters      |
| 2    | 4th name character               | Name Text, chars 5-7, etc...       |
*/
void PrintWordlist(uint32_t WID, char *substring, int verbosity) {
    char key[32];
    char name[32];
    char wordname[32];
    if (strlen(substring) == 0)         // zero length string same as NULL
        substring = NULL;
    if (verbosity) {
        printf("\nNAME             LEN    XTE    XTC FID  LINE  WHERE    VALUE FLAG HEAD");
    }
    do {
        uint8_t length = FetchByte(WID+4);
        WordColor((length>>4) & 0x0E);
        int flags = length>>5;
        length &= 0x1F;
        FetchString(wordname, WID+5, length);
        if (substring) {
            if (strlen(substring) > 31) goto done;  // no words this long
            strcpy(key, substring);
            strcpy(name, wordname);
            if (CaseInsensitive) {
                UnCase(key);
                UnCase(name);
            }
            char *s = strstr(name, key);
            if (s != NULL) goto good;
        } else {
good:       if (verbosity) {            // long version
                uint32_t where = FetchCell(WID-12);
                uint32_t xtc = FetchCell(WID-8);
                uint32_t xte = FetchCell(WID-4);
                uint32_t linenum = ((xte>>16) & 0xFF00) + (xtc>>24);
                printf("\n%-17s%3d%7X%7X",
                       wordname, FetchByte(WID+3), xte&0xFFFFFF, xtc&0xFFFFFF);
                if (linenum == 0xFFFF)
                    printf("  --    --");
                else
                    printf("%4X%6d", where>>24, linenum);
                printf("%7X%9X%5d %X",
                       where&0xFFFFFF, FetchCell(WID-16), flags, WID);
            }
            else printf("%s", wordname);
            ColorNormal();
            printf(" ");
        }
        WID = FetchCell(WID) & 0xFFFFFF;
    } while (WID);
    printed = 1;
done:
    ColorNormal();
}

void tiffWords (char *substring, int verbosity) {
    uint8_t wids = FetchByte(WIDS);
    while (wids--) {
        uint32_t wid = FetchCell(CONTEXT + wids*4);  // search the first list
        PrintWordlist(FetchCell(wid), substring, verbosity);
    }
    ColorNormal();
}

// Look up the name of a definition from its address (xte)
char *GetXtNameWID(uint32_t WID, uint32_t xt) {
//    if (xt) {
        do {
            uint32_t xte = FetchCell(WID - 4) & 0xFFFFFF;
            if (xte == xt) {
                uint8_t length = (uint8_t) FetchByte(WID + 4);
                FetchString(str, WID + 5, length & 0x1F);
                return str;
            }
            WID = FetchCell(WID) & 0xFFFFFF;
        } while (WID);
//    }
    return NULL;
}

char *GetXtName(uint32_t xt) {
    uint8_t wids = FetchByte(WIDS);  char *name;
    while (wids--) {
        uint32_t wid = FetchCell(CONTEXT + wids * 4);  // search the first list
        wid = FetchCell(wid);
        name = GetXtNameWID(wid, xt);
        if (name != NULL) return name;
    } return NULL;
}

// Replace all instances of OldXt with NewXt.
// This uses WriteROM, which is host-only (doesn't work on flash) so there's
// no ReplaceXTWID equivalent in the target Forth.
void ReplaceXTWID(uint32_t WID, uint32_t OldXt, uint32_t NewXt) {
    do {
        uint32_t xte = FetchCell(WID - 4);
        uint32_t xtc = FetchCell(WID - 8);
        if (((xte ^ OldXt) & 0xFFFFFF) == 0) {
            xte = (xte & ~0xFFFFFF) + NewXt;
            WriteROM(xte, WID - 4);
        }
        if (((xtc ^ OldXt) & 0xFFFFFF) == 0) {
            xtc = (xtc & ~0xFFFFFF) + NewXt;
            WriteROM(xtc, WID - 8);
        }
        WID = FetchCell(WID) & 0xFFFFFF;
    } while (WID);
}
void ReplaceXTs(void) {  // ( newXT oldXT -- )
    uint32_t OldXt = PopNum();
    uint32_t NewXt = PopNum();
    uint8_t wids = FetchByte(WIDS);
    while (wids--) {
        uint32_t wid = FetchCell(CONTEXT + wids * 4);  // search the first list
        wid = FetchCell(wid);
        ReplaceXTWID(wid, OldXt, NewXt);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Dumb Compilation

void EraseSPIimage (void) {  // Erase SPI flash image and internal ROM
    int i, ior;
    for (i=0; i < (SPIflashSize/1024); i++) {
        ior = EraseSPI4K(i * 4096);     // start at a byte address
        if (!tiffIOR) tiffIOR = ior;
    }
    for (i=0; i < ROMsize; i++) {       // size is cells
        WriteROM(-1, i*4);              // addressing is bytes
    }
}

// StoreROM simulates flash memory.
// It starts out blank and writing '0's clears bits.
// If you try to write '0' to already '0' bits, you get an error.
// To write directly to internal ROM, use WriteROM.

void StoreROM (uint32_t data, uint32_t address) {
    int ior = 0;
    if (address&3) {
        ior = -23;
    }
    if (address < (ROMsize*4)){
        uint32_t rom = FetchCell(address);
        ior = WriteROM(rom & data, address);
        if (~(rom|data)) ior = -60; // non-blank
    }
#ifdef BootFromSPI
    WriteAXI(data, address);
#else
    if (address >= (ROMsize*4)){
        WriteAXI(data, address);
    }
#endif // BootFromSPI
    if (!tiffIOR) tiffIOR = ior;
}

void CommaC (uint32_t X) {  // append a word to code space
    uint32_t cp = FetchCell(CP);
    StoreROM(X, cp);
    StoreCell(cp + 4, CP);
}

void CommaD (uint32_t X) {  // append a word to data space
    uint32_t dp = FetchCell(DP);
    StoreCell(X, dp);
    StoreCell(dp + 4, DP);
}

void CommaH (uint32_t X) {  // append a word to header space
    uint32_t hp = FetchCell(HP);
    StoreROM(X, hp);
    StoreCell(hp + 4, HP);
}

uint8_t xstrlen(char *s) {  // strlen that skips escape codes
    uint8_t len = 0;
    char c;
    while (1) {
        c = *s++;
        if (!c) break;
        if (c == '\\') {
            if (!*s++) break; // don't skip terminator
        }
        len++;
    }
    return len;
}

// Generic counted string compile
// Include optional flags when compiling a header name
// The esc flag means
void CommaXstring (char *s,             // string to compile
                   void(*fn)(uint32_t), // Post-string padding function
                   int flags,           // flags to OR with count byte
                   int esc) {           // true if escape sequences are supported
    int length;
    if (esc) length = xstrlen(s);
    else     length = strlen(s);
    uint32_t word = 0xFFFFFF00 | ((flags | length) & 0xFF);
    int i = 1;  uint32_t x, mask;  char hex[4];
    while (length--) {
        char c = *s++;
        if (esc) {                      // escape sequences supported
            if (c == '\\') {
                c = *s++;
                switch (c) {
                    case 'a': c = 7; break;   // BEL  (bell)
                    case 'b': c = 8; break;   // BS (backspace)
                    case 'e': c = 27; break;  // ESC (not in C99)
                    case 'f': c = 12; break;  // FF (form feed)
                    case 'l': c = 10; break;  // LF
                    case 'n': c = 10; break;  // newline
                    case 'q': c = '"'; break; // double-quote
                    case 'r': c = 13; break;  // CR
                    case 't': c = 9; break;   // HT (tab)
                    case 'v': c = 11; break;  // VT
                    case 'x': hex[2] = 0;  	  // hex byte
						hex[0] = *s++;
						hex[1] = *s++;
						c = (char)strtol(hex, (char **)NULL, 16); break;
                    case 'z': c = 0; break;   // NUL
                    case '"': c = '"'; break; // double-quote
                    default: break;
                }
            }
        }
        x = c << (i*8);
        mask = ~(0xFF << (i*8));
        word &= (mask | x);
        if (i == 3) {
            fn(word);
            word = 0xFFFFFFFF;
        }
        i = (i + 1) & 3;
    }
    if (i) {
        fn(word);                // pad if needed
    }
}

void CommaHstring (char *s) {   // compile string to head space
    CommaXstring(s, CommaH, 0, 0); // disable escape sequences
}
void CommaDstring (char *s) {   // compile string to data space
    CommaXstring(s, CommaD, 0, 1);
}
void CommaCstring (char *s) {   // compile string to code space
    CommaXstring(s, CommaC, 0, 1);
}

////////////////////////////////////////////////////////////////////////////////
// Tracing and debug facilities

// Get registers the easy way if TRACEABLE, the hard way if not.
// Note: The B register is not readable by the debugger (there's no opcode for it)
#ifdef TRACEABLE
extern uint32_t VMreg[10];
uint32_t RegRead(int ID) {
    switch(ID) {
        case 0:
        case 1: return VMreg[ID];   // T N
        case 2:
        case 3:
        case 4: return 4*(VMreg[ID] + ROMsize);
        case 5: return 4*VMreg[ID];
        default: return 0;
    }
}
#else

uint32_t ReadSP(int opcode) {           // read a stack pointer
    DbgGroup(opDUP, opDUP, opXOR, opcode, opSKIP);
    return DbgGroup(opPORT, opDROP, opSKIP, opNOP, opNOP);
}

uint32_t RegRead(int ID) {
    switch(ID) {
        case 0: return DbgGroup(opDUP, opPORT, opDROP, opSKIP, opNOP);  // T
        case 1: return DbgGroup(opOVER, opPORT, opDROP, opSKIP, opNOP); // N
        case 2: return ReadSP(opRP);
        case 3: return ReadSP(opSP) + 4; // correct for offset
        case 4: return ReadSP(opUP);
        case 5: return (DbgPC*4); // Don't make this the first RegRead  // PC
        default: return 0;
    }
}
#endif // TRACEABLE

int Tracing;                    // TRUE if recording trace history

#ifdef TRACEABLE
//==============================================================================
// The trace buffer is a power-of-2 sized array of state change structures.
// Size is set in config.h

static uint32_t uHead;          // next free element in state change history
static uint32_t uTail;          // first element in state change history
static uint32_t uHere;          // current position in history, =uHead if latest
static uint32_t uMask;          // bit mask for indices

struct StateChange {
    uint8_t Type;
    int32_t ID;
    uint32_t Old;
    uint32_t New;
};

// Let the compiler manage large static arrays instead of malloc etc.
struct StateChange StateChanges[1<<TraceDepth];

void CreateTrace(void) {        // allocate memory for the trace buffer
    uMask = (1<<TraceDepth) - 1;               // AND with indices
    uHead = 0;  uTail = 0;  uHere = 0;
}

void DestroyTrace(void) {       // free memory for the trace buffer
}
/// The Trace function tracks VM state changes using these parameters:
/// Type of state change: 0 = unmarked, 1 = new opcode, 2 or 3 = new group;
/// Register ID: Complement of register number if register, memory if other;
/// Old value: 32-bit.
/// New value: 32-bit.

unsigned int TraceElements (void) {     // How many elements are in the trace history?
    return ((uHead - uTail) & uMask);
}
void Trace(unsigned int Type, int32_t ID, uint32_t Old, uint32_t New){
    uint32_t idx;
    if (Tracing) {
        if ((Old != New) || (Type)){    // skip states that didn't actually change
            uHead = uHere;
            if (TraceElements() == uMask) { // is it topped out?
                uTail = (uTail+1) & uMask;  // drop the oldest point
            }
            idx = uHead & uMask;
            StateChanges[idx].Type = (uint8_t) Type;
            StateChanges[idx].ID = ID;
            StateChanges[idx].Old = Old;
            StateChanges[idx].New = New;
            uHead = (uHead + 1) & uMask;
            uHere = uHead;              // keep the pointer current
        }
    }
}

// There are two kinds of Undo and two kinds of Redo:
// Undo reverses instruction groups without moving head.
// Redo restores instruction groups without moving head.
// If you execute a group, any history forward of uHere is discarded.

// Undo one instruction, ior=0 if okay, 1 if end reached.
int UndoTrace(void) {
    int32_t ID;  int Type;
    while (1) {
        if (uHere == uTail) return 1;   // already at the beginning
        uHere = (uHere-1) & uMask;      // step backward
        ID = StateChanges[uHere].ID;
        Type = StateChanges[uHere].Type;
        UnTrace(ID, StateChanges[uHere].Old);
        if (Type>1) return 0;           // instruction is undone
    }
}

// Redo one instruction, ior=0 if okay, 1 if end reached.
int RedoTrace(void) {
    int32_t ID;  int Type;
    while (1) {
        if (uHere == uHead) return 1;   // already at the beginning
        ID = StateChanges[uHere].ID;
        Type = StateChanges[uHere].Type;
        UnTrace(ID, StateChanges[uHere].New);
        uHere = (uHere+1) & uMask;      // step forward
        if (Type>1) return 0;           // instruction is redone
    }
}


//---------------------------------------------------------
#ifdef VERBOSE
// Dump trace history buffer data, for debugging tracing.

void TraceHist(void) {                  // dump trace history
    uint32_t size = TraceElements();
    uint32_t ptr = uHead;
    int32_t ID;
    int col = 0;
    if (size>31) size=32;               // limit the size
    printf("Tail=%d, Head=%d, Mask=%X\n", uTail, uHead, uMask);
    while (size--) {
        ptr = (ptr-1) & uMask;          // pre-decrement from head (newest first)
        ID = StateChanges[ptr].ID;
        printf("%d ", StateChanges[ptr].Type);
        if (ID < 0) {                  // negative is register
            printf("R[%X] ", (~ID & 0xFF));
        } else {
            printf("%04X ", ID);
        }
        printf("%X %X, ", StateChanges[ptr].Old, StateChanges[ptr].New);
        col++;
        if (col==4) {
            col=0;  printf("\n");
        }
    }
}
#endif

//==============================================================================
#endif

// Initialize useful variables in the terminal task
void InitializeTIB (void) {
    SetDbgReg(STATUS);                  // reset USER pointer
    DbgGroup(opDUP, opPORT, opSetUP, opSKIP, opNOP);
    SetDbgReg(TiffRP0);                 // reset return stack
    DbgGroup(opDUP, opPORT, opSetRP, opSKIP, opNOP);
    SetDbgReg(TiffSP0);                 // reset data stack
    // T ends nonzero after this, so we clear it with XOR.
    DbgGroup(opDUP, opPORT, opSetSP, opDUP, opXOR);
    StoreCell(TiffRP0, RP0);            // USER vars in terminal task
    StoreCell(TiffSP0, SP0);
    StoreCell(STATUS, FOLLOWER);  	    // only one task
    StoreCell(TIB, TIBB);               // point to TIB
    StoreCell(0, STATE);
    StoreCell(0, COLONDEF);             // clear this byte and the other three
    InitIR();
}
/*
| Cell | Usage                          |
| ---- | ------------------------------:|
| 0    | Link                           |
| 1    | Value resolved by GILD         |
| 2    | WID                            |
| 3    | Optional name (counted string) |
*/
uint32_t WordlistHead (void) {          // find the first blank link
    int32_t link = HeadPointerOrigin+4;
    int32_t result;
    do {
        result = link;
        link = FetchCell(link);         // -1 if end of list
    } while (link != -1);
    return result;
}
void AddWordlistHead (uint32_t wid, char *name) {
    uint32_t hp = FetchCell(HP);
    StoreROM(hp, WordlistHead());       // resolve forward link
    CommaH(-1);                         // link
    CommaH(-1);                         // to be resolved by GILD
    if (name) {
        CommaH(wid + 0x80000000);       // include "named" tag
        CommaHstring(name);             // padded to cell alignment
    } else {
        CommaH(wid);
    }
}

// GILD stores WORDLISTS in the second header space cell

// Initialize ALL variables in the terminal task
void InitializeTermTCB (void) {
    VMpor();                            // clear VM and RAM
    initFilelist();
    EraseSPIimage();                    // clear SPI flash image
    StoreCell(HeadPointerOrigin+20, HP); // leave 5 cells for filelist, widlist, hp, cp, and dp
    StoreCell(CodePointerOrigin, CP);
    StoreCell(DataPointerOrigin, DP);
    StoreCell(10, BASE);                // decimal
    StoreCell(FORTHWID, CURRENT);       // definitions are to Forth wordlist
    StoreCell(FORTHWID, CONTEXT);       // context is Forth
    StoreByte(1, WIDS);                 // one wordlist in search order
    AddWordlistHead(FORTHWID, "forth");
    InitializeTIB();
    InitCompiler();                     // load compiler words
}

int Rdepth(void) {                      // return stack depth
    return (int)(FetchCell(RP0) - RegRead(2)) / 4;
}
int Sdepth(void) {                      // data stack depth
    int foo = FetchCell(SP0);
    return (int)(foo - RegRead(3)) / 4;
}

uint32_t ChangeRegs[2][6];              // rows: old, new

void RegChangeInit (void) {             // Initialize register changes
    for (int i=0; i<6; i++) {
        uint32_t x = RegRead(i);
        ChangeRegs[0][i] = x;
        ChangeRegs[1][i] = x;
    }
    ChangeRegs[1][5] -= 4;
}
static char RegName[6][4] = {" T", " N", "RP", "SP", "UP", "PC"};

void SetCursorPosition(int X, int Y){
    printf("\033[%d;%df", Y, X);
}
void RegChanges (FILE *fp, int format) {
    if (!format) {                      // format 0 = debug dashboard
        SetCursorPosition(0, DumpRows+8);
    }
    for (int i=0; i<6; i++) {           // [1] = expected, [0] = actual
        ChangeRegs[0][i] = RegRead(i);  //                       ^^^^^^
    }
    ChangeRegs[1][5] += 4;              // expected PC is already bumped
    int changes = 0;
    for (int i=0; i<6; i++) {           // compare...
        uint32_t actual   = ChangeRegs[0][i];
        uint32_t expected = ChangeRegs[1][i];
        char *reg = RegName[i];
        if (actual != expected) {
            changes++;
            switch (format) {
                case 2: // VHDL format:
                fprintf(fp, "    changes(%d, x\"%08X\");\n", i, actual);
                break;
                case 1: // C format:
                fprintf(fp, "    changes(%d, 0x%08X);\n", i, actual);
                break;
                default:
                fprintf(fp, "%s changed from %08X to %08X\n", reg, expected, actual);
            }
        }
    }
    if (!format) {                      // format 0 = debug dashboard
        while (changes<4) {
            changes++;
            printf("%64s\n", " ");
        }
    }
    memmove(ChangeRegs[1], ChangeRegs[0], 6*sizeof(uint32_t));
}

void MakeTestVectors(FILE *ofp, int length, int format) {
    VMpor();
    RegChangeInit();                    // start at PC = 0
    for (int i=0; i<length; i++) {
        uint32_t pc = RegRead(5);
        uint32_t ir = FetchCell(pc);
        switch (format) {
        case 2:         // VHDL
            fprintf(ofp, "    newstep(x\"%08X\", %d);  -- PC = %04Xh\n", ir, i, pc);
            break;
            default:    // C
            fprintf(ofp, "    newstep(0x%08X, %d);  // PC = %04Xh\n", ir, i, pc);
        }
        Tracing=1;  VMstep(ir,0);
        Tracing=0;
        RegChanges(ofp, 1);             // display changes in C format
    }
    VMpor();
}


////////////////////////////////////////////////////////////////////////////////
/// Debugger Display uses full screen with VT100 commands
/// It does not work with the old Windows CMD console.
/// Use a real terminal like ConEmu or Linux.
/// Starting from Windows 10 TH2 (v1511), conhost.exe and cmd.exe support ANSI
/// and VT100 Escape Sequences out of the box (although they have to be enabled).
////////////////////////////////////////////////////////////////////////////////

void CellDump(int length, uint32_t addr) {  // DUMP
    uint32_t line[8];                       // buffer for ASCII
    uint8_t c;  int i, j, pad, len;
    if (length>4096) length=4096;           // limit to reasonable length
    addr = (addr+3) & ~3;                   // align
    while (length) {
        printf("%04X ", addr);              // address
        if (length<8) {
            len = length;  pad = 8 - length;
        } else {
            len = 8;  pad = 0;
        }
        for (i=0; i<len; i++) {             // data
            line[i] = FetchCell(addr);
            addr = addr + 4;
            printf("%08X ", line[i]);
        }
        for (i=0; i<pad; i++) {             // padding
            printf("         ");
        }
        for (i=0; i<len; i++) {             // data
            for (j=0; j<4; j++) {
                c = (uint8_t)((line[i] >> (j*8)) & 0xFF);
                if (c < ' ') c = '.';
                if ((c & 0x7F) == 0x7F) c = '.';
                printf("%c", c);
            }
        }
        for (i=0; i<pad; i++) {             // padding
            printf(" ");
        }
        length -= len;
        printf("\n");
    }
}

void DumpDataStack(void){
    int i;  int row = 2;
    int sp0 = FetchCell(SP0);
    int depth = Sdepth();
    int overflow = depth + 2 - DumpRows;
    if (depth<0) {
        SetCursorPosition(DataStackCol, row++);
        printf("Underflow");
    } else {
        if (overflow >= 0) {            // limit depth
            sp0 -= overflow * 4;
            depth -= overflow;
        }
        for (i=depth-1; i>=0; i--) {
            SetCursorPosition(DataStackCol, row++);
            printf("  %08X", FetchCell(sp0 - 4*depth + i*4));
        }
    }
    SetCursorPosition(DataStackCol, row++);
    printf("N %d", RegRead(1));
    SetCursorPosition(DataStackCol, row++);
    printf("T %d", RegRead(0));
}

void DumpReturnStack(void){
    int i;   int row = 2;
    int rp0 = FetchCell(RP0);
    int depth = Rdepth();
    int overflow = depth + 1 - DumpRows;
    if (depth<0) {
        SetCursorPosition(ReturnStackCol, row++);
        printf("Underflow");
    } else {
        if (overflow >= 0) {            // limit depth
            rp0 -= overflow * 4;
            depth -= overflow;
        }
        for (i=depth-1; i>=0; i--) {
            SetCursorPosition(ReturnStackCol, row++);
            printf("  %08X", FetchCell(rp0 - 4*depth + i*4));
        }
    }
}

void ResetColor(void) {
    if (ColorTheme) {
        printf("\033[0m");              // reset colors
    }
}

uint32_t DisassembleGroup(uint32_t addr, int hilight) {
    uint32_t r;
    uint32_t ir = FetchCell(addr);
    if (hilight) {
        ColorHilight();
        if (!ColorTheme) {
            printf("*");                // no colors, hilight with asterisk
        }
    } else {
        ResetColor();
    }
    printf("%04X %08X ", addr, ir);
    r = DisassembleIR(ir);
    char *name = GetXtName(addr);
    if (name) {
        ResetColor();
        printf("  \\ ");
        ColorDef();
        printf("%s", name);
        ResetColor();
    }
    printed = 1;
    return r;
}

// Disassemble to screen
void Disassemble(uint32_t addr, uint32_t length) {
    int i;
    if (addr & 0xFF800000) {
        printf("Can't disassemble a C function");
    } else {
        if (length == 1) { // could be a defer
            int jmp = DisassembleGroup(addr, 0);
            if (jmp) {
                addr = jmp;
                length = 10;
                printf("\n              refers to:");
            } else {
                goto ex;
            }
        }
        for (i=0; i<length; i++) {
            printf("\n");
            DisassembleGroup(addr, 0);
            addr += 4;
        }
    }
ex: ColorNormal();
}

void DumpROM(void) {
    int row = 2;  int i;
    static uint32_t ROMorigin;          // first ROM address
    uint32_t PC = RegRead(5);
    uint32_t addr = ROMorigin;          // start here unless...
    if (PC < ROMorigin) {               // move backward to show PC
        addr = PC;
        ROMorigin = PC;
    } else {
        if (PC > (ROMorigin+24)) {      // move forward to show PC
            addr = PC-24;
            ROMorigin = PC-24;
        }
    }
    for (i=0; i<DumpRows; i++) {
        SetCursorPosition(ROMdumpCol, row++);
        DisassembleGroup(addr, (PC == addr));
        addr += 4;
    }
}

void DumpRegs(void) {
    int row = 2;
    int old = printed;
    char name[7][4] = {" T", " N", "RP", "SP", "UP", "PC", "DB"};
    char term[13][10] = {"STATUS", "FOLLOWER", "RP0", "SP0", "TOS", "HANDLER", "BASE",
                        "Head: HP", "Code: CP", "Data: DP", "STATE", "CURRENT", "SOURCEID"};
    uint32_t i;
    printf("\033[s");                   // save cursor
    printf("\033[%d;0H", DumpRows+2);
    printf("\033[1J\033[H");            // erase to top of screen, home cursor
    ResetColor();
    printf("\033[4m");                  // hilight header
    printf("Data Stack  ReturnStack Registers  Terminal Vars  ");
    printf("ROM at PC     Instruction Disassembly"); // ESC [ <n> m
    printf("\033[0m");                  // default FG/BG

    DumpDataStack();
    DumpReturnStack();
    for (i=2; i<6; i++) {               // internal VM registers
        SetCursorPosition(RegistersCol, row++);
        printf("%s %X", name[i], RegRead(i));
    }
    row = 2;
    for (i=0; i<DumpRows; i++) {        // terminal task USER variables
        SetCursorPosition(RAMdumpCol + 5 - strlen(term[i]), row++);
        printf("%s %X", term[i], FetchCell(STATUS + i*4));
    }
    DumpROM();
    printf("\033[u");                   // restore cursor
    printed = old;
}

/// Keyboard input is raw, from tiffEKEY().
/// Use single keystrokes to dump various parameters, calculator-style.

void Bell(int flag) {
    if (flag) printf("\a");
}

uint32_t Param = 0;     // parameter

void ShowParam(void){
    SetCursorPosition(0, DumpRows+2);
    ResetColor();
    printf("%08X\n", Param);
    DisassembleIR(Param);
    printf("\033[K\n");  // erase to end of line
}

uint32_t vmTEST (void) {
    uint32_t normal = 0xFFFFFFFF;       // normal execution
    printf("\033[2J");                  // CLS
    PushNumR(0xDEADC0DC);               // extra run terminator
    RegChangeInit();
Re: DumpRegs();
    RegChanges(stdout, 0);
    SetCursorPosition(0, DumpRows+4);   // help along the bottom
    printf("\n(0..F)=digit, Enter=Clear, O=pOp, P=Push, R=Refresh, X=eXecute, \\=POR, ESC=Bye\n");
    #ifdef TRACEABLE
    printf("G=Goto, S=Step, /=Run, @=Fetch, U=dUmp, W=WipeHistory, Y=Redo, Z=Undo \n");
    #else
    printf("G=Goto, S=Step, /=Run, @=Fetch, U=dUmp\n");
    #endif
    int width = TermWidth();
    if (width < 95) {
        printf("*** ATTENTION: Console is %d columns. At least 95 columns are recommended. ***", width);
    }
    while (1) {
        uint32_t pc;
        ShowParam();
        int c = UserFunction(0,0,1);
        if (isxdigit(c)) {
            if (c>'9') c-=('A'-10); else c-='0';
            Param = Param*16 + (c&0x0F);
        } else {
            switch (c) {
                case 3:                                     // ^C
                case 27: SetCursorPosition(0, DumpRows+7);  // ESC = bye
                    PopNumR();                              // discard the terminator
#ifdef __linux__
                    CookedMode();
#endif // __linux__
                    return RegRead(5) & normal;
                case 13:  Param=0;  break;                  // ENTER = clear
                case 'p':
                case 'P': PushNum(Param);   goto Re;        // P = Push
                case 'o':
                case 'O': Param = PopNum();  goto Re;       // O = Pop
                case 'r':
                case 'R': goto Re;                          // R = Refresh
                case 'u':
                case 'U': CellDump(32, Param);   break;     // U = dump
                case 'g':
                case 'G': SetPCreg(Param);   goto Re;       // G = goto
                case ' ':
                case 's': // execute instruction from ROM   // S = Step
                case 'S': Tracing=1;  VMstep(FetchCell(RegRead(5)),0);
                          Tracing=0;  goto Re;
                case 'x': // execute instruction from Param (doesn't step PC)
                case 'X': Tracing=1;  VMstep(Param,1);      // X = Execute
                          Tracing=0;  goto Re;
                case '@': Param = FetchCell(Param); break;  // @ = Fetch
                case '\\': InitializeTermTCB();
                          ReloadFile();  goto Re;           // \ = Reset
                case '/': do { pc = RegRead(5);
                              Tracing=1;  VMstep(FetchCell(pc),0);
                              Tracing=0;
                          } while (0xDEADC0DC != RegRead(5));
                          PushNumR(0xDEADC0DC);
                          SetPCreg(pc);
                          normal = 0;                       // can't trust Execute to not hang
                          goto Re;                          // / = Execute until return
#ifdef TRACEABLE
                case 'h':
#ifdef VERBOSE
                case 'H': TraceHist();   break;             // H = history
#endif
                case 'w':
                case 'W': uHead=0; uTail=0; uHere=0; break; // W = wipe history
                case 'y':
                case 'Y': Bell(RedoTrace());   goto Re;     // Y = Redo
                case 'z':
                case 'Z': Bell(UndoTrace());   goto Re;     // Z = Undo
#endif
                default: printf("%d   ", c);
            }
        }
    }
}
