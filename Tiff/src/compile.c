
#include <stdio.h>
#include <stdlib.h>
#include "vm.h"
#include "vmaccess.h"
#include "compile.h"
#include "tiff.h"
#include "colors.h"
#include <string.h>
#define RunLimit 1000000

/// The compiler keeps all state inside the VM when compiling code
/// so that executable code can take over later by redirecting
/// the header's xtc and xte.

static void FlushLit (void);            // forward reference
uint32_t DbgPC;                         // shared with vmaccess.c
uint32_t OpcodeCount[64];               // static instruction count

static char names[64][6] = {
    ".",     "dup",  "exit",  "+",    "user", "drop", "r>",  "2/",
    "?",     "1+",   "swap",  "-",    "?",    "c!+",  "c@+", "u2/",
    "no:",   "2+",   "-bran", "jmp",  "?",    "w!+",  "w@+", "and",
    "?",     "litx", ">r",    "call", "?",    "0=",   "w@",  "xor",
    "rept",  "4+",   "over",  "?",    "?",    "!+",   "@+",  "2*",
    "-rept", "?",    "rp",    "?",    "?",    "rp!",  "@",   "2*c",
    "-if:",  "?",    "sp",    "@as",  "?",    "sp!",  "c@",  "port",
    "+if",   "lit",  "up",    "!as",  "?",    "up!",  "r@",  "com"
};

char * OpName(unsigned int opcode) {
    return names[opcode&0x3F];
}

// Determine if opcode uses immediate data
static int isImmediate(unsigned int opcode) {
    switch (opcode) {
        case opLitX:
        case opLIT:
        case opFetchAS:
        case opStoreAS:
        case opUSER:
        case opMiBran:
        case opCALL:
        case opJUMP: return 1;
        default: return 0;
    }
}
// Determine if opcode uses immediate address
static int isImmAddress(unsigned int opcode) {
    switch (opcode) {
        case opMiBran:
        case opCALL:
        case opJUMP: return 1;
        default: return 0;
    }
}

void DisassembleIR(uint32_t IR) {
    int slot = 26;  // 26 20 14 8 2
    int opcode;
    while (slot>=0) {
        opcode = (IR >> slot) & 0x3F;
        NextOp: if (isImmediate(opcode)) {
            uint32_t imm = IR & ~(0xFFFFFFFF << slot);
            if (isImmAddress(opcode)) {
                char *name = GetXtName(imm<<2);
                if (name) {
                    ColorCompiled();
                    printf("%s ", name);
                } else {
                    ColorImmAddress();
                    printf("0x%X ", imm<<2);
                }
            } else {
                ColorImmediate();
                printf("0x%X ", imm);
            }
            slot=0;
        }
        ColorOpcode();
        printf("%s ", OpName(opcode));
        slot -= 6;
    }
    if (slot == -4) {
        opcode = IR & 3;
        goto NextOp;
    }
    ColorNormal();
}

void ListOpcodeCounts(void) {           // list the static profile
    int i;                              // in csv format
    for (i=0; i<64; i++){
        printf("\n%d,\"%s\",%d", i, OpName(i), OpcodeCount[i]);
    }   printed = 1;
}

void InitIR (void) {                    // initialize the IR
    StoreCell(0, IRACC);
    StoreHalf(26, SLOT);                // SLOT=26, LITPEND=0
    StoreByte(0, CALLED);
}

static void AppendIR (unsigned int opcode, uint32_t imm) {
    uint32_t ir = FetchCell(IRACC);
    ir = ir + (opcode << FetchByte(SLOT)) + imm;
    StoreCell(ir, IRACC);
    OpcodeCount[opcode&0x3F]++;
}

// Append a zero-operand opcode
// Slot positions are 26, 20, 14, 8, 2, and 0

static void Implicit (int opcode) {
    FlushLit();
    if ((FetchByte(SLOT) == 0) && (opcode > 3))
        NewGroup();                     // doesn't fit in the last slot
    AppendIR(opcode, 0);
    int8_t slot = FetchByte(SLOT);
    if (slot) {
        slot -= 6;
        if (slot < 0) slot = 0;
        StoreByte(slot, SLOT);
    } else {
        NewGroup();
    }
}

// Finish off IR with the largest literal that will fit.
// All literals are unsigned.

