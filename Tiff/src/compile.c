
#include <stdio.h>
#include <stdlib.h>
#include "vm.h"
#include "vmaccess.h"
#include "compile.h"
#include "tiff.h"
#include <string.h>
#define RunLimit 1000

/// The compiler keeps all state inside the VM when compiling code
/// so that executable code can take over later by redirecting
/// the header's xtc and xte.

static void FlushLit (void);

static void InitIR (void) {             // initialize the IR
    StoreCell(0, IRACC);
    StoreHalf(26, SLOT);                // SLOT=26, LITPEND=0
    StoreByte(0, CALLED);
}

static void AppendIR (int opcode, uint32_t imm) {
    uint32_t ir = FetchCell(IRACC);
    ir = ir + (opcode << FetchByte(SLOT)) + imm;
    StoreCell(ir, IRACC);
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

// Tail-call optimization requires knowing the address of the instruction
// containing the call as well as its slot number. It makes certain assumptions
// about opcodes. CALL is 074, JUMP is 064 octal (111100 and 110100 binary).

// notail not implemented yet

static void CompCall (uint32_t addr, int notail) {
    Explicit(opCALL, addr/4);
    if (notail) return;
    StoreByte(1, CALLED);
}

static void Compile (uint32_t addr) {
    CompCall (addr, 0);
}

// ; does several things:
// 1. Attempt to convert the last call to a jump.
// 2. Otherwise, append an EXIT and end the group.
// 3. Unsmudge the current header if it's smudged.
// 4. Return to EXECUTE mode.

void Semicolon (void) {  /*EXPORT*/
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
	    NewGroup();
	}
	uint32_t wid = FetchCell(CURRENT);
	wid = FetchCell(wid);               // -> current definition
    uint32_t name = FetchCell(wid + 4);
    if (name & 0x80) {                  // if the smudge bit is set
        StoreROM(~0x80, wid + 4);       // then clear it
    }
    StoreCell(0, STATE);
}

static void HardLit (int32_t N) {
    uint32_t u = abs(N);
    if (u < 0x03FFFFFF) {               // single width literal
        Explicit(opLIT, u);
        if (N < 0) {
            Implicit(opCOM);
        }
    } else {
        Explicit(opLIT, (N>>24));       // split into 8 and 24
        Explicit(opShift24, (N & 0xFFFFFF));
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

// Functions invoked after being found, from either xte or xtc.
// Positive means execute in the VM.
// Negative means execute in C.

void tiffFUNC (int n, uint32_t ht) {   /*EXPORT*/
    uint32_t i;
    if (n & 0x800000) n |= 0xFF000000; // sign extend 24-bit n
	if (n<0) { // internal C function
        uint32_t w = FetchCell(ht);
		switch(~n) {
			case 0: PushNum (w);    break;
			case 1: Literal (w);    break;
			case 2: FakeIt  (w);    break;  // execute opcode
            case 3: Implicit(w);    break;  // compile implicit opcode
            case 4: tiffIOR = -14;  break;
            case 5: Immediate(w);   break;  // compile immediate opcode
            case 6: SkipOp(w);      break;  // compile skip opcode
            case 7: HardSkipOp(w);  break;  // compile skip opcode in slot 0
            case 8: FlushLit();  NewGroup();  break; // skip to new instruction group
            case 9: Compile(w);     break;  // compile call to w
			default: break;
		}
	} else { // execute in the VM
        SetDbgReg(DbgPC);
        DbgGroup(opDUP, opPORT, opPUSH, opSKIP, opNOP); // Push current PC
        SetDbgReg(0xDEADC0DE);
        DbgGroup(opDUP, opPORT, opPUSH, opSKIP, opNOP); // Push bogus return address
        SetDbgReg(n);
        DbgGroup(opDUP, opPORT, opPUSH, opEXIT, opNOP); // Jump to code to run
        for (i=0; i<RunLimit; i++) {
            uint32_t NextPC = VMstep(FetchCell(DbgPC), 1);
            if (NextPC == 0xDEADC0DE) {                 // Terminator found
                DbgGroup(opEXIT, opSKIP, opNOP, opNOP, opNOP); // restore PC
                return;
            }
        }
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
    CommaHeader("|", ~4, ~8, 0, 0);     // skip to new opcode group
    AddImplicit(opNOP    , "nop");
    AddImplicit(opDUP    , "dup");
    AddImplicit(opEXIT   , "exit");
    AddImplicit(opADD    , "+");
    AddImplicit(opR      , "r@");
    AddImplicit(opAND    , "and");
    AddImplicit(opOVER   , "over");
    AddImplicit(opPOP    , "r>");
    AddImplicit(opXOR    , "xor");
    AddImplicit(opA      , "a");
    AddImplicit(opRDROP  , "rdrop");
    AddImplicit(opStoreAS, "!as");
    AddImplicit(opFetchA , "@a");
    AddImplicit(opTwoStar, "2*");
    AddImplicit(opFetchAplus, "@a+");
    AddImplicit(opRSHIFT1, "u2/");
    AddImplicit(opWfetchA, "w@a");
    AddImplicit(opSetA   , "a!");
    AddImplicit(opREPT   , "rept");
    AddImplicit(opTwoDiv , "2/");
    AddImplicit(opCfetchA, "c@a");
    AddImplicit(opSetB   , "b!");
    AddExplicit(opSP     , "sp");
    AddImplicit(opCOM    , "com");
    AddImplicit(opStoreA , "!a");
    AddImplicit(opSetRP  , "rp!");
    AddExplicit(opRP     , "rp");
    AddImplicit(opPORT   , "port");
    AddImplicit(opStoreBplus, "!b+");
    AddImplicit(opSetSP  , "sp!");
    AddExplicit(opUP     , "up");
    AddImplicit(opWstoreA, "w!a");
    AddImplicit(opSetUP  , "up!");
    AddExplicit(opShift24, "sh24");
    AddImplicit(opCstoreA, "c!a");
    AddExplicit(opUSER   , "user");
    AddImplicit(opNIP    , "nip");
    AddExplicit(opJUMP   , "jump");
    AddImplicit(opFetchAS, "@as");
    AddExplicit(opLIT    , "lit");
    AddImplicit(opDROP   , "drop");
    AddImplicit(opROT    , "rot");
    AddExplicit(opCALL   , "call");
    AddImplicit(opOnePlus, "1+");
    AddImplicit(opPUSH   , ">r");
    AddImplicit(opSWAP   , "swap");
    AddSkip    (opSKIP   , "no:");
    AddSkip    (opSKIPNZ , "nif:");
    AddSkip    (opSKIPZ  , "if:");
    AddSkip    (opSKIPMI , "+if:");
    AddSkip    (opSKIPGE , "-if:");
    AddSkip    (opNEXT   , "next");
    AddHardSkip(opSKIP   , "no|");
    AddHardSkip(opSKIPNZ , "nif|");
    AddHardSkip(opSKIPZ  , "if|");
    AddHardSkip(opSKIPMI , "+if|");
    AddHardSkip(opSKIPGE , "-if|");
}

