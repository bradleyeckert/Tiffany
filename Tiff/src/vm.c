#include <stdio.h>
#include <stdlib.h>
#include "vm.h"
#include <string.h>

#define SDUP   RAM[--SP] = N;  N = T
#define SDROP  T = N;  N = RAM[SP++]
#define SNIP   N = RAM[SP++]
#define RDUP   RAM[--RP] = R
#define RDROP  R = RAM[RP++]
#define IMM    (IR & ~(-1<<slot))

/// The VM's RAM and ROM are internal to this module.
/// They are both little endian regardless of the target machine.
/// RAM and ROM are accessed only through execution of an instruction
/// group. Writing to ROM is a special case, depending on a USER function
/// with the restriction that you can't turn '0's into '1's.

/// USER functions pass data in and out using mailbox global DebugReg,
/// write to ROM and erase ROM. These are implemented here. Others are
/// external, called by function number starting at 0.

/// AXI I/O is done through external functions.

// Globals: Mailboxes, strobe (set flag to 1) when read/written.
uint32_t DebugReg;

static uint32_t RAM[RAMsize];	// RAM & ROM are arrays of 32-bit cells.
static uint32_t ROM[ROMsize];

// USER function: Lower IR = opUSER<<14 + func#
static int32_t InternalFn (uint32_t T, uint32_t N, int fn ) {
	uint32_t i;
	switch (fn) {
		case 10000: for (i=0; i<ROMsize; i++)
					    ROM[i]=0xFFFFFFFF;      // erase ROM
                    memset(RAM, 0, sizeof RAM); // erase RAM
                    return 0;
		case 10001: if (T < ROMsize*4) {        // N is byte address
                        if (T & 3) return -23;  // alignment problem
                        i = ROM[T/4];			// ROM address
                        if (~(i&N)) return -60; // not erased
                        ROM[T/4] = i & N;
                        return 0;
                    } else {
                        return -9;              // bad range
                    }
		case 10002:
//			printf("\nDump[%X], %d words", T, N);
			N = N & 0xFF;	// limit to 256 words
			for (i=0; i<N; i++) {
				if (i%8 == 0) printf("\n%04X: ", (T+i*4));
				printf("%08X ", RAM[(T/4)+i]);
			}   printf("\n");
			return 0;
		default: return UserFunction (T, N, fn);
	}
}

/// IR is the instruction group.
/// RunState is 0 when PC post-increments, other when not.