static void Explicit (int opcode, uint32_t n) {
    FlushLit();
    int8_t slot = FetchByte(SLOT);
    if ((n >= (1 << slot)) || (slot == 0))
        NewGroup();                     // it doesn't fit
    AppendIR(opcode, n);
    StoreCell((FetchByte(SLOT) << 24) + FetchCell(CP), CALLADDR);
    StoreByte(0, SLOT);
    NewGroup();
}

void NewGroup (void) {                  // finish the group and start a new one
    FlushLit();                         // if a literal is pending, compile it
    uint8_t slot = FetchByte(SLOT);
    if (slot > 20) return;              // already at first slot
    if (slot > 0) Implicit(opSKIP);     // skip unused slots
    CommaC(FetchCell(IRACC));
    InitIR();
}

////////////////////////////////////////////////////////////////////////////////
// Tail-call optimization requires knowing the address of the instruction
// containing the call as well as its slot number. It makes certain assumptions
// about opcodes. CALL is 074, JUMP is 064 octal (111100 and 110100 binary).
// See tiffCOLON.

// notail not implemented yet

static void CompCall (uint32_t addr, int notail) {
    Explicit(opCALL, addr/4);
    if (notail) return;
    StoreByte(1, CALLED);
}

static void Compile (uint32_t addr) {   // the normal compile
    CompCall (addr, 0);
}

// Definitions that only use implicit opcodes are very easy to compile as
// macros.

static void CompileMacro(uint32_t addr) { // compile as a macro
    int i;  int opcode;  uint32_t ir;
    int slot = 26;
    for (i=0; i<20; i++) {                // limit length of macro
        if (slot == 26) {
            ir = FetchCell(addr);
            addr += 4;
        }
        if (slot < 0) {
            opcode = ir & 3;                // slot = -4
            slot = 26;
        } else {
            opcode = (ir >> slot) & 0x3F;   // slot = 26, 20, 14, 8, 2
            slot -= 6;
        }
        if (opcode == opEXIT) return;
        Implicit(opcode);
    } tiffIOR = -61;
}

// ; does several things:
// 1. Attempt to convert the last call to a jump.
// 2. Otherwise, append an EXIT and end the group.
// 3. Unsmudge the current header if it's smudged.
// 4. if COLONDEF, resolve the code length.
// 5. Return to EXECUTE mode.

void SemiExit (void) {  /*EXPORT*/
	if (FetchByte(CALLED)) {            // ca = packed slot and address
        uint32_t ca = FetchCell(CALLADDR);
        uint32_t addr = ca & 0xFFFFFF;
        uint8_t slot = ca >> 24;
        uint32_t mask = ~(8 << slot);
        StoreROM(mask, addr);           // convert CALL to JUMP
        StoreCell(0, CALLADDR);         // clear ca
        StoreByte(0, CALLED);
	} else {
	    Implicit(opEXIT);
	}
}

void Semicolon (void) {  /*EXPORT*/
    SemiExit();  NewGroup();
	uint32_t wid = FetchCell(CURRENT);
	wid = FetchCell(wid);               // -> current definition
    uint32_t name = FetchCell(wid + 4);
    if (name & 0x20) {                  // if the smudge bit is set
        StoreROM(~0x20, wid + 4);       // then clear it
    }
    // The code address is in the header at wid-16.
    if (FetchByte(COLONDEF)) {          // resolve length of definition
        StoreByte(0, COLONDEF);
        uint32_t org = FetchCell(wid-4);  // start address = xte
        uint32_t cp  = FetchCell(CP);     // end address
        uint32_t length = (cp - org) / 4; // length in cells
        if (length>255) length=255;       // limit to 8-bit
        StoreROM((length<<24) + 0xFFFFFF, wid);
    }
    StoreCell(0, STATE);
}

void tiffMACRO (void) {  /*EXPORT*/
    uint32_t wid = FetchCell(CURRENT);
    wid = FetchCell(wid);               // -> current definition
    StoreROM(~4, wid - 8);              // flip xtc from compile to macro
}

void tiffCALLONLY (void) {  /*EXPORT*/
    uint32_t wid = FetchCell(CURRENT);
    wid = FetchCell(wid);               // -> current definition
    StoreROM(~0x80, wid + 4);           // clear the "call only" bit
}

void tiffANON (void) {  /*EXPORT*/
    uint32_t wid = FetchCell(CURRENT);
    wid = FetchCell(wid);               // -> current definition
    StoreROM(~0x40, wid + 4);           // clear the "anonymous" bit
}

////////////////////////////////////////////////////////////////////////////////

