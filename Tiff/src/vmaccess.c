#include <stdio.h>
#include <stdlib.h>
#include "vm.h"
#include "tiff.h"
#include "vmaccess.h"
#include <string.h>
#include <ctype.h>

/// Stacks, RAM and ROM are inside the VM, accessed through a narrow channel,
/// see vm.c. This abstraction allows you to put the VM anywhere, such as in
/// a DLL or at the end of a JTAG cable.
/// Access is through executing an instruction group in the VM:
/// VMstep() and DebugReg.
/// Even if TRACEABLE allows easier access, it isn't used here.

uint32_t DbgPC; // last PC returned by VMstep

uint32_t DbgGroup (uint32_t op0, uint32_t op1,
                   uint32_t op2, uint32_t op3, uint32_t op4) {
    DbgPC = VMstep(op0<<26 | op1<<20 | op2<<14 | op3<<8 | op4<<2, 1);
    return GetDbgReg();
}
uint32_t PopNum (void) {                // Pop from the stack
    return DbgGroup(opPORT, opDROP, opSKIP, opNOOP, opNOOP);
}
void PushNum (uint32_t N) {             // Push to the stack
    SetDbgReg(N);
    DbgGroup(opDUP, opPORT, opSKIP, opNOOP, opNOOP);
}
uint32_t FetchCell (uint32_t addr) {    // Read from RAM or ROM
    SetDbgReg(addr);
    DbgGroup(opA, opDUP, opPORT, opSetA, opNOOP);
    return DbgGroup(opFetchA, opPORT, opDROP, opSetA, opNOOP);
}
void StoreCell (uint32_t N, uint32_t addr) {  // Write to RAM
    SetDbgReg(addr);
    DbgGroup(opA, opDUP, opPORT, opSetA, opNOOP);
    SetDbgReg(N);
    DbgGroup(opDUP, opPORT, opStoreA, opSetA, opNOOP);
}
uint8_t FetchByte (uint32_t addr) {     // Read from RAM or ROM
    SetDbgReg(addr);
    DbgGroup(opA, opDUP, opPORT, opSetA, opNOOP);
    return (uint8_t) DbgGroup(opCfetchA, opPORT, opDROP, opSetA, opNOOP);;
}
uint16_t FetchHalf (uint32_t addr) {    // Read from RAM or ROM
    SetDbgReg(addr);
    DbgGroup(opA, opDUP, opPORT, opSetA, opNOOP);
    return (uint16_t) DbgGroup(opWfetchA, opPORT, opDROP, opSetA, opNOOP);
}
void StoreByte (uint8_t N, uint32_t addr) {  // Write to RAM
    SetDbgReg(addr);
    DbgGroup(opA, opDUP, opPORT, opSetA, opNOOP);
    SetDbgReg(N);
    DbgGroup(opDUP, opPORT, opCstoreA, opSetA, opNOOP);
}
void SetPCreg (uint32_t PC) {           // Set new PC
    SetDbgReg(PC);
    DbgGroup(opDUP, opPORT, opPUSH, opEXIT, opNOOP);
}

void vmRAMfetchStr(char *s, unsigned int address, uint8_t length){
    int i;  char c;                     // Get a string from RAM
    for (i=0; i<length; i++) {
        c = FetchByte(address++);
        *s++ = c;
    }   *s++ = 0;                       // end in trailing zero
}
void vmRAMstoreStr(char *s, unsigned int address){
    char c;                             // Store a string to RAM,
    while ((c = *s++)){                 // not including trailing zero
        StoreByte(c, address++);
    }
}

/// Get registers the easy way if TRACEABLE, the hard way if not.
/// Note: The B register is not readable by the debugger (there's no opcode for it)
#ifdef TRACEABLE
extern uint32_t VMreg[10];
    uint32_t RegRead(int ID) {
        switch(ID) {
            case 0:
            case 1:
            case 2:
            case 3:
            case 4: return VMreg[ID];   // T N R A B
            case 5:
            case 6:
            case 7: return 4*(VMreg[ID] + ROMsize);
            case 8: return 4*VMreg[ID];
            default: return 0;
        }
    }
