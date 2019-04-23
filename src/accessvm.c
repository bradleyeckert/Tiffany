#include <stdio.h>
#include <stdlib.h>
#if _WIN32
#include <windows.h>
#endif

#include "vm.h"
#include "vmUser.h"
#include "flash.h"
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

/// Stacks are accessed by stepping the VM (without advancing the PC).

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
void SetPCreg (uint32_t PC) {           // Set new PC
    SetDbgReg(PC);
    DbgGroup(opDUP, opPORT, opPUSH, opEXIT, opNOP);
}

void WipeTIB (void) {
    StoreCell(0, TIBS);
    StoreCell(0, TOIN);
    for (int i=0; i<MaxTIBsize; i+=4) {
        StoreCell(0, i+TIB);            // clear TIB
    }
    for (int i=0; i<PADsize; i+=4) {
        StoreCell(0, i+PAD);            // clear PAD
    }
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
                if (!FetchByte(CASESENS)) {
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

// returns ht if found, 0 otherwise
uint32_t iword_FIND (void) {  // ( addr len -- addr len | 0 ht )
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
        printf("NAME             LEN    XTE    XTC FID  LINE  WHERE    VALUE FLAG HEAD\n");
    }
    do {
        uint8_t length = FetchByte(WID+4);
        WordColor((length>>5) & 7);
        int flags = length>>5;
        length &= 0x1F;
        FetchString(wordname, WID+5, length);
        if (substring) {
            if (strlen(substring) > 31) goto done;  // no words this long
            strcpy(key, substring);
            strcpy(name, wordname);
            if (!FetchByte(CASESENS)) {
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
                printf("%-17s%3d%7X%7X",
                       wordname, FetchByte(WID+3), xte&0xFFFFFF, xtc&0xFFFFFF);
                if (linenum == 0xFFFF)
                    printf("  --    --");
                else
                    printf("%4X%6d", where>>24, linenum);
                printf("%7X%9X%5d %X\n",
                       where&0xFFFFFF, FetchCell(WID-16), flags, WID);
            }
            else printf("%s", wordname);
            ColorNormal();
            printf(" ");
        }
        WID = FetchCell(WID) & 0xFFFFFF;
    } while (WID);
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
    printf("\n");
}

// Look up the name of a definition from its address (xte)
char *GetXtNameWID(uint32_t WID, uint32_t xt) {
    do {
        uint32_t xte = FetchCell(WID - 4) & 0xFFFFFF;
        if (xte == xt) {
            uint8_t length = (uint8_t) FetchByte(WID + 4);
            FetchString(str, WID + 5, length & 0x1F);
            return str;
        }
        WID = FetchCell(WID) & 0xFFFFFF;
    } while (WID);
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

// StoreROM simulates flash memory.
// It starts out blank and writing '0's clears bits.
// If you try to write '0' to already '0' bits, you get an error.
// To write directly to internal ROM, use WriteROM.

// -------------------
// old  data pgm oops
//   0    0    1    1
//   0    1    1    0
//   1    0    0    0
//   1    1    1    0

void StoreROM (uint32_t data, uint32_t address) {
    int ior = 0;
    if (address&3) {
        tiffIOR = -23;
        return;
    }
    uint32_t old = FetchCell(address);
    uint32_t pgm = old & data;
    if (address < (ROMsize*4)) {
        ior = WriteROM(pgm, address);
    }
    if (address >= ((ROMsize+RAMsize)*4)) {
        ior = FlashWrite(data, address);
    }
    if (~(old|data)) {
        tiffIOR = -60; // non-blank bits
        return;
    }
    if (ior) {
        tiffIOR = ior;
    }
}

void CommaC (uint32_t x) {  // append a word to code space
    uint32_t cp = FetchCell(CP);
    StoreROM(x, cp);
    StoreCell(cp + 4, CP);
}

void CommaD (uint32_t x) {  // append a word to data space
    uint32_t dp = FetchCell(DP);
    StoreCell(x, dp);
    StoreCell(dp + 4, DP);
}

void CommaH (uint32_t x) {  // append a word to header space
    uint32_t hp = FetchCell(HP);
    StoreROM(x, hp);
    StoreCell(hp + 4, HP);
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
    return vmRegRead(ID);
    /*
    switch(ID) {
        case 0: return DbgGroup(opDUP, opPORT, opDROP, opSKIP, opNOP);  // T
        case 1: return DbgGroup(opOVER, opPORT, opDROP, opSKIP, opNOP); // N
        case 2: return ReadSP(opRP);
        case 3: return ReadSP(opSP) + 4; // correct for offset
        case 4: return ReadSP(opUP);
        case 5: return (DbgPC*4); // Don't make this the first RegRead  // PC
        default: return 0;
    }
    */
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
//    StoreCell(STATUS, FOLLOWER);  	    // only one task
    StoreCell(TIB, TIBB);               // point to TIB
    StoreCell(0, STATE);
    StoreByte(0, COLONDEF);             // clear this byte
    InitIR();
}

void AddWordlistHead (uint32_t wid, char *name) {
    CommaH(0x12345678);                 // name tag
    if (name) {
        CommaH(wid + 0xFF000000);       // include "named" tag
        CompString(name, 7, HP);        // padded to cell alignment
    } else {
        CommaH(wid);
    }

}

// Initialize ALL variables in the terminal task
void InitializeTermTCB (void) {
    VMpor();                            // clear VM and RAM
    vmMEMinit();
    initFilelist();                     // clear list of filenames used by LOCATE
    StoreCell(HeadPointerOrigin+4, HP); // leave cell for filelist
    StoreCell(0, CP);
    StoreCell(DataPointerOrigin, DP);
    StoreCell(10, BASE);                // decimal
    StoreCell(FORTHWID, CURRENT);       // definitions are to Forth wordlist
    StoreCell(FORTHWID, CONTEXT);       // context is Forth
    StoreByte(1, WIDS);                 // one wordlist in search order
    AddWordlistHead(FORTHWID, "forth");
    InitializeTIB();
    InitCompiler();                     // load compiler words
    StoreByte(StartupTheme, THEME);
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
    uint32_t * temp = (uint32_t*) malloc(RAMsize * sizeof(uint32_t));
    for (int i=0; i<RAMsize; i++) {     // stash RAM in temporary location
        temp[i] = FetchCell((ROMsize+i)*4);
    }
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
    tiffIOR = 0;
    VMpor();
    for (int i=0; i<RAMsize; i++) {     // restore RAM
        StoreCell(temp[i], (ROMsize+i)*4);
    }
    free(temp);
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
    if (FetchByte(THEME)) {
        printf("\033[0m");              // reset colors
    }
}

uint32_t DisassembleGroup(uint32_t addr, int hilight) {
    uint32_t r;
    uint32_t ir = FetchCell(addr);
    if (hilight) {
        ColorHilight();
        if (!FetchByte(THEME)) {
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
    printf("\n");
    return r;
}

// Disassemble to screen
void Disassemble(uint32_t addr, uint32_t length) {
    int i;
    if (addr & 0xFF800000) {
        printf("Can't disassemble a C function\n");
    } else {
        if (length == 1) { // could be a defer
            int jmp = DisassembleGroup(addr, 0);
            if (jmp) {
                addr = jmp;
                length = 10;
                printf("              refers to:\n");
            } else {
                goto ex;
            }
        }
        for (i=0; i<length; i++) {
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
    printf("\n(0..F)=digit, Enter=Clear, O=pOp, P=Push, R=Refresh, X=eXecute, ^C=Bye\n");
    #ifdef TRACEABLE
    printf("G=Goto, S=Step, V=oVer, /=Run, @=Fetch, U=dUmp, W=WipeHistory, Y=Redo, Z=Undo \n");
    #else
    printf("G=Goto, S=Step, V=oVer, /=Run, @=Fetch, U=dUmp\n");
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
                case 3:  SetCursorPosition(0, DumpRows+7);  // ^C = bye
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
                case 'v':
                case 'V': pc = RegRead(5);
                          uint32_t done = pc + 4;
                          while (pc != done) {
                              Tracing=1;  VMstep(FetchCell(pc),0);
                              Tracing=0;  pc = RegRead(5);
                          }
                          SetPCreg(pc);
                          goto Re;                          // / = Execute until return
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