static void HardLit (int32_t N) {
    uint32_t u = abs(N);
    if (u < 0x03FFFFFF) {               // single width literal
        if (N < 0) {
            Explicit(opLIT, u-1);       // negative number
            Implicit(opCOM);
        } else {
            Explicit(opLIT, u);
        }
    } else {
        Explicit(opLIT, (N>>24));       // split into 8 and 24
        Explicit(opLitX, (N & 0xFFFFFF));
    }
}

static void FlushLit(void) {            // compile a literal if it's pending
    if (FetchByte(LITPEND)) {
        StoreByte(0, LITPEND);
        HardLit(FetchCell(NEXTLIT));
    }
    StoreByte(0, CALLED);               // everything clears the call tail
}

void Literal (uint32_t N) { /*EXPORT*/  // cache the newest literal
    FlushLit();
    if (N < 0x4000000) {
        StoreCell(N, NEXTLIT);
        StoreByte(1, LITPEND);
    } else HardLit(N);                  // large literal doesn't cache
}

static void Immediate (uint32_t op) {   // expecting cached immediate data
    if (FetchByte(LITPEND)) {
        StoreByte(0, LITPEND);
        Explicit(op, FetchCell(NEXTLIT));
    } else tiffIOR = -59;
}

static void SkipOp (uint32_t op) {      // put a skip opcode the next valid slot
    FlushLit();
	if (FetchByte(SLOT) < 8) NewGroup();
	Implicit(op);
}

static void HardSkipOp (uint32_t op) {  // put a skip opcode in a new group
	NewGroup();
	Implicit(op);
}

static void FakeIt (int opcode) {       // execute an opcode in the VM
    DbgGroup(opcode, opSKIP, opNOP, opNOP, opNOP);
}

// Compile branches: | 0= -bran <20-bit> |

void NoExecute (void) {
    if (!FetchCell(STATE)) tiffIOR = -14;
}

void CompIf (void){
    NewGroup();  NoExecute();
    PushNum(FetchCell(CP));
    Implicit(opZeroEquals);
    Explicit(opMiBran, 0xFFFFF);
}
void CompThen (void){
    NewGroup();  NoExecute();
    uint32_t mark = PopNum();
    uint32_t dest = FetchCell(CP) >> 2;
    StoreROM(0xFFF00000 + dest, mark);
}

// compile a forward reference

void CompDefer (void){
    NewGroup();
    Explicit(opJUMP, 0x3FFFFFF);
}


// Functions invoked after being found, from either xte or xtc.
// Positive means execute in the VM.
// Negative means execute in C.

int32_t breakpoint = -1;

void tiffFUNC (int32_t n) {   /*EXPORT*/
    uint32_t i;
    if (n & 0x800000) n |= 0xFF000000; // sign extend 24-bit n
	if (n<0) { // internal C function
#ifdef VERBOSE
        printf("xt=%X, ht=%X ", n, FetchCell(HEAD));  printed = 1;
#endif
        uint32_t ht = FetchCell(HEAD);
        uint32_t w = FetchCell(ht-16);
		switch(~n) {
			case 0: PushNum (w);    break;
			case 1: Literal (w);    break;
			case 2: FakeIt  (w);    break;  // execute opcode
            case 3: Implicit(w);    break;  // compile implicit opcode
            case 4: tiffIOR = -14;  break;
            case 5: Immediate(w);   break;  // compile immediate opcode
            case 6: SkipOp(w);      break;  // compile skip opcode
            case 7: HardSkipOp(w);  break;  // compile skip opcode in slot 0
            case 8: FlushLit();  NewGroup();   break;  // skip to new instruction group
// Compile xt must be multiple of 8 so that clearing bit2 converts to a macro
            case 16: Compile(FetchCell(ht-4)); break;  // compile call to xte
            case 20: CompileMacro(FetchCell(ht-4)); break;
			default: break;
		}
	} else { // execute in the VM
#ifdef VERBOSE
        printf(", Executing at %X", n);  printed = 1;
#endif // VERBOSE
        SetDbgReg(DbgPC);
        DbgGroup(opDUP, opPORT, opPUSH, opSKIP, opNOP); // Push current PC
        SetDbgReg(0xDEADC0DC);
        DbgGroup(opDUP, opPORT, opPUSH, opSKIP, opNOP); // Push bogus return address
        SetDbgReg(n);
        DbgGroup(opDUP, opPORT, opPUSH, opEXIT, opNOP); // Jump to code to run
        DbgPC = n;
        for (i=1; i<RunLimit; i++) {
            if (DbgPC == breakpoint) {
                DbgPC = vmTEST();
                breakpoint = -1;
            }
            uint32_t ir = FetchCell(DbgPC);
#ifdef VERBOSE
            printf("\nStep %d: IR=%08X, PC=%X", i, ir, DbgPC);
#endif // VERBOSE
            DbgPC = 4 * VMstep(ir, 0);
#ifdef VERBOSE
            printf(" ==> PC=%X", DbgPC);
#endif // VERBOSE
            if (DbgPC == 0xDEADC0DC) {                  // Terminator found
                DbgGroup(opEXIT, opSKIP, opNOP, opNOP, opNOP); // restore PC
#ifdef VERBOSE
                printf("\nFinished after %d groups\n", i);
#endif // VERBOSE
                return;
            }
        }
        printf("\nHit the RunLimit in compile.c");  printed = 1;
	}
}