#else
uint32_t RegRead(int ID) {
    switch(ID) {
        case 0: return DbgGroup(opDUP, opPORT, opDROP, opSKIP, opNOOP); // T
        case 1: return DbgGroup(opOVER, opPORT, opDROP, opSKIP, opNOOP);// N
        case 2: return DbgGroup(opR, opPORT, opDROP, opSKIP, opNOOP);   // R
        case 3: return DbgGroup(opA, opPORT, opDROP, opSKIP, opNOOP);   // A
        case 5: VMstep((opA<<8) + opRP*4, 1);                           // RP
            return DbgGroup(opA, opPORT, opDROP, opSetA, opNOOP);
        case 6: VMstep((opA<<8) + opSP*4, 1);                           // SP
            return (DbgGroup(opA, opPORT, opDROP, opSetA, opNOOP) + 4);
            // The SP is offset due to saving A on the stack
        case 7: VMstep((opA<<8) + opUP*4, 1);                           // UP
            return DbgGroup(opA, opPORT, opDROP, opSetA, opNOOP);
        case 8: return (DbgPC*4); // Don't make this the first RegRead  // PC
        default: return 0;
    }
}
#endif // TRACEABLE

void EraseSPIimage (void) {  // Erase SPI flash image
    int i, ior;
    for (i=0; i < (AXIsize/1024); i++) {
        ior = EraseAXI4K(i * 1024);
        if (!tiffIOR) tiffIOR = ior;
    }
}

void StoreROM (uint32_t data, uint32_t address) {
    int ior = 0;
    if (address < (ROMsize*4)){
        ior = WriteROM(data, address);
    }
#ifdef BootFromSPI
// to do: also write to AXI through SetDbgReg and DbgGroup.

#endif // BootFromSPI
    if (!tiffIOR) tiffIOR = ior;
}

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

int Tracing;                    // TRUE if recording trace history

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
/// will be removing this soon

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
//---------------------------------------------------------

//==============================================================================
 #endif

// Initialize useful variables in the terminal task
void InitializeTIB(void) {
    SetDbgReg(STATUS);                  // reset USER pointer
    DbgGroup(opDUP, opPORT, opSetUP, opSKIP, opNOOP);
    SetDbgReg(TiffRP0);                 // reset return stack
    DbgGroup(opDUP, opPORT, opSetRP, opSKIP, opNOOP);
    SetDbgReg(TiffSP0);                 // reset data stack
    // T ends nonzero after this, so we clear it with XOR.
    DbgGroup(opDUP, opPORT, opSetSP, opDUP, opXOR);
    StoreCell(STATUS, FOLLOWER);  	    // only one task
    StoreCell(TiffRP0, R0);             // USER vars in terminal task
    StoreCell(TiffSP0, S0);
    StoreCell(TIB, TIBB);               // point to TIB
    StoreCell(0, STATE);
}

// Initialize useful variables in the terminal task
void InitializeTermTCB(void) {
    VMpor();
    EraseSPIimage();
    StoreCell(HeadPointerOrigin, HP);
    StoreCell(CodePointerOrigin, CP);
    StoreCell(DataPointerOrigin, DP);
    StoreCell(10, BASE);
    InitializeTIB();
}

int Rdepth(void) {                      // return stack depth
    return (int)(FetchCell(R0) - RegRead(5)) / 4;
}
int Sdepth(void) {                      // data stack depth
    return (int)(FetchCell(S0) - RegRead(6)) / 4;
}

// Load a ROM image file

char* LoadedFilename;                   // saved filename and format
int LoadedFileType;

int BinaryLoad(char* filename) {        // Load ROM from binary file
    FILE *fp;
    uint8_t data[4];
    int length, i;
    uint32_t n;
    uint32_t addr = 0;
    fp = fopen(filename, "rb");
    if (!fp) { return -1; }             // bad filename
    LoadedFilename = filename;          // save in case we want to reload the file
    LoadedFileType = 1;
    do {
        memset(data, 255, 4);
        length = fread(data, 1, 4, fp); // get 4 bytes of data
        n = 0;
        for (i = 0; i < 4; i++) {       // make little-endian word
            n += data[i] << (8 * i);
        }
        WriteROM(n, addr);              // ignore ior
        addr += 4;
    } while (length == 4);
    fclose(fp);
    return 0;
}

void ReloadFile(void) {                 // Reload known ROM image file
    switch(LoadedFileType) {
        case 1: BinaryLoad(LoadedFilename);
        default: break;
    }
}

