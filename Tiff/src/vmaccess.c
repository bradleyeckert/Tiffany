#include <stdio.h>
#include <stdlib.h>
#include "vm.h"
// #include "tiff.h"
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
uint32_t PopNum (void) {   // Pop from the stack
    return DbgGroup(opPORT, opDROP, opSKIP, opNOOP, opNOOP);
}
void PushNum (uint32_t N) {   // Push to the stack
    SetDbgReg(N);
    DbgGroup(opDUP, opPORT, opSKIP, opNOOP, opNOOP);
}
uint32_t FetchCell (uint32_t addr) {  // Read from RAM or ROM
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
uint8_t FetchByte (uint32_t addr) {  // Read from RAM or ROM
    SetDbgReg(addr);
    DbgGroup(opA, opDUP, opPORT, opSetA, opNOOP);
    return (uint8_t) DbgGroup(opCfetchA, opPORT, opDROP, opSetA, opNOOP);;
}
uint16_t FetchHalf (uint32_t addr) {  // Read from RAM or ROM
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

// depreciated, fix up
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

/// Get registers the easy way if TRACEABLE, the hard way if not.
/// Note: The B register is not readable by the debugger (there's no opcode for it)
#ifdef TRACEABLE
    extern uint32_t VMreg[10];
    uint32_t RegRead(int ID) {
        switch(ID) {
            case 0:
            case 1:
            case 2:
            case 3: return VMreg[ID];
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

// Debugger Display uses full screen with VT100 commands

void SetCursorPosition(int X, int Y){
    printf("\033[%d;%df", Y, X);
}

void DumpDataStack(void){
    int i;
    int row = 2;
    int depth = (dbgSP0 - RegRead(6))/4;
    if (depth<0) {
        SetCursorPosition(0, row++);
        printf("Underflow");
    } else {
        if (depth>7) depth=7;         // limit displayable
        for (i=depth-1; i>=0; i--) {
            SetCursorPosition(0, row++);
            printf("    %08X", FetchCell(dbgSP0 - 4*depth + i*4));
        }
    }
    SetCursorPosition(0, row++);
    printf("N = %d", RegRead(1));
    SetCursorPosition(0, row++);
    printf("T = %d", RegRead(0));
}

void DumpReturnStack(void){
    int i;
    int row = 2;
    int depth = (dbgRP0 - RegRead(5))/4;
    if (depth<0) {
        SetCursorPosition(15, row++);
        printf("Underflow");
    } else {
        if (depth>8) depth=8;
        for (i=depth-1; i>=0; i--) {
            SetCursorPosition(15, row++);
            printf("    %08X", FetchCell(dbgRP0 - 4*depth + i*4));
        }
    }
    SetCursorPosition(15, row++);
    printf("R = %d", RegRead(2));
}

void DumpIR(uint32_t IR) {
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
                printf("0x%X ", IR & ~(0xFFFFFFFF << opcode));
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
    uint32_t addr = RegRead(8); // PC
    for (i=0; i<9; i++) {
        SetCursorPosition(41, row++);
        printf("%04X %08X ", addr, FetchCell(addr));
        DumpIR(FetchCell(addr));
        addr += 4;
    }
}

void DumpRegs(void) {
    int row = 2;
    char name[9][4] = {" T", " N", " R", " A", " B", "RP", "SP", "UP", "PC"};
    int i;
    printf("\033[2J");                  // clear screen
    printf("Data Stack    Return Stack  Registers   ROM at PC     Disassembly");
    DumpDataStack();
    DumpReturnStack();
    for (i=0; i<9; i++) {
        SetCursorPosition(29, row++);
        printf("%s = %Xh", name[i], RegRead(i));
    }
    DumpROM();
}


/// Keyboard input is raw, from tiffEKEY().
/// Use single keystrokes to dump various parameters, calculator-style.
/// Note: IDE may cook keyboard input, you should run from the command line.


uint32_t Param = 0;     // parameter

void ShowParam(void){
    SetCursorPosition(0, 11);
    printf("%08X\n", Param);
    DumpIR(Param);
    printf("\033[K\n");  // erase to end of line
}

void vmTEST (void) {
    int c;
    VMpor();
    EraseROM();
Re: DumpRegs();
    SetCursorPosition(0, 15);           // help along the bottom
    printf("(0..F)=digit, -=Clear, O=Pop, P=Push, S=Step, ");
    printf("X=Execute, @=Fetch, \\=POR, ESC=Bye\n");

    while (1) {
        ShowParam();
        c = tiffEKEY();
        if (isxdigit(c)) {
            if (c>'9') c-=('A'-10); else c-='0';
            Param = Param*16 + (c&0x0F);
        } else {
            switch (c) {
                case 27: SetCursorPosition(0, 16); return; // ESC = bye
                case '-':  Param=0;  break;                // ^C = clear
                case 4:  CellDump(32, Param);   break;     // ^D = dump

                case 'p':
                case 'P': PushNum(Param);   goto Re;       // P = Push
                case 'o':
                case 'O': Param = PopNum();  goto Re;      // O = Pop
                case 'x':
                case 'X': VMstep(Param,0);   goto Re;      // X = Execute
                case '@': Param = FetchCell(Param); break; // @ = Fetch
                case 23: StoreCell(Param, 1024);   break;  // ^W = Write
                case 24: StoreByte(Param, 100);    break;  // ^X = Write8
                case 25: Param = FetchByte(Param); break;  // ^Y = Read
                case 26: StoreByte(Param, 1024);   break;  // ^Z = Write
                case '\\': VMpor();   goto Re;             // \ = Reset
                default:
                    printf("%d   ", c);
            }
        }
    }
}