static void AddImplicit(int opcode, char *name) {
    CommaH(opcode);
    CommaHeader(name, ~2, ~3, 0, 0);
}
static void AddExplicit(int opcode, char *name) {
    CommaH(opcode);
    CommaHeader(name, ~4, ~5, 0, 0);
}
static void AddSkip(int opcode, char *name) {
    CommaH(opcode);
    CommaHeader(name, ~4, ~6, 0, 0);
}
static void AddHardSkip(int opcode, char *name) {
    CommaH(opcode);
    CommaHeader(name, ~4, ~7, 0, 0);
}
void InitCompiler(void) {  /*EXPORT*/   // Initialize the compiler
    InitIR();
    memset(OpcodeCount, 0, 64);         // clear static opcode counters
    CommaHeader("|", ~4, ~8, 0, 0);     // skip to new opcode group
    AddImplicit(opNOP       , "nop");
    AddImplicit(opDUP       , "dup");
//    AddImplicit(opEXIT      , "exit");
    AddImplicit(opADD       , "+");
    AddImplicit(opSUB       , "-");
    AddImplicit(opRfetch    , "r@");
    AddImplicit(opAND       , "and");
    AddImplicit(opOVER      , "over");
    AddImplicit(opPOP       , "r>");
    AddImplicit(opXOR       , "xor");
    AddExplicit(opStoreAS   , "!as");
    AddImplicit(opTwoStar   , "2*");
    AddImplicit(opCstorePlus, "c!+");
    AddImplicit(opCfetchPlus, "c@+");
    AddImplicit(opCfetch    , "c@");
    AddImplicit(opWstorePlus, "w!+");
    AddImplicit(opWfetchPlus, "w@+");
    AddImplicit(opWfetch    , "w@");
    AddImplicit(opStorePlus , "!+");
    AddImplicit(opFetchPlus , "@+");
    AddImplicit(opFetch     , "@");
    AddImplicit(opUtwoDiv   , "u2/");
    AddImplicit(opREPT      , "rept");
    AddImplicit(opMiREPT    , "-rept");
    AddImplicit(opTwoDiv    , "2/");
    AddExplicit(opSP        , "sp");
    AddImplicit(opCOM       , "invert");
    AddImplicit(opSetRP     , "rp!");
    AddExplicit(opRP        , "rp");
    AddImplicit(opPORT      , "port");
    AddImplicit(opSetSP     , "sp!");
    AddExplicit(opUP        , "up");
    AddImplicit(opSetUP     , "up!");
    AddExplicit(opLitX      , "litx");
    AddExplicit(opUSER      , "user");
    AddExplicit(opJUMP      , "jump");
    AddExplicit(opFetchAS   , "@as");
    AddExplicit(opLIT       , "lit");
    AddImplicit(opDROP      , "drop");
    AddExplicit(opCALL      , "call");
    AddImplicit(opOnePlus   , "1+");
    AddImplicit(opTwoPlus   , "2+");
    AddImplicit(opFourPlus  , "cell+");
    AddImplicit(opPUSH      , ">r");
    AddImplicit(opSWAP      , "swap");
    AddSkip    (opSKIP      , "no:");
    AddSkip    (opSKIPLT    , "+if:");
    AddSkip    (opSKIPGE    , "-if:");
    AddHardSkip(opSKIP      , "|no");
    AddHardSkip(opSKIPLT    , "|+if");
    AddHardSkip(opSKIPGE    , "|-if");
    AddImplicit(opZeroEquals, "0=");

}
