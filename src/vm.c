#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "vm.h"
#include "vmUser.h"
#include "vmHost.h"
#include "flash.h"
#include <string.h>

// This file serves as the official specification for the Mforth VM.
// It is usable as a template for generating a C testbench or embedding in an app.

//`0`#define ROMsize `3`
//`0`#define RAMsize `4`
//`0`#define SPIflashBlocks `5`
//`0`#define EmbeddedROM

#define IMM     (IR & ~(-1<<slot))

#ifndef TRACEABLE
// Useful macro substitutions if not tracing
#define SDUP()  RAM[--SP & (RAMsize-1)] = N;  N = T
#define SDROP() T = N;  N = RAM[SP++ & (RAMsize-1)]
#define SNIP()  N = RAM[SP++ & (RAMsize-1)]
#define RDUP(x) RAM[--RP & (RAMsize-1)] = x
#define RDROP() RAM[RP++ & (RAMsize-1)]
#endif // TRACEABLE

/* -----------------------------------------------------------------------------
    Globals:
        tiffIOR
        If TRACEABLE: VMreg[], OpCounter[], ProfileCounts[], cyclecount, maxRPtime, maxReturnPC
    Exports:
        VMpor, VMstep, vmMEMinit, SetDbgReg, GetDbgReg, vmRegRead,
        FetchCell, FetchHalf, FetchByte, StoreCell, StoreHalf, StoreByte,
        In not embedded: WriteROM

    Addresses are VM byte addresses
*/

/*global*/ int tiffIOR;                 // error code for the C-based QUIT loop
#ifndef EmbeddedROM
/*global*/ uint32_t RAMsize = RAMsizeDefault;
/*global*/ uint32_t ROMsize = ROMsizeDefault;
/*global*/ uint32_t SPIflashBlocks = FlashBlksDefault;
#else
char * LoadFlashFilename = NULL;
#endif

static int exception = 0;               // local error code

//`0`static const uint32_t InternalROM[`2`] = {`10`};
//`0`uint32_t FetchROM(uint32_t addr) {
//`0`  if (addr < `2`) {
//`0`    return InternalROM[addr];
//`0`  }
//`0`  return -1;
//`0`}

/// Virtual Machine for 32-bit MachineForth.

/// The optional Trace function tracks state changes using these parameters:
/// Type of state change: 0 = unmarked, 1 = new opcode, 2 or 3 = new group;
/// Register ID: Complement of register number if register, memory if other;
/// Old value: 32-bit.
/// New value: 32-bit.

#ifdef EmbeddedROM
    static uint32_t RAM[RAMsize];
#else
    static uint32_t * ROM;
    static uint32_t * RAM;
#endif // EmbeddedROM