int32_t VMstep(uint32_t IR, int RunState) {

	static uint32_t T = 0;	 static uint8_t RP = 64;
	static uint32_t N = 0;	 static uint8_t SP = 32;
	static uint32_t R = 0;	 static uint8_t UP = 64;
	static uint32_t A = 0;
	static uint32_t B = 0;	 static uint32_t PC = 0;

	uint32_t M; // temporary
	int opcode, slot;
	unsigned int addr, shift;

	slot = 26;
	while (slot>=0) {
		opcode = (IR >> slot) & 0x3F;	   // slot = 26, 20, 14, 8, 2
		switch (opcode) {
			case 000:									break;	// NOOP
			case 001: SDUP;								break;	// DUP
			case 002: RDROP;  PC = R;	slot = -1;		break;	// ;
			case 003: T = T + N;	  SNIP;				break;	// +
			case 004: slot = -1;						break;	// NO|
			case 005: SDUP;   T = R;					break;	// R
			case 006: RDROP;  PC = R;					break;	// ;|
			case 007: T = T & N;	  SNIP;				break;	// AND

			case 010: if (T!=0) slot = -1;				break;	// NIF|
			case 011: M = N;  SDUP;  T = M;				break;	// OVER
			case 012: SDUP;  T = R;  RDROP;				break;	// R>
			case 013: T = T ^ N;	  SNIP;				break;	// XOR
			case 014: if (T==0) slot = -1;				break;	// IF|
			case 015: SDUP;  T = A;						break;	// A
			case 016:				 RDROP;				break;	// RDROP
			case 017:									break;	//

			case 020: if (T<0)	 slot = -1;				break;	// +IF|
			case 021: A += 4*R;							break;	// !AS (faked)
			case 022: SDUP;								        // @A
                addr = A>>2;
				if (addr < ROMsize) {
                    T = ROM[addr];
                } else {
                    addr -= ROMsize;
                    if (addr < RAMsize) {
                        T = RAM[addr];
                    } else {
                        T = 0;
                    }
                }									    break;
			case 023: 									break;
			case 024: if (T>=0)  slot = -1;				break;	// -IF|
			case 025: T = T*2;	        				break;	// 2*
			case 026: SDUP; 									// @A+
                addr = A>>2;
				if (addr < ROMsize) {
                    T = ROM[addr];
                } else {
                    addr -= ROMsize;
                    if (addr < RAMsize) {
                        T = RAM[addr];
                    } else {
                        T = 0;
                    }
                }   A += 4;							    break;
			case 027:									break;	//

			case 030: if (R<0) {slot = -1;}  R--;		break;	// NEXT|
			case 031: T = (unsigned) T / 2;			break;	// U2/
			case 032: SDUP; 									// W@A
                addr = A>>2;  shift = (A&2) * 8;
				if (addr < ROMsize) {
                    T = (ROM[addr] >> shift) & 0xFFFF;
                } else {
					addr -= ROMsize;
					if (addr < RAMsize) {
						T = (RAM[addr] >> shift) & 0xFFFF;
					} else {
						T = 0;
					}
				}									    break;
			case 033: A = T;	 SDROP;					break;	// A!
			case 034: if ((R&0x10000)==0) {slot = 26;}
			    R--;	                                break;	// REPT|
			case 035: T = T / 2;			            break;	// 2/
			case 036: SDUP; 									// C@A
                addr = A>>2;  shift = (A&3) * 8;
				if (addr < ROMsize) {
                    T = (ROM[addr] >> shift) & 0xFF;
                } else {
                    addr -= ROMsize;
                    if (addr < RAMsize) {
                        T = (RAM[addr] >> shift) & 0xFF;
                    } else {
                        T = 0;
                    }
                }									    break;
			case 037: B = T;	 SDROP;					break;	// B!

			case 040: A = (IMM + SP + ROMsize)*4;  	            // SP
                slot = -1;		                        break;
			case 041: T = ~T;							break;	// COM
			case 042: 									        // !A
				addr = (A>>2) - ROMsize;
				if (addr < RAMsize) {
                    RAM[addr] = T;
                }   SDROP;                              break;
			case 043: RP = (uint8_t) (T>>2);  SDROP;	break;	// RP!
			case 044: A = (IMM + RP + ROMsize)*4;  	            // RP
                slot = -1;		                        break;
			case 045: M = T; T=DebugReg; DebugReg=M;	break;	// PORT
			case 046:  									        // !B+
				addr = (B>>2) - ROMsize;
				if (addr < RAMsize*4) {
                    RAM[addr] = T;
                }   SDROP;  B += 4;                     break;
			case 047: SP = (uint8_t) (T>>2);  SDROP;	break;	// SP!

			case 050: A = (IMM + UP + ROMsize)*4;  	            // UP
			    slot = -1;		                        break;
			case 051:                           		break;	//
			case 052:  									        // W!A
                addr = (A>>2) - ROMsize;
                shift = (A&2) * 8;
				if (addr < RAMsize) {
				    M = RAM[addr] & (~(0xFFFF  << shift));
                    RAM[addr] = ((T & 0xFFFF) << shift) | M;
                }   SDROP;                              break;
			case 053: UP = (uint8_t) (T>>2);  SDROP;	break;	// UP!

			case 054: T = T + IMM;   slot = -1;     	break;	// LIT+
			case 055:               					break;	//
			case 056:  									        // C!A
                addr = (A>>2) - ROMsize;
                shift = (A&3) * 8;
				if (addr < RAMsize) {
                    M = RAM[addr] & (~(0xFF  << shift));
                    M = ((T & 0xFF) << shift) | M;
					RAM[addr] = M;
				}   SDROP;                              break;
			case 057:								    break;	//

			case 060: T=InternalFn(T,N,IMM);  slot=-1;  break;  // USER
			case 061:                       			break;	//
			case 062:									break;	//
			case 063: SNIP;								break;	// NIP
			case 064: PC = IMM;  slot = -1;			    break;	// JUMP
			case 065:               					break;	//
			case 066: A += 4*R;				    		break;	// @AS
			case 067: SDUP;  T = IMM;	slot = -1;		break;	// LIT

			case 070: M = N;  N = T;  T = M;		    break;	// SWAP
			case 071:                       			break;	//
			case 072: SDROP;					        break;	// DROP
			case 073: M=RAM[SP];  RAM[SP]=N;  N=T; T=M; break;  // ROT
			case 074: RDUP;  R=PC;  PC=IMM;  slot=-1;   break;	// CALL
			case 075: T = T + 1;            			break;	// 1+
			case 076: RDUP; R=T; SDROP;            	    break;	// R>
			default:                           		    break;	//
		}
		slot -= 6;
	}
	if (RunState) return PC;
	return PC+1;
}