////////////////////////////////////////////////////////////////////////////////
/// Debugger Display uses full screen with VT100 commands
/// It does not work with the old Windows CMD console.
/// Use a real terminal like ConEmu or Linux.
/// Starting from Windows 10 TH2 (v1511), conhost.exe and cmd.exe support ANSI
/// and VT100 Escape Sequences out of the box (although they have to be enabled).
////////////////////////////////////////////////////////////////////////////////

void SetCursorPosition(int X, int Y){
    printf("\033[%d;%df", Y, X);
}

void CellDump(int length, uint32_t addr) {  // DUMP
    uint32_t line[8];                       // buffer for ASCII
    uint8_t c;  int i, j, pad, len;
    if (length>4096) length=4096;           // limit to reasonable length
    addr = (addr+3) & 0xFFFFFFFC;           // align
    while (length) {
        printf("\n%04X ", addr);            // address
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
            printf("    ");
        }
        length -= len;
    }
}

void DumpDataStack(void){
    int i;  int row = 2;
    int SP0 = FetchCell(S0);
    int depth = Sdepth();
    int overflow = depth + 2 - DumpRows;
    if (depth<0) {
        SetCursorPosition(DataStackCol, row++);
        printf("Underflow");
    } else {
        if (overflow >= 0) {            // limit depth
            SP0 -= overflow * 4;
            depth -= overflow;
        }
        for (i=depth-1; i>=0; i--) {
            SetCursorPosition(DataStackCol, row++);
            printf("  %08X", FetchCell(SP0 - 4*depth + i*4));
        }
    }
    SetCursorPosition(DataStackCol, row++);
    printf("N %d", RegRead(1));
    SetCursorPosition(DataStackCol, row++);
    printf("T %d", RegRead(0));
}

void DumpReturnStack(void){
    int i;   int row = 2;
    int RP0 = FetchCell(R0);
    int depth = Rdepth();
    int overflow = depth + 1 - DumpRows;
    if (depth<0) {
        SetCursorPosition(ReturnStackCol, row++);
        printf("Underflow");
    } else {
        if (overflow >= 0) {            // limit depth
            RP0 -= overflow * 4;
            depth -= overflow;
        }
        for (i=depth-1; i>=0; i--) {
            SetCursorPosition(ReturnStackCol, row++);
            printf("  %08X", FetchCell(RP0 - 4*depth + i*4));
        }
    }
    SetCursorPosition(ReturnStackCol, row++);
    printf("R %d", RegRead(2));
}

void DisassembleIR(uint32_t IR) {
    int slot = 26;  // 26 20 14 8 2
    int opcode;
    char name[64][6] = {
    "NOOP", "DUP",  ";",    "+",   "NO|",   "R@",   ";|",    "AND",
    "NIF|", "OVER", "R>",   "XOR", "IF|",   "A",    "RDROP", "---",
    "+IF|", "!AS",  "@A",   "---", "-IF|",  "2*",   "@A+",   "---",
    "NEXT", "U2/",  "W@A",  "A!",  "REPT|", "2/",   "C@A",   "B!",
    "SP",   "COM",  "!A",   "RP!", "RP",    "PORT", "!B+",   "SP!",
    "UP",   "---",  "W!A",  "UP!", "---",   "---",  "C!A",   "---",
    "USER", "---",  "---",  "NIP", "JUMP",  "---",  "@AS",   "---",
    "LIT",  "---",  "DROP", "ROT", "CALL",  "1+",   ">R",    "SWAP"
    };
    while (slot>=0) {
        opcode = (IR >> slot) & 0x3F;
NextOp: if (opcode>31)
            if ((opcode&3)==0) { // immediate opcode uses prefix format
                printf("0x%X ", IR & ~(0xFFFFFFFF << slot));
                slot=0;
            }
        printf("%s ", name[opcode]);
        slot -= 6;
    }
    if (slot == -4) {
        opcode = IR & 3;
        goto NextOp;
    }
}

void DumpROM(void) {
    int row = 2;  int i;
    static uint32_t ROMorigin;          // first ROM address
    uint32_t PC = RegRead(8);
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
        if (PC == (addr)) {
            printf("\033[4m");          // hilight
        }
        printf("%04X %08X ", addr, FetchCell(addr));
        DisassembleIR(FetchCell(addr));
        printf("\033[0m");              // default FG/BG
        addr += 4;
    }
}