#ifdef TRACEABLE
    #define T  VMreg[0]
    #define N  VMreg[1]
    #define RP VMreg[2]
    #define SP VMreg[3]
    #define UP VMreg[4]
    #define PC VMreg[5]
    #define DebugReg VMreg[6]
    #define CARRY    VMreg[7]
    #define RidT   (-1)
    #define RidN   (-2)
    #define RidRP  (-3)
    #define RidSP  (-4)
    #define RidUP  (-5)
    #define RidPC  (-6)
    #define RidDbg (-7)
    #define RidCY  (-8)
    #define VMregs 10

    uint32_t VMreg[VMregs];     // registers with undo capability
    uint32_t OpCounter[64];     // opcode counter
    uint32_t * ProfileCounts;
    uint32_t cyclecount;
    uint32_t maxRPtime;   		// max cycles between RETs
    uint32_t maxReturnPC = 0;   // PC where it occurred
    static uint32_t RPmark;

    static int New; // New trace type, used to mark new sections of trace

    static void SDUP(void)  {
        Trace(New,RidSP,SP,SP-1); New=0;
                     --SP;
        Trace(0,SP & (RAMsize-1),RAM[SP & (RAMsize-1)],  N);
                                 RAM[SP & (RAMsize-1)] = N;
        Trace(0, RidN, N,  T);
                       N = T; }
    static void SDROP(void) {
        Trace(New,RidT,T,  N); New=0;
                       T = N;
        Trace(0, RidN, N,  RAM[SP & (RAMsize-1)]);
                       N = RAM[SP & (RAMsize-1)];
        Trace(0,RidSP,SP,SP+1);
                   SP++; }
    static void SNIP(void)  {
        Trace(New,RidN,N,  RAM[SP & (RAMsize-1)]);  New=0;
                       N = RAM[SP & (RAMsize-1)];
        Trace(0,RidSP, SP,SP+1);
                       SP++; }
    static void RDUP(uint32_t x)  {
        Trace(New,RidRP,RP,RP-1); New=0;
                       --RP;
        Trace(0,RP & (RAMsize-1),RAM[RP & (RAMsize-1)],  x);
                                 RAM[RP & (RAMsize-1)] = x; }
    static uint32_t RDROP(void) {
        uint32_t r = RAM[RP & (RAMsize-1)];
        Trace(New,RidRP, RP,RP+1);  New=0;
                         RP++;  return r; }

#else
    static uint32_t T;	    static uint32_t RP = 64;
    static uint32_t N;	    static uint32_t SP = 32;
    static uint32_t PC;     static uint32_t UP = 64;
    static uint32_t CARRY;  static uint32_t DebugReg;

#endif // TRACEABLE

// Generic fetch from ROM or RAM: ROM is at the bottom, RAM is in middle, ROM is at top
static uint32_t FetchX (int32_t addr, int shift, int32_t mask) {
    uint32_t cell;
    if (addr < 0) {
        int addrmask = RAMsize-1;
        cell = RAM[addr & addrmask];
    } else if (addr >= ROMsize) {
        cell = FlashRead(addr << 2);
    } else {
#ifdef EmbeddedROM
        cell = FetchROM(addr);
#else
        cell = ROM[addr];
#endif // EmbeddedROM
    }
    if (mask < 0) return cell;
    uint32_t r = (cell >> shift) & mask;
    return r;
}

// Generic store to RAM only.
static void StoreX (int32_t addr, uint32_t data, int shift, int32_t mask) {
    if (addr < 0) {
        int ra = addr & (RAMsize - 1);
        uint32_t temp = RAM[ra] & (~(mask << shift));
#ifdef TRACEABLE
        temp = ((data & mask) << shift) | temp;
        Trace(New, ra, RAM[ra], temp);  New=0;
        RAM[ra] = temp;
#else
        RAM[ra] = ((data & mask) << shift) | temp;
#endif // TRACEABLE
    } else {
        exception = -9;
    }
}

/// EXPORTS ////////////////////////////////////////////////////////////////////

void vmMEMinit(char * name){            // erase all ROM and flash,
#ifndef EmbeddedROM						// allocate memory if not allocated yet.
    if (NULL == ROM) {
        ROM = (uint32_t*) malloc(MaxROMsize * sizeof(uint32_t));
    }
    if (NULL == RAM) {
        RAM = (uint32_t*) malloc(MaxRAMsize * sizeof(uint32_t));
    }
  #ifdef TRACEABLE
    if (NULL == ProfileCounts) {
        ProfileCounts = (uint32_t*) malloc(MaxROMsize * sizeof(uint32_t));
    }
  #endif
    // initialize actual sizes
    memset(ROM, -1, ROMsize*sizeof(uint32_t));
    memset(RAM,  0, RAMsize*sizeof(uint32_t));
#endif // EmbeddedROM
    FlashInit(LoadFlashFilename);
};

#ifndef EmbeddedROM
void ROMbye (void) {					// free VM memory if it used malloc
    free(ROM);
    free(RAM);
}
#endif // EmbeddedROM

