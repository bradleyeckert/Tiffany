
#include <stdio.h>
#include <stdlib.h>
#include "vm.h"
#include "vmaccess.h"
#include <string.h>

void Literal (uint32_t N) {
    printf("LIT:%d", N);
}

// Functions invoked after being found, from either xte or xtc.
// Positive means execute in the VM.
// Negative means execute in C.

void tiffFUNC (int n, uint32_t W) {
    if (n & 0x800000) n |= 0xFF000000; // sign extend 24-bit n
	if (n<0) { // internal C function
		switch(~n) {
			case 0: PushNum(W);  break;
			case 1: Literal(W);  break;
			default: break;
		}
	} else { // execute in the VM
	}
}