void DumpRegs(void) {
    int row = 2;
    char name[9][4] = {" T", " N", " R", " A", " B", "RP", "SP", "UP", "PC"};
    char term[9][10] = {"STATUS", "FOLLOWER", "S0", "R0", "HANDLER", "BASE",
                        "Head: HP", "Code: CP", "Data: DP"};
    uint32_t i;
    printf("\033[s");                   // save cursor
    printf("\033[%d;0H", DumpRows+2);
    printf("\033[1J\033[H");            // erase to top of screen, home cursor
    printf("\033[4m");                  // hilight header
    printf("Data Stack  ReturnStack Registers  Terminal Vars  ");
    printf("ROM at PC     Instruction Disassembly"); // ESC [ <n> m
    printf("\033[0m");                  // default FG/BG

    DumpDataStack();
    DumpReturnStack();
    for (i=3; i<9; i++) {               // internal VM registers
#ifndef TRACEABLE
        if (i != 4) {  // can't read B so skip it
#endif
            SetCursorPosition(RegistersCol, row++);
            printf("%s %X", name[i], RegRead(i));
#ifndef TRACEABLE
        }
#endif
    }
    row = 2;
    for (i=0; i<9; i++) {               // terminal task USER variables
        SetCursorPosition(RAMdumpCol + 5 - strlen(term[i]), row++);
        printf("%s %X", term[i], FetchCell(STATUS + i*4));
    }
    DumpROM();
    printf("\033[u");                   // restore cursor
}

/// Keyboard input is raw, from tiffEKEY().
/// Use single keystrokes to dump various parameters, calculator-style.
/// Note: IDE may cook keyboard input, you should run from the command line.

void Bell(int flag) {
    if (flag) printf("\a");
}

uint32_t Param = 0;     // parameter

void ShowParam(void){
    SetCursorPosition(0, DumpRows+2);
    printf("%08X\n", Param);
    DisassembleIR(Param);
    printf("\033[K\n");  // erase to end of line
}

void vmTEST (void) {
    int c;
    printf("\033[2J");                  // CLS
Re: DumpRegs();
    SetCursorPosition(0, DumpRows+6);   // help along the bottom
    printf("(0..F)=digit, Enter=Clear, O=pOp, P=Push, R=Refresh, X=eXecute, \\=POR, ESC=Bye\n");
    #ifdef TRACEABLE
    printf("G=Goto, S=Step, @=Fetch, U=dUmp, W=WipeHistory, Y=Redo, Z=Undo \n");
    #else
    printf("G=Goto, S=Step, @=Fetch, U=dUmp\n");
    #endif
    while (1) {
        ShowParam();
        c = tiffEKEY();
        if (isxdigit(c)) {
            if (c>'9') c-=('A'-10); else c-='0';
            Param = Param*16 + (c&0x0F);
        } else {
            switch (c) {
                case 27: SetCursorPosition(0, 17); return; // ESC = bye
                case 13:  Param=0;  break;                 // ENTER = clear
                case 'p':
                case 'P': PushNum(Param);   goto Re;       // P = Push
                case 'o':
                case 'O': Param = PopNum();  goto Re;      // O = Pop
                case 'r':
                case 'R': goto Re;                         // R = Refresh
                case 'u':
                case 'U': CellDump(32, Param);   break;    // U = dump
                case 'g':
                case 'G': SetPCreg(Param);   goto Re;      // G = goto
                case ' ':
                case 's': // execute instruction from ROM  // S = Step
                case 'S': Tracing=1;  VMstep(FetchCell(RegRead(8)),0);
                          Tracing=0;  goto Re;
                case 'x': // execute instruction from Param (doesn't step PC)
                case 'X': Tracing=1;  VMstep(Param,1);     // X = Execute
                          Tracing=0;  goto Re;
                case '@': Param = FetchCell(Param); break; // @ = Fetch
                case '\\': InitializeTermTCB();
                          ReloadFile();  goto Re;          // \ = Reset
#ifdef TRACEABLE
                case 'h':
                case 'H': TraceHist();   break;            // H = history
                case 'w':
                case 'W': uHead=0; uTail=0; uHere=0; break; // W = wipe history
                case 'y':
                case 'Y': Bell(RedoTrace());   goto Re;    // Y = Redo
                case 'z':
                case 'Z': Bell(UndoTrace());   goto Re;    // Z = Undo
#endif
                default: printf("%d   ", c);
            }
        }
    }
}