// Unprotected write: Doesn't care what's already there.
// This is a sharp knife, make sure target app doesn't try to use it.
#ifdef EmbeddedROM
int WriteROM(uint32_t data, uint32_t address) {
    return -20;                         // writing to read-only memory
}
#else
int WriteROM(uint32_t data, uint32_t address) {
    uint32_t addr = address >> 2;
    if (address & 3) return -23;        // alignment problem
    if (addr >= (SPIflashBlocks<<10)) return -9;
    if (addr < ROMsize) {
        ROM[addr] = data;
        return 0;
    }
    tiffIOR = FlashWrite(data, address);
    printf("FlashWrite to %X, you should be using SPI flash write (ROM! etc) instead\n", address);
    // writing above ROM space
           tiffIOR = -20;
    return tiffIOR;
}
#endif // EmbeddedROM

uint32_t FetchCell(int32_t addr) {
    if (addr & 3) {
        exception = -23;
    }
	int32_t ca = addr>>2;  // if addr<0, "/4" <> ">>2". weird, huh?
    if (addr < 0) {
        return (RAM[ca & (RAMsize-1)]);
    }
    if (ca < ROMsize) {
#ifdef EmbeddedROM
        return (FetchROM(ca));
#else
        return (ROM[ca]);
#endif // EmbeddedROM
    }
    return (FlashRead(addr));
}

/*
uint32_t FetchCell(int32_t addr) {
    if (addr & 3) {
        exception = -23;
    }
    return FetchX(addr>>2, 0, 0xFFFFFFFF);
}
*/

uint16_t FetchHalf(int32_t addr) {
    if (addr & 1) {
        exception = -23;
    }
    int shift = (addr & 2) << 3;
    return FetchX(addr>>2, shift, 0xFFFF);
}
uint8_t FetchByte(int32_t addr) {
    int shift = (addr & 3) << 3;
    return FetchX(addr>>2, shift, 0xFF);
}

void StoreCell (uint32_t x, int32_t addr) {
    if (addr & 3) {
        exception = -23;
    }
    if (addr < 0) {
        StoreX(addr>>2, x, 0, 0xFFFFFFFF);
        return;
    }
#ifdef EmbeddedROM
    if ((addr < ROMsize*4) || (addr >= (ROMsize+RAMsize)*4)) {
        exception = -20;
        return;
    }
#else
// Simulated ROM bits are checked for blank. You may not write a '0' to a blank bit.
    if (addr < ROMsize*4) {
        uint32_t old = FetchCell(addr);
        exception = WriteROM(old & x, addr);
        if ((old|x) != 0xFFFFFFFF) {
            exception = -60;
            printf("\nStoreCell: addr=%X, old=%X, new=%X, PC=%X ", addr, old, x, PC*4);
        }
        return;
    }
    if (addr >= (ROMsize+RAMsize)*4) {
        FlashWrite(x, addr);
        return;
    }
#endif // EmbeddedROM
    StoreX(addr>>2, x, 0, 0xFFFFFFFF);
}

void StoreHalf (uint16_t x, int32_t addr) {
    if (addr & 1) {
        exception = -23;
    }
    int shift = (addr & 2) << 3;
    StoreX(addr>>2, x, shift, 0xFFFF);
}
void StoreByte (uint8_t x, int32_t addr) {
    int shift = (addr & 3) << 3;
    StoreX(addr>>2, x, shift, 0xFF);
}

#ifdef TRACEABLE
    // Untrace undoes a state change by restoring old data
    void UnTrace(int32_t ID, uint32_t old) {  // EXPORTED
        int idx = ~ID;
        if (ID<0) {
            if (idx < VMregs) {
                VMreg[idx] = old;
            }
        } else {
            StoreX(ID, old, 0, 0xFFFFFFFF);
        }
    }
#endif // TRACEABLE

