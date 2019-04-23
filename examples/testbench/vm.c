#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "vm.h"
#include "vmuser.h"
#include "flash.h"
#include <string.h>

// This file serves as the official specification for the Mforth VM.
// It is usable as a template for generating a C testbench or embedding in an app.

#define IMM    (IR & ~(-1<<slot))
//
#define ROMsize 8192
//
#define RAMsize 512
//
#define SPIflashBlocks 256
//
#define EmbeddedROM

/* -----------------------------------------------------------------------------
    Globals:
        tiffIOR
        If TRACEABLE: VMreg[], OpCounter[], ProfileCounts[], cyclecount, maxRPtime, maxReturnPC
    Exports:
        VMpor, VMstep, ROMinit, SetDbgReg, GetDbgReg, vmRegRead,
        FetchCell, FetchHalf, FetchByte, StoreCell, StoreHalf, StoreByte,
        In not embedded: WriteROM

    Addresses are VM byte addresses
*/

int tiffIOR; // global errorcode

static int exception = 0;  // local errorcode

//
static const uint32_t InternalROM[384] = {
/*0000*/ 0x4C000172, 0x00002710, 0x4C00015D, 0xFC90C240, 0x96B09000, 0x36B09000,
/*0006*/ 0x56B09000, 0x05AB8318, 0x96B09000, 0x05AD8318, 0x36B09000, 0xFC909000,
/*000C*/ 0xFC9FC240, 0xFE1FC240, 0x9E709000, 0x68A18A08, 0x29A28608, 0x2A209000,
/*0012*/ 0x2AB09000, 0x28629A28, 0x69A09000, 0x18628628, 0x68A09000, 0x186F8A68,
/*0018*/ 0x2BE29A08, 0xAEB09000, 0x8A209000, 0x68A18A68, 0x68A18A1A, 0x69A8A218,
/*001E*/ 0x1930001B, 0xAEBAC240, 0x05209000, 0x04240000, 0x07005FFE, 0x05F09000,
/*0024*/ 0x74240000, 0x7DD09000, 0x7DD74240, 0x75D09000, 0xFC914240, 0x449E4003,
/*002A*/ 0xFD709000, 0x98AB8A08, 0x965AC240, 0xC3F25000, 0x09000000, 0xFCAFD7FE,
/*0030*/ 0x68240000, 0x69A2860C, 0x2868C240, 0xFDAFC918, 0x89DC2B26, 0xAC240000,
/*0036*/ 0xC1300033, 0x09000000, 0x38240000, 0xE4000004, 0xCAE09000, 0xE4000008,
/*003C*/ 0xCAE09000, 0xE400000C, 0xEAEE4008, 0xCBF2431C, 0x1C240000, 0x0679F2B8,
/*0042*/ 0x2AB09000, 0xE400000A, 0xE4008218, 0x96B09000, 0xE4000010, 0xE4008218,
/*0048*/ 0x96B09000, 0xB9000000, 0xE4FFFFFF, 0x5C240000, 0x8A2FC340, 0xC2BAEB08,
/*004E*/ 0xACAFC918, 0x84A68A68, 0x68240000, 0x18624688, 0x88340000, 0xC2B69A6A,
/*0054*/ 0xAEBAE16A, 0x18A1A20C, 0xFA20CA68, 0x2857D000, 0xC2B68240, 0xAC6AC6AC,
/*005A*/ 0x85A09000, 0xE400000C, 0xAAE09000, 0x186AC6AC, 0x68240000, 0x8A27DD40,
/*0060*/ 0x4AB09000, 0xAEB1A16A, 0x186FC9FC, 0xC2B68240, 0x6A168240, 0x181383E7,
/*0066*/ 0x05A0C6FC, 0x5DA09000, 0x8A27C540, 0x4BF24316, 0x2AB14240, 0x29300068,
/*006C*/ 0x8A27C540, 0x4BF24316, 0xAC509000, 0x2930006C, 0xFC9FD000, 0xC1300075,
/*0072*/ 0xFC929A28, 0x98695AA0, 0x1ABAEB08, 0xAEBAC240, 0xFC9FD000, 0xC130007B,
/*0078*/ 0xFC929A28, 0x38635AA0, 0x1ABAEB08, 0xAEBAC240, 0xFC9FD000, 0xC1300085,
/*007E*/ 0x24168328, 0xF83286FC, 0x25000000, 0x6BF27F28, 0xFC9FCA88, 0xDA236B18,
/*0084*/ 0x2704C081, 0xAEBAC240, 0x6A289B68, 0x4930008A, 0x19B0007C, 0x4C00008B,
/*008A*/ 0x19B00076, 0x09000000, 0x8924C091, 0x2BF24928, 0x68A40000, 0xF8A36818,
/*0090*/ 0xAEBAC240, 0xAEBAC240, 0xE4000000, 0x4C00008C, 0xFC9FF0AE, 0xE400003F,
/*0096*/ 0x5FF24A40, 0x9E82AB08, 0x09000000, 0xFC9FF0AE, 0xE400003F, 0x5FF24A40,
/*009C*/ 0x3E82AB08, 0x09000000, 0x0417F91F, 0xFD000000, 0x6A76AF40, 0x22218368,
/*00A2*/ 0x20940000, 0x18625000, 0xC13000A0, 0xADA6AB18, 0x18A09000, 0x8A2FC90C,
/*00A8*/ 0xAC84C0B7, 0xE400001F, 0xFCF9D000, 0x69A6AF18, 0xBC84C0B2, 0x07EFC90C,
/*00AE*/ 0xAD000000, 0x23EFC90C, 0x06FFC7AC, 0x4C0000B4, 0xFBF243E4, 0x9EB40000,
/*00B4*/ 0x18625000, 0xC13000AB, 0xAEB2AFFE, 0xAEBAF900, 0xFC109000, 0x8A27DA88,
/*00BA*/ 0x69B0002D, 0x69B00036, 0x19B000A7, 0x28615000, 0x493000C0, 0xFC940000,
/*00C0*/ 0x28615000, 0x493000C3, 0xFC940000, 0x09000000, 0x05A8A27C, 0x68169B2D,
/*00C6*/ 0x69B00036, 0x19B000A7, 0x28615000, 0x493000CB, 0xFC940000, 0x28615000,
/*00CC*/ 0x493000D2, 0xFC989000, 0x493000D2, 0xF9A28628, 0xFC90CAFC, 0x27F40000,
/*00D2*/ 0x1AB09000, 0x04505A40, 0x493000D7, 0xFC969B33, 0x19000000, 0x68115000,
/*00D8*/ 0x493000DA, 0xF8340000, 0x19B000A7, 0x1924C0DD, 0x2BF24A40, 0x09000000,
/*00DE*/ 0x885293D3, 0x6C0000DE, 0xAC240000, 0x6C0000DE, 0x2AB09000, 0x8A26C06C,
/*00E4*/ 0x48A40000, 0xAC240000, 0x8A229B6C, 0x48A40000, 0xAC240000, 0x8A26C068,
/*00EA*/ 0x48A40000, 0xAC240000, 0x8A229B68, 0x48A40000, 0xAC240000, 0x68AF8328,
/*00F0*/ 0x1BF24308, 0x8BF24368, 0xFC90C640, 0x4C000068, 0x6C00009E, 0xAC240000,
/*00F6*/ 0x8A27C568, 0x6C00002D, 0x29B0002D, 0x6C00009E, 0x1924C0FC, 0x6C000033,
/*00FC*/ 0x09000000, 0x69B000F6, 0x193000D3, 0x6C0000FD, 0x2AB09000, 0x10000006,
/*0102*/ 0x09000000, 0x2B900000, 0xFDA40000, 0x8924C10F, 0x2BF27F28, 0x3867F907,
/*0108*/ 0xFD000000, 0x25A07901, 0x5FF279ED, 0x64B88320, 0x5CA3DF18, 0xC1300109,
/*010E*/ 0xADA4C105, 0xAEB1BF08, 0x89AE4004, 0xC9A40000, 0xE4008214, 0xB9AE4000,
/*0114*/ 0xAB908214, 0x96B6C030, 0x1B908214, 0x96B1AB19, 0x7C240000, 0x052AC240,
/*011A*/ 0xE4008214, 0xBAD19000, 0xE4008214, 0x96B18A68, 0xD6B2AB18, 0x18A09000,
/*0120*/ 0x09000000, 0x01000000, 0xE40004D2, 0x05000000, 0xAD000000, 0x05000000,
/*0126*/ 0x85000000, 0x0D000000, 0x15000000, 0x45000000, 0xFD000000, 0x05A40000,
/*012C*/ 0x1D000000, 0x25000000, 0xF9000000, 0xFC90D000, 0x19000000, 0x7D000000,
/*0132*/ 0x75000000, 0x74540000, 0xE4BC614E, 0xE4008268, 0x05A40000, 0x95000000,
/*0138*/ 0xAD000000, 0xF8E40000, 0x29000000, 0xD9000000, 0xAEB40000, 0xF9640000,
/*013E*/ 0x29000000, 0x79000000, 0xAEB40000, 0xFA640000, 0x29000000, 0xB9000000,
/*0144*/ 0x5D000000, 0xE40003E8, 0xF95AD000, 0xF9E40000, 0xAF900063, 0xF8DAD000,
/*014A*/ 0x1B640000, 0x8D000000, 0x89000000, 0x3D000000, 0x1D000000, 0x8D000000,
/*0150*/ 0x29000000, 0x1CF40000, 0xBD000000, 0x9D000000, 0xFC340000, 0x64000055,
/*0156*/ 0xAC240000, 0x10000002, 0xAC240000, 0xE40082F5, 0xB9000000, 0xAD000000,
/*015C*/ 0x09000000, 0x09000000, 0xE4000000, 0xE40082F4, 0xE4000004, 0x6C000070,
/*0162*/ 0xE4008304, 0x07900010, 0x0F900010, 0x6C000076, 0xE4000000, 0xFF900000,
/*0168*/ 0xFDB0009E, 0xAF900000, 0xFCF07902, 0xFDB000A7, 0xAEB09000, 0xE40F4240,
/*016E*/ 0xE4000163, 0xE4000071, 0x6C0000FF, 0x4C000119, 0x6C000120, 0x6C000121,
/*0174*/ 0xE400005B, 0x6C000157, 0x6C000159, 0xE400002D, 0x6C000157, 0x6C00015E,
/*017A*/ 0xE40005B4, 0x6C000110, 0xAF90002D, 0x6C000157, 0x4C00017E, 0x09000000};
//
static uint32_t FetchROM(uint32_t addr) {
//
  if (addr < 384) {
//
    return InternalROM[addr];
//
  }
//
  return -1;
//
}

