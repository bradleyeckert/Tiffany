
#include <stdio.h>
#include <stdlib.h>
#include "vm.h"
#include "vmaccess.h"
#include "compile.h"
#include <string.h>

uint32_t IR;                            // instruction group
int slot;                               // bit position of next opcode
int32_t NextLit;                        // pending literal
int LitPending;

void InitIR (void) {
    IR = 0;  slot = 26;
}

// Finish off IR with the largest literal that will fit.
// All literals are unsigned.

void Explicit (int opcode, uint32_t n) {
    if (n >= (1<<slot)) {               // it doesn't fit
        CommaC(IR);
        InitIR();
    }
    IR |= (opcode << slot) | n;
    CommaC(IR);
    InitIR();
}

// Append a zero-operand opcode

void Implicit (int opcode) {
    FlushLit();
    if (opcode >= (1<<slot)) {          // doesn't fit in last slot
        CommaC(IR);
        InitIR();
    }
    IR |= opcode << slot;
    slot -= 6;
    if (slot<0) {                       // slots used up
        CommaC(IR);
        InitIR();
    }
}

void HardLit (int32_t N) {
    uint32_t u = abs(N);
    if (u < 0x03FFFFFF) {               // single width literal
        Explicit(opLIT, u);
        if (N < 0) {
            Implicit(opCOM);
        }
    } else {
        printf("Lit32:%X ", u);
        Explicit(opLIT, (N>>24));       // split into 8 and 24
        Explicit(opShift24, (N & 0xFFFFFF));
    }
}

void FlushLit(void) {
    if (LitPending) {
        HardLit(NextLit);
        LitPending = 0;
    }
}

void Literal (uint32_t N) {
    FlushLit();
    if (N < 0x4000000) {
        NextLit = N;
        LitPending = 1;
    } else HardLit(N);  // large literal doesn't cache
}

void Immediate (uint32_t op) {
    if (LitPending) {
        Explicit(op, (unsigned)NextLit);
        LitPending = 0;
    } else tiffIOR = -59;
}


void FakeIt (int opcode) {
    DbgGroup(opcode, opSKIP, opNOOP, opNOOP, opNOOP);
}

// Functions invoked after being found, from either xte or xtc.
// Positive means execute in the VM.
// Negative means execute in C.

void tiffFUNC (int n, uint32_t ht) {
    if (n & 0x800000) n |= 0xFF000000; // sign extend 24-bit n
	if (n<0) { // internal C function
        uint32_t w = FetchCell(ht-20);
		switch(~n) {
			case 0: PushNum (w);  break;
			case 1: Literal (w);  break;
			case 2: FakeIt  (w);  break;  // execute opcode
            case 3: Implicit(w);  break;  // compile implicit opcode
            case 4: tiffIOR = -14;  break;
            case 5: Immediate(w);  break; // compile immediate opcode
			default: break;
		}
	} else { // execute in the VM
	}
}