////////////////////////////////////////////////////////////////////////////////
/// Access to the VM is through four functions:
///    VMstep       // Execute an instruction group
///    VMpor        // Power-on reset
///    SetDbgReg    // write to the debug mailbox
///    GetDbgReg    // read from the debug mailbox
/// IR is the instruction group.
/// Paused is 0 when PC post-increments, other when not.

void VMpor(void) {  // EXPORTED
#ifdef TRACEABLE
    memset(OpCounter,0,64*sizeof(uint32_t)); // clear opcode profile counters
    memset(ProfileCounts, 0, ROMsize*sizeof(uint32_t));  // clear profile counts
    cyclecount = 0;                     // cycles since POR
    RPmark = 0;
    maxRPtime = 0;
#endif // TRACEABLE
    PC = 0;  RP = 64;  SP = 32;  UP = 64;
    T=0;  N=0;  DebugReg = 0;
    memset(RAM,  0, RAMsize*sizeof(uint32_t));       // clear RAM
#ifdef EmbeddedROM
    FlashInit(0);
#endif // EmbeddedROM
}

uint32_t VMstep(uint32_t IR, int Paused) {  // EXPORTED
	uint32_t M;  int slot;
	uint64_t DX;
	unsigned int opcode;
// The PC is incremented at the same time the IR is loaded. Slot0 is next clock.
// The instruction group returned from memory will be latched in after the final
// slot executes. In the VM, that is simulated by a return from this function.
// Some slots may alter the PC. The last slot may alter the PC, so if it modifies
// the PC extra cycles are taken after the instruction group to resolve the
// instruction flow. If the PC has been steady long enough for the instruction
// to show up, it's latched into IR. Otherwise, there will be some delay while
// memory returns the instruction.

    if (!Paused) {
#ifdef TRACEABLE
        if (PC < ROMsize) {
            ProfileCounts[PC]++;
        }
        Trace(3, RidPC, PC, PC + 1);
#endif // TRACEABLE
        PC = PC + 1;
    }

	slot = 32;
	do { // valid slots: 26, 20, 14, 8, 2, -4
        slot -= 6;
        if (slot < 0) {
            opcode = IR & 3;                // slot = -4
        } else {
            opcode = (IR >> slot) & 0x3F;   // slot = 26, 20, 14, 8, 2
        }
#ifdef TRACEABLE
        uint32_t time;
        OpCounter[opcode]++;
        New = 1;  // first state change in an opcode
        if (!Paused) {
            cyclecount += 1;
        }
#endif // TRACEABLE
        switch (opcode) {
			case opNOP:									break;	// nop
			case opDUP: SDUP();							break;	// dup
			case opEXIT:
                M = RDROP()/4;
#ifdef TRACEABLE
                Trace(New, RidPC, PC, M);  New=0;
                if (!Paused) {
                    cyclecount += 3;    // PC change flushes pipeline
                }
#endif // TRACEABLE
                // PC is a cell address. The return stack works in bytes.
                PC = M;  goto ex;                   	        // exit
			case opADD:
			    DX = (uint64_t)N + (uint64_t)T;
#ifdef TRACEABLE
                Trace(New, RidT, T, (uint32_t)DX);  New=0;
                Trace(0, RidCY, CARRY, (uint32_t)(DX>>32));
#endif // TRACEABLE
                T = (uint32_t)DX;
                CARRY = (uint32_t)(DX>>32);
                SNIP();	                                break;	// +
			case opSKIP: goto ex;					    break;	// no:
			case opUSER: M = UserFunction (T, N, IMM);          // user
#ifdef TRACEABLE
                Trace(New, RidT, T, M);  New=0;
#endif // TRACEABLE
                T = M;  goto ex;
#ifndef HostFunction
// Host operations are available on platforms that support them.
// They are not traceable, so don't try.
            case opHost:
                SDUP();  SDUP();        // put TOS in RAM
                M = HostFunction(IMM, &RAM[SP & (RAMsize-1)]);
                SP += M;                // adjust stack depth
                SDROP();  SDROP();
                goto ex;
#endif // HostFunction
			case opZeroLess:
                M=0;  if ((signed)T<0) M--;
#ifdef TRACEABLE
                Trace(New, RidT, T, M);  New=0;
#endif // TRACEABLE
                T = M;                                  break;  // 0<
			case opPOP:  SDUP();  M = RDROP();
#ifdef TRACEABLE
                Trace(0, RidT, T, M);
#endif // TRACEABLE
			    T = M;      				            break;	// r>
			case opTwoDiv:
#ifdef TRACEABLE
                Trace(0, RidCY, CARRY, T&1);
                Trace(New, RidT, T, (signed)T >> 1);  New=0;
#endif // TRACEABLE
			    CARRY = T&1;  T = (signed)T >> 1;       break;	// 2/
			case opSKIPNC: if (!CARRY) goto ex;	        break;	// ifc:
			case opOnePlus:
#ifdef TRACEABLE
                Trace(New, RidT, T, T + 1);  New=0;
#endif // TRACEABLE
			    T = T + 1;                              break;	// 1+
			case opPUSH:  RDUP(T);  SDROP();            break;  // >r
			case opCstorePlus:    /* ( n a -- a' ) */
			    StoreByte(N, T);
#ifdef TRACEABLE
                Trace(0, RidT, T, T+1);
#endif // TRACEABLE
                T += 1;   SNIP();                       break;  // c!+
			case opCfetchPlus:  SDUP();  /* ( a -- a' c ) */
                M = FetchByte((signed)N);
#ifdef TRACEABLE
                Trace(0, RidT, T, M);
                Trace(0, RidN, N, N+1);
#endif // TRACEABLE
                T = M;
                N += 1;                                 break;  // c@+
			case opUtwoDiv:
#ifdef TRACEABLE
                Trace(0, RidCY, CARRY, T&1);
                Trace(New, RidT, T, (unsigned) T / 2);  New=0;
#endif // TRACEABLE
			    CARRY = T&1;  T = T / 2;                break;	// u2/
			case opOVER: M = N;  SDUP();
#ifdef TRACEABLE
                Trace(0, RidT, T, M);
#endif // TRACEABLE
                T = M;				                    break;	// over
			case opJUMP:
#ifdef TRACEABLE
                Trace(New, RidPC, PC, IMM);  New=0;
                if (!Paused) {
                    cyclecount += 3;
                }
				// PC change flushes pipeline in HW version
#endif // TRACEABLE
                // Jumps and calls use cell addressing
			    PC = IMM;  goto ex;                             // jmp
			case opWstorePlus:    /* ( n a -- a' ) */
			    StoreHalf(N, T);
#ifdef TRACEABLE
                Trace(0, RidT, T, T+2);
#endif // TRACEABLE
                T += 2;   SNIP();                       break;  // w!+
			case opWfetchPlus:  SDUP();  /* ( a -- a' c ) */
                M = FetchHalf((signed)N);
#ifdef TRACEABLE
                Trace(0, RidT, T, M);
                Trace(0, RidN, N, N+2);
#endif // TRACEABLE
                T = M;
                N += 2;                                 break;  // w@+
			case opAND:
#ifdef TRACEABLE
                Trace(New, RidT, T, T & N);  New=0;
#endif // TRACEABLE
                T = T & N;  SNIP();	                    break;	// and
            case opLitX:
				M = (T<<24) | (IMM & 0xFFFFFF);
#ifdef TRACEABLE
                Trace(New, RidT, T, M);  New=0;
#endif // TRACEABLE
                T = M;
                goto ex;                                        // litx
			case opSWAP: M = N;                                 // swap
#ifdef TRACEABLE
                Trace(New, RidN, N, T);  N = T;  New=0;
                Trace(0, RidT, T, M);    T = M;         break;
#else
                N = T;  T = M;  break;
#endif // TRACEABLE
			case opCALL:  RDUP(PC<<2);                        	// call
#ifdef TRACEABLE
                Trace(0, RidPC, PC, IMM);  PC = IMM;
                if (!Paused) {
                    cyclecount += 3;
                }
                goto ex;
#else
                PC = IMM;  goto ex;
#endif // TRACEABLE
            case opZeroEquals:
                M=0;  if (T==0) M--;
#ifdef TRACEABLE
                Trace(New, RidT, T, M);  New=0;
#endif // TRACEABLE
                T = M;                                  break;  // 0=
			case opWfetch:  /* ( a -- w ) */
                M = FetchHalf((signed)T);
#ifdef TRACEABLE
                Trace(0, RidT, T, M);
#endif // TRACEABLE
                T = M;                                  break;  // w@
			case opXOR:
#ifdef TRACEABLE
                Trace(New, RidT, T, T ^ N);  New=0;
#endif // TRACEABLE
                T = T ^ N;  SNIP();	                    break;	// xor
			case opREPTC:
			    if (!(CARRY & 1)) slot = 32;                    // reptc
#ifdef TRACEABLE
                Trace(New, RidN, N, N+1);  New=0; // repeat loop uses N
#endif // TRACEABLE                               // test and increment
                N++;  break;
			case opFourPlus:
#ifdef TRACEABLE
                Trace(New, RidT, T, T + 4);  New=0;
#endif // TRACEABLE
			    T = T + 4;                              break;	// 4+
            case opSKIPNZ:
				M = T;  SDROP();
                if (M == 0) break;
                goto ex;  										// ifz:
			case opADDC:  // carry into adder
			    DX = (uint64_t)N + (uint64_t)T + (uint64_t)(CARRY & 1);
#ifdef TRACEABLE
                Trace(New, RidT, T, (uint32_t)DX);  New=0;
                Trace(0, RidCY, CARRY, (uint32_t)(DX>>32));
#endif // TRACEABLE
                T = (uint32_t)DX;
                CARRY = (uint32_t)(DX>>32);
                SNIP();	                                break;	// c+
			case opStorePlus:    /* ( n a -- a' ) */
			    StoreCell(N, (signed)T);
#ifdef TRACEABLE
                Trace(0, RidT, T, T+4);
#endif // TRACEABLE
                T += 4;   SNIP();                       break;  // !+
			case opFetchPlus:  SDUP();  /* ( a -- a' c ) */
                M = FetchCell((signed)T);
#ifdef TRACEABLE
                Trace(0, RidT, T, M);
                Trace(0, RidN, N, N+4);
#endif // TRACEABLE
                T = M;
                N += 4;                                 break;  // @+
			case opTwoStar:
                M = T * 2;
#ifdef TRACEABLE
                Trace(0, RidT, T, M);
                Trace(0, RidCY, CARRY, T>>31);
#endif // TRACEABLE
                CARRY = T>>31;   T = M;                 break;  // 2*
			case opMiREPT:
                if (N&0x10000) slot = 32;          	            // -rept
#ifdef TRACEABLE
                Trace(New, RidN, N, N+1);  New=0; // repeat loop uses N
#endif // TRACEABLE                               // test and increment
                N++;  break;
			case opRP: M = RP;                                  // rp
                goto GetPointer;
			case opDROP: SDROP();		    	        break;	// drop
			case opSetRP:
#ifdef TRACEABLE
			    time = cyclecount - RPmark; // cycles since last RP!
			    RPmark = cyclecount;
                if (time > maxRPtime) {
                    maxRPtime = time ;
                }
#endif // TRACEABLE
                M = (T>>2) & (RAMsize-1);
#ifdef TRACEABLE
                Trace(New, RidRP, RP, M);  New=0;
#endif // TRACEABLE
			    RP = M;  SDROP();                       break;	// rp!
			case opFetch:  /* ( a -- n ) */
                M = FetchCell((signed)T);
#ifdef TRACEABLE
                Trace(0, RidT, T, M);
#endif // TRACEABLE
                T = M;                                  break;  // @
            case opTwoStarC:
                M = (T << 1) | (CARRY&1);
#ifdef TRACEABLE
                Trace(0, RidCY, CARRY, T>>31);
                Trace(0, RidT, T, M);
#endif // TRACEABLE
                CARRY = T>>31;   T = M;                 break;  // 2*c
			case opSKIPGE: if ((signed)T < 0) break;            // -if:
                goto ex;
			case opSP: M = SP;                                  // sp
GetPointer:     M = T + (M - RAMsize)*4; // common for rp, sp, up
#ifdef TRACEABLE
                Trace(0, RidT, T, M);
#endif // TRACEABLE
			    T = M;                                  break;
			case opSetSP:
                M = (T>>2) & (RAMsize-1);
#ifdef TRACEABLE
                Trace(New, RidSP, SP, M);  New=0;
#endif // TRACEABLE
                // SP! does not post-drop
			    SP = M;         	                    break;	// sp!
			case opCfetch:  /* ( a -- w ) */
                M = FetchByte((signed)T);
#ifdef TRACEABLE
                Trace(0, RidT, T, M);
#endif // TRACEABLE
                T = M;                                  break;  // c@
			case opPORT: M = T;
#ifdef TRACEABLE
                Trace(0, RidT, T, DebugReg);
                Trace(0, RidDbg, DebugReg, M);
#endif // TRACEABLE
                T=DebugReg;
                DebugReg=M;
                break;	                                        // port
			case opLIT: SDUP();
#ifdef TRACEABLE
                Trace(0, RidT, T, IMM);
#endif // TRACEABLE
                T = IMM;  goto ex;                              // lit
			case opUP: M = UP;  	                            // up
                goto GetPointer;
			case opSetUP:
                M = (T>>2) & (RAMsize-1);
#ifdef TRACEABLE
                Trace(New, RidUP, UP, M);  New=0;
#endif // TRACEABLE
			    UP = M;  SDROP();	                    break;	// up!
			case opRfetch: SDUP();
                M = RAM[RP & (RAMsize-1)];
#ifdef TRACEABLE
                Trace(New, RidT, T, M);  New=0;
#endif // TRACEABLE
                T = M;					                break;	// r@
			case opCOM:
#ifdef TRACEABLE
                Trace(New, RidT, T, ~T);  New=0;
#endif // TRACEABLE
			    T = ~T;                                 break;	// com
			default:                           		    break;	//
		}
	} while (slot>=0);
ex:
#ifdef EmbeddedROM
    if (PC >= (SPIflashBlocks<<10)) {
        exception = -9;                 // Invalid memory address
    }
#else                                   // ignore PC=DEADC0DC
    if ((PC >= (SPIflashBlocks<<10)) && (PC != 0x37AB7037)) {
        exception = -9;                 // Invalid memory address
    }
#endif // EmbeddedROM

    if (exception) {
        tiffIOR = exception;            // tell Tiff there was an error
        RDUP(PC<<2);
        PC = 2;                         // call an error interrupt
        DebugReg = exception;
        exception = 0;
    }
    return PC;
}

// write to the debug mailbox
void SetDbgReg(uint32_t n) {  // EXPORTED
    DebugReg = n;
}

// read from the debug mailbox
uint32_t GetDbgReg(void) {  // EXPORTED
    return DebugReg;
}

// Instrumentation

uint32_t vmRegRead(int ID) {
	switch(ID) {
		case 0: return T;
		case 1: return N;
		case 2: return (RP-RAMsize)*4;
		case 3: return (SP-RAMsize)*4;
		case 4: return (UP-RAMsize)*4;
		case 5: return PC*4;
		default: return 0;
	}
}