/// Virtual Machine for 32-bit MachineForth.

/// The optional Trace function tracks state changes using these parameters:
/// Type of state change: 0 = unmarked, 1 = new opcode, 2 or 3 = new group;
/// Register ID: Complement of register number if register, memory if other;
/// Old value: 32-bit.
/// New value: 32-bit.

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
    uint32_t ProfileCounts[ROMsize];
    uint32_t cyclecount;
    uint32_t maxRPtime;   // max cycles between RETs
    uint32_t maxReturnPC = 0;   // PC where it occurred
    static uint32_t RPmark;

    static int New; // New trace type, used to mark new sections of trace
    static uint32_t RAM[RAMsize];
    static uint32_t ROM[ROMsize];
    static uint32_t AXI[AXIRAMsize];

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

    static uint32_t RAM[RAMsize];
#ifndef EmbeddedROM
    static uint32_t ROM[ROMsize];
#endif // EmbeddedROM
    static uint32_t AXI[AXIRAMsize];

    static void SDUP(void)  { RAM[--SP & (RAMsize-1)] = N;  N = T; }
    static void SDROP(void) { T = N;  N = RAM[SP++ & (RAMsize-1)]; }
    static void SNIP(void)  { N = RAM[SP++ & (RAMsize-1)]; }
    static void RDUP(uint32_t x)  { RAM[--RP & (RAMsize-1)] = x; }
    static uint32_t RDROP(void) { return RAM[RP++ & (RAMsize-1)]; }

#endif // TRACEABLE

// Generic fetch from ROM or RAM: ROM is at the bottom, RAM is in middle, ROM is at top
static uint32_t FetchX (uint32_t addr, int shift, int mask) {
    if (addr < ROMsize) {
#ifdef EmbeddedROM
        return (FetchROM(addr) >> shift) & mask;
#else
//      printf("@c[%X]=%X ", addr, ROM[addr]);
        return (ROM[addr] >> shift) & mask;
#endif // EmbeddedROM
    }
    if (addr < (ROMsize + RAMsize)) {
//      printf("@[%X]=%X ", addr, ROM[addr]);
        return (RAM[addr-ROMsize] >> shift) & mask;
    }
    return (FlashRead(addr*4) >> shift) & mask;
}

// Generic store to RAM only.
static void StoreX (uint32_t addr, uint32_t data, int shift, int mask) {
    if ((addr<ROMsize) || (addr>=(ROMsize+RAMsize))) {
        exception = -9;  return;
    } // disallow ROM and SPI flash writing
    int ra = addr - ROMsize;  // cell index
    uint32_t temp = RAM[ra] & (~(mask << shift));
#ifdef TRACEABLE
    temp = ((data & mask) << shift) | temp;
    Trace(New, ra, RAM[ra], temp);  New=0;
    RAM[ra] = temp;
#else
    RAM[ra] = ((data & mask) << shift) | temp;
#endif // TRACEABLE
//  printf("![%X]=%X ", ra, RAM[ra]);
}

/// EXPORTS ////////////////////////////////////////////////////////////////////

// Unprotected write: Doesn't care what's already there.
// This is a sharp knife, make sure target app doesn't try to use it.
#ifdef EmbeddedROM
int WriteROM(uint32_t data, uint32_t address) {
    return -20;                         // writing to read-only memory
}
#else
int WriteROM(uint32_t data, uint32_t address) {
    uint32_t addr = address / 4;
    if (address & 3) return -23;        // alignment problem
    if (addr >= (SPIflashBlocks<<10)) return -9;
    if (addr < ROMsize) {
        ROM[addr] = data;
        return 0;
    }
    tiffIOR = FlashWrite(data, address);
    printf("FlashWrite to %X", address);  // writing above ROM space
 //           tiffIOR = -20;
    return tiffIOR;
}
#endif // EmbeddedROM


uint32_t FetchCell(uint32_t addr) {
    if (addr & 3) {
        exception = -23;
    }
    return FetchX(addr>>2, 0, 0xFFFFFFFF);
}
uint16_t FetchHalf(uint32_t addr) {
    if (addr & 1) {
        exception = -23;
    }
    return FetchX(addr>>2, (addr&2)*8, 0xFFFF);
}
uint8_t FetchByte(uint32_t addr) {
    return FetchX(addr>>2, (addr&3)*8, 0xFF);
}
void StoreCell (uint32_t x, uint32_t addr) {
    if (addr & 3) {
        exception = -23;
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

void StoreHalf (uint16_t x, uint32_t addr) {
    if (addr & 1) {
        exception = -23;
    }
    StoreX(addr>>2, x, (addr&2)*8, 0xFFFF);
}
void StoreByte (uint8_t x, uint32_t addr) {
    StoreX(addr>>2, x, (addr&3)*8, 0xFF);
}

void ROMinit(void){
#ifndef EmbeddedROM
    for (int i=0; i<ROMsize; i++) {
        WriteROM(-1, i*4);
    }
#endif // EmbeddedROM
    FlashInit();
};


// Send a stream of RAM words to the AXI bus.
// The only thing on the AXI bus here is RAM that can only be accessed by !AS and @AS.
// An external function could be added in the future for other stuff.
static void SendAXI(uint32_t src, uint32_t dest, uint8_t length) {
    for (int i=0; i<=length; i++) {
        uint32_t data = FetchCell(src);
        if (dest >= AXIRAMsize) {
            exception = -9;
        } else {
            AXI[dest++] = data;
        }
        src += 4;
    } return;
}

// Receive a stream of RAM words from the AXI bus.
// An external function could be added in the future for other stuff.
static void ReceiveAXI(uint32_t src, uint32_t dest, uint8_t length) {
    for (int i=0; i<=length; i++) {
        uint32_t data = 0;
        if (src >= AXIRAMsize) {
            exception = -9;
        } else {
            data = AXI[src++];
        }
        StoreCell(data, dest);
        dest += 4;
    } return;
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
    FlashInit();
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
                M = RDROP() >> 2;
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
/*			case opSUB:
			    DX = (uint64_t)N - (uint64_t)T;
#ifdef TRACEABLE
                Trace(New, RidT, T, (uint32_t)DX);  New=0;
                Trace(0, RidCY, CARRY, (uint32_t)(DX>>32));
#endif // TRACEABLE
                T = (uint32_t)DX;
                CARRY = (uint32_t)(DX>>32);
                SNIP();	                                break;	// -
*/
			case opCstorePlus:    /* ( n a -- a' ) */
			    StoreByte(N, T);
#ifdef TRACEABLE
                Trace(0, RidT, T, T+1);
#endif // TRACEABLE
                T += 1;   SNIP();                       break;  // c!+
			case opCfetchPlus:  SDUP();  /* ( a -- a' c ) */
                M = FetchX(N>>2, (N&3) * 8, 0xFF);
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
			case opTwoPlus:
#ifdef TRACEABLE
                Trace(New, RidT, T, T + 2);  New=0;
#endif // TRACEABLE
			    T = T + 2;                              break;	// 2+
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
                M = FetchHalf(N);

 //               M = FetchX(N>>2, (N&2) * 8, 0xFFFF);
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
                M = FetchHalf(T);
#ifdef TRACEABLE
                Trace(0, RidT, T, M);
#endif // TRACEABLE
                T = M;                                  break;  // w@
			case opXOR:
#ifdef TRACEABLE
                Trace(New, RidT, T, T ^ N);  New=0;
#endif // TRACEABLE
                T = T ^ N;  SNIP();	                    break;	// xor
			case opREPT:  slot = 32;                    break;	// rept
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
			    StoreCell(N, T);
#ifdef TRACEABLE
                Trace(0, RidT, T, T+4);
#endif // TRACEABLE
                T += 4;   SNIP();                       break;  // !+
			case opFetchPlus:  SDUP();  /* ( a -- a' c ) */
                M = FetchCell(T);
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
                if (N&0x8000) slot = 32;          	            // -rept
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
                M = FetchCell(T);
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
GetPointer:     M = T + (M + ROMsize)*4;
#ifdef TRACEABLE
                Trace(0, RidT, T, M);
#endif // TRACEABLE
			    T = M;                                  break;
			case opFetchAS:
                ReceiveAXI(N, T, IMM);  goto ex;	            // @as
			case opSetSP:
                M = (T>>2) & (RAMsize-1);
#ifdef TRACEABLE
                Trace(New, RidSP, SP, M);  New=0;
#endif // TRACEABLE
                // SP! does not post-drop
			    SP = M;         	                    break;	// sp!
			case opCfetch:  /* ( a -- w ) */
                M = FetchX(T>>2, (T&3) * 8, 0xFF);
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
			case opStoreAS:
                SendAXI(N, T, IMM);  goto ex;                   // !as
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
		case 2: return (RP+ROMsize)*4;
		case 3: return (SP+ROMsize)*4;
		case 4: return (UP+ROMsize)*4;
		case 5: return PC*4;
		default: return 0;
	}
}

