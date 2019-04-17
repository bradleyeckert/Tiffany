#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "vm.h"
#include <string.h>

#define IMM    (IR & ~(-1<<slot))
//
#define EmbeddedROM
//
#define ROMsize 8192
//
#define RAMsize 512

int tiffIOR; // global errorcode

static int error = 0;  // local errorcode

// This file is usable as a template for generating a C testbench

//
static const uint32_t ROM[501] = {
/*0000*/ 0x4C0001EF, 0x4C0001F2, 0x4FFFFFFF, 0x96B09000, 0x36B09000, 0x56B09000,
/*0006*/ 0x05AB8318, 0x96B09000, 0x05AD8318, 0x36B09000, 0xFC909000, 0xFC9FC240,
/*000C*/ 0xFE1FC240, 0x9E709000, 0x68A18A08, 0x29A28608, 0x2A209000, 0x2AB09000,
/*0012*/ 0x28629A28, 0x69A09000, 0x18628628, 0x68A09000, 0x186F8A68, 0x2BE29A08,
/*0018*/ 0xAEB09000, 0x8A209000, 0x68A18A68, 0x68A18A1A, 0x69A8A218, 0x1930001A,
/*001E*/ 0xAEBAC240, 0x05209000, 0x04240000, 0x07805F08, 0x05FFC240, 0x7DD09000,
/*0024*/ 0x7DD74240, 0x75D09000, 0xFC914240, 0x449E4003, 0xFD709000, 0x98AB8A08,
/*002A*/ 0x965AC240, 0xC3F25000, 0x09000000, 0xFCAFD7FE, 0x68240000, 0x69A2860C,
/*0030*/ 0x2868C240, 0xFDAFC918, 0x89DC2B26, 0xAC240000, 0xC1300031, 0x09000000,
/*0036*/ 0x38240000, 0xE4000004, 0xCAE09000, 0xE4000008, 0xCAE09000, 0xE400000C,
/*003C*/ 0xEAEE4008, 0xC8B1C708, 0x0679F2B8, 0x2AB09000, 0xE400000A, 0xE4008218,
/*0042*/ 0x96B09000, 0xE4000010, 0xE4008218, 0x96B09000, 0xB9000000, 0xE4FFFFFF,
/*0048*/ 0x5C240000, 0x8A2FC340, 0xC2BAEB08, 0xACAFC918, 0x84A68A68, 0x68240000,
/*004E*/ 0x18624688, 0x88340000, 0xC2B69A6A, 0xAEBAE16A, 0x18A1A20C, 0xFA20CA68,
/*0054*/ 0x2857D000, 0xC2B68240, 0xAC6AC6AC, 0x85A09000, 0xE400000C, 0xAAE09000,
/*005A*/ 0x186AC6AC, 0x68240000, 0x8A27DD40, 0x4AB09000, 0xAEB1A16A, 0x186FC9FC,
/*0060*/ 0xC2B68240, 0x6A168240, 0x181383E7, 0x05A0C6FC, 0x5DA09000, 0x8A27C540,
/*0066*/ 0x48B14240, 0x2AB14240, 0x29300065, 0x8A27C540, 0x48B14240, 0xAC509000,
/*006C*/ 0x29300069, 0xFC940000, 0xE2BAEB08, 0x24A68A40, 0x98695AA0, 0x1ABAEB08,
/*0072*/ 0xFC940000, 0xE2BAEB08, 0x24A68A40, 0x38635AA0, 0x1ABAEB08, 0xC2BAEB08,
/*0078*/ 0x05A0CAFB, 0x286FC940, 0x6BF27F28, 0xFC9FCA88, 0xDA236B18, 0x2704C07A,
/*007E*/ 0xAEB09000, 0x6A289B65, 0x49300083, 0x19B00077, 0x4C000084, 0x19B00072,
/*0084*/ 0x09000000, 0x2BF25000, 0xE2BAEB08, 0x24A68A40, 0xF8A36818, 0xAEBAC240,
/*008A*/ 0xE400003F, 0x5FF278AE, 0x24A40000, 0x9E82AB08, 0x09000000, 0xE400003F,
/*0090*/ 0x5FF278AE, 0x24A40000, 0x3E82AB08, 0x09000000, 0x0417F91F, 0xFD000000,
/*0096*/ 0x6A76AF40, 0x22218368, 0x20940000, 0x18625000, 0xC1300096, 0xADA6AB18,
/*009C*/ 0x18A09000, 0x8A22EB40, 0x213000A1, 0xAEBAF900, 0xFC109000, 0xE400001F,
/*00A2*/ 0xFCF9D000, 0x69A6AF18, 0xBC84C0A9, 0x07E2EB40, 0x213000A8, 0xF8B40000,
/*00A8*/ 0x4C0000AB, 0xF8BE4000, 0x9EB40000, 0x18625000, 0xC13000A3, 0xAEB2AFFE,
/*00AE*/ 0x8A27DA88, 0x69B0002B, 0x69B00034, 0x19B0009D, 0x28615000, 0x493000B5,
/*00B4*/ 0xFC940000, 0x28615000, 0x493000B8, 0xFC940000, 0x09000000, 0x05A8A27C,
/*00BA*/ 0x68169B2B, 0x69B00034, 0x19B0009D, 0x28615000, 0x493000C0, 0xFC940000,
/*00C0*/ 0x28615000, 0x493000C6, 0xFC989000, 0x493000C6, 0xF9A28628, 0x2CAFC9FC,
/*00C6*/ 0x1AB09000, 0x04505A40, 0x493000CB, 0xFC969B31, 0x19000000, 0x68115000,
/*00CC*/ 0x493000CE, 0xF8340000, 0x19B0009D, 0x1924C0D1, 0x2BF24A40, 0x09000000,
/*00D2*/ 0x885293C7, 0x6C0000D2, 0xAC240000, 0x6C0000D2, 0x2AB09000, 0x8A26C069,
/*00D8*/ 0x48A40000, 0xAC240000, 0x8A229B69, 0x48A40000, 0xAC240000, 0x8A26C065,
/*00DE*/ 0x48A40000, 0xAC240000, 0x8A229B65, 0x48A40000, 0xAC240000, 0x68AF8328,
/*00E4*/ 0x18B09000, 0x88B68B18, 0x4C000065, 0x6C000094, 0xAC240000, 0x8A27C568,
/*00EA*/ 0x6C00002B, 0x29B0002B, 0x6C000094, 0x1924C0EF, 0x6C000031, 0x09000000,
/*00F0*/ 0x69B000E9, 0x193000C7, 0x6C0000F0, 0x2AB09000, 0x10000006, 0x09000000,
/*00F6*/ 0x89AE4004, 0xC9A40000, 0xE4008214, 0xB9AE4000, 0xAB908214, 0x96B6C02E,
/*00FC*/ 0x1B908214, 0x96B1AB19, 0x7C240000, 0x052AC240, 0xE4008214, 0xBAD19000,
/*0102*/ 0xE4008214, 0x96B18A68, 0xD6B2AB18, 0x18A09000, 0x09000000, 0x09000000,
/*0108*/ 0x04400004, 0x09000000, 0xE400000A, 0x6C0000E7, 0x6C000108, 0x0D000000,
/*010E*/ 0x6C000106, 0x05B00108, 0x6C000069, 0x4930010E, 0xAC240000, 0x6C000106,
/*0114*/ 0x04400003, 0x49300113, 0x10000002, 0xAC240000, 0x4C000156, 0x39300118,
/*011A*/ 0x040A0D02, 0x4A325B1B, 0xE4000468, 0x4C000119, 0xE400046B, 0x4C000119,
/*0120*/ 0x04400000, 0x09000000, 0x04400001, 0x09000000, 0x0000044C, 0x00000470,
/*0126*/ 0x00000478, 0x00000480, 0x00000488, 0xE4000490, 0xE4008230, 0x96B09000,
/*012C*/ 0x9E740000, 0xE4008230, 0xB83B9A08, 0xE4000000, 0x4C00012C, 0xE4000001,
/*0132*/ 0x4C00012C, 0xE4000002, 0x4C00012C, 0xE4000003, 0x4C00012C, 0xE4000004,
/*0138*/ 0x4C00012C, 0x89AFC9FC, 0x289286DA, 0xE4000006, 0x6C00008A, 0x69B00139,
/*013E*/ 0xE400003F, 0x5C60C240, 0x6C000139, 0x079000C0, 0x5F900080, 0x7DD40000,
/*0144*/ 0x49300146, 0xAD300140, 0x07900080, 0x5D24C155, 0x07900020, 0x5D24C153,
/*014A*/ 0x07900010, 0x5D24C150, 0xE4000007, 0x5DB0013B, 0x6C00013B, 0x4C00013B,
/*0150*/ 0xE400000F, 0x5DB0013B, 0x4C00013B, 0xE400001F, 0x5D30013B, 0x09000000,
/*0156*/ 0x07F27F14, 0xFD24C15B, 0x6C000140, 0x6C00012F, 0x4C000156, 0xAEB09000,
/*015C*/ 0xE4000020, 0x4C00012F, 0xFC940000, 0xE2B09000, 0x6C00015C, 0x2530015F,
/*0162*/ 0x09000000, 0xE4000009, 0x89B00069, 0xE4000007, 0x5C3E4030, 0x0C240000,
/*0168*/ 0xE4008354, 0xE4008288, 0x96B09000, 0xE4008288, 0xBBF27F04, 0xE4008288,
/*016E*/ 0x96B36B08, 0xE4000000, 0xE4008218, 0xB9B0009D, 0x6B908218, 0xB9B0009D,
/*0174*/ 0x29B00163, 0x6C00016B, 0x18240000, 0x6C00016F, 0x8A26C02D, 0x7524C177,
/*017A*/ 0x09000000, 0x1524C17E, 0xE400002D, 0x6C00016B, 0x09000000, 0xAEB40000,
/*0180*/ 0xE4008288, 0xBB908354, 0x88B09000, 0x88B6C15E, 0x4C000118, 0x68169B34,
/*0186*/ 0x6C000168, 0x6C000177, 0x19B0017B, 0x6C00017F, 0x19300183, 0xE4000000,
/*018C*/ 0x29300185, 0x69B00021, 0x19300185, 0xE4000000, 0x6C000185, 0x4C00015C,
/*0192*/ 0xE4000000, 0x4C00018F, 0xE4008218, 0xBB90000A, 0x7D24C198, 0x4C000192,
/*0198*/ 0x6C000021, 0x4C00018F, 0xB9300194, 0x6C000168, 0xFC940000, 0x69B0016F,
/*019E*/ 0x18940000, 0xC130019D, 0xADB00177, 0x4C00017F, 0xE4008218, 0xB9A6C043,
/*01A4*/ 0xE4000000, 0x29B0019B, 0x1B908218, 0x96B6C118, 0x4C00015C, 0x4C0001AD,
/*01AA*/ 0x6C65480B, 0x57206F6C, 0x646C726F, 0xE40006A8, 0x39B00118, 0x6C000131,
/*01B0*/ 0xE400000A, 0xE4000000, 0x2BF25A68, 0xF9B00194, 0x6C00004E, 0x4C0001B3,
/*01B6*/ 0x09000000, 0x00000004, 0x00008204, 0x00008200, 0x000081F0, 0x00008100,
/*01BC*/ 0x00000000, 0x00000005, 0x00008218, 0x0000000A, 0x00009B3C, 0x000006DC,
/*01C2*/ 0x00008354, 0x00000000, 0x00000004, 0x0000822C, 0x00008284, 0x00000490,
/*01C8*/ 0x00000001, 0x00000000, 0x00000002, 0x0000823C, 0x0000828C, 0x00000000,
/*01CE*/ 0x00000002, 0x00008248, 0x001A0001, 0x00000000, 0x00000002, 0x00008250,
/*01D4*/ 0x1A0006D4, 0x00000000, 0x00000004, 0x0000825C, 0x00008A58, 0x00040003,
/*01DA*/ 0x00008284, 0x00000000, 0x00000002, 0x00008284, 0x00009B1C, 0x00000000,
/*01E0*/ 0x00000000, 0xE40006DC, 0x9A28899C, 0x9C329A28, 0x18140000, 0x493001E8,
/*01E6*/ 0x6A619B6D, 0x4C0001E2, 0x1B908288, 0x96B40000, 0xE4008200, 0xF7908100,
/*01EC*/ 0xD79081EC, 0xB7908288, 0xB9A09000, 0x6C0001E1, 0x6C0001A9, 0x4C0000F4,
/*01F2*/ 0x6C0001E1, 0x6C0001A9, 0x4C0000F4};
//
uint32_t FetchROM(uint32_t addr) {
//
  if (addr < 501) {
//
    return ROM[addr];
//
  }
//
  return -1;
//
}

/// Virtual Machine for 32-bit MachineForth.

/// The VM registers are defined here but generally not accessible outside this
/// module except through VMstep. The VM could be at the end of a cable, so we
/// don't want direct access to its innards.

/// These functions are always exported: VMpor, VMstep, SetDbgReg, GetDbgReg.
/// If TRACEABLE, you get more exported: UnTrace, VMreg[],
/// while importing the Trace function. This offers a way to break the
/// "no direct access" rule in a PC environment, for testing and VM debugging.

/// Flash writing is handled by streaming data to AXI space, through VMstep and
/// friends. ROM writing uses a WriteROM function exported to Tiff but not used
/// in a target system.

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
    static uint32_t RPmark;
    uint32_t maxRPtime;   // max cycles between RETs
    uint32_t maxReturnPC = 0;   // PC where it occurred

    static int New; // New trace type, used to mark new sections of trace
    static uint32_t RAM[RAMsize];
//
#ifdef __NEVER_INCLUDE__
    static uint32_t ROM[ROMsize];
//
#endif
    uint32_t AXI[SPIflashSize+AXIRAMsize];

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
#endif
    uint32_t AXI[SPIflashSize+AXIRAMsize];

    static void SDUP(void)  { RAM[--SP & (RAMsize-1)] = N;  N = T; }
    static void SDROP(void) { T = N;  N = RAM[SP++ & (RAMsize-1)]; }
    static void SNIP(void)  { N = RAM[SP++ & (RAMsize-1)]; }
    static void RDUP(uint32_t x)  { RAM[--RP & (RAMsize-1)] = x; }
    static uint32_t RDROP(void) { return RAM[RP++ & (RAMsize-1)]; }

#endif // TRACEABLE

// Generic fetch from ROM or RAM: ROM is at the bottom, RAM is in middle, AXI is at top
static uint32_t FetchX (uint32_t addr, int shift, int mask) {
    if (addr >= (SPIflashSize+AXIRAMsize)) {
        return 0;
    }
    if (addr < ROMsize) {
        return (ROM[addr] >> shift) & mask;
    } else {
        if (addr < (ROMsize + RAMsize)) {
            return (RAM[addr-ROMsize] >> shift) & mask;
        } else {
            return (AXI[addr] >> shift) & mask;
        }
    }
}

// Generic store to RAM only.
static void StoreX (uint32_t addr, uint32_t data, int shift, int mask) {
    if ((addr<ROMsize) || (addr>=(ROMsize+RAMsize))) {
        error = -9;  return;
    }
    addr -= ROMsize;
    uint32_t temp = RAM[addr] & (~(mask << shift));
#ifdef TRACEABLE
    temp = ((data & mask) << shift) | temp;
    Trace(New, addr, RAM[addr], temp);  New=0;
    RAM[addr] = temp;
#else
    RAM[addr] = ((data & mask) << shift) | temp;
#endif // TRACEABLE
}

/// EXPORTS ////////////////////////////////////////////////////////////////////

uint32_t FetchCell(uint32_t addr) {
    if (addr & 3) {
        error = -23;
    }
    return FetchX(addr>>2, 0, 0xFFFFFFFF);
}
uint16_t FetchHalf(uint32_t addr) {
    if (addr & 1) {
        error = -23;
    }
    return FetchX(addr>>2, (addr&2)*8, 0xFFFF);
}
uint8_t FetchByte(uint32_t addr) {
    return FetchX(addr>>2, (addr&3)*8, 0xFF);
}
void StoreCell (uint32_t x, uint32_t addr) {
    if (addr & 3) {
        error = -23;
    }
    StoreX(addr>>2, x, 0, 0xFFFFFFFF);
}
void StoreHalf (uint16_t x, uint32_t addr) {
    if (addr & 1) {
        error = -23;
    }
    StoreX(addr>>2, x, (addr&2)*8, 0xFFFF);
}
void StoreByte (uint8_t x, uint32_t addr) {
    StoreX(addr>>2, x, (addr&3)*8, 0xFF);
}

#ifdef EmbeddedROM
int WriteROM(uint32_t data, uint32_t address) {
    uint32_t addr = address / 4;
    if (address & 3) return -23;        // alignment problem
    if (addr < SPIflashSize) {          // always write AXI data
// An embedded system may want to protect certain ranges
        AXI[addr] = data;
    } else {
        return -9;
    }
    if (addr < ROMsize) {
        return -9;
    }
    return 0;
}
#else
int WriteROM(uint32_t data, uint32_t address) {
    uint32_t addr = address / 4;
    if (address & 3) return -23;        // alignment problem
    if (addr < SPIflashSize) {          // always write AXI data
        AXI[addr] = data;
    } else {
        return -9;
    }
    if (addr < ROMsize)  {
        ROM[addr] = data;
    }
    return 0;
}
#endif // EmbeddedROM


// Send a stream of RAM words to the AXI bus.
// The only thing on the AXI bus here is SPI flash.
// An external function could be added in the future for other stuff.
// dest is a cell address, length is 0 to 255 meaning 1 to 256 words.
// *** Modify to only support the AXIRAMsize region after SPI flash.
static void SendAXI(uint32_t src, uint32_t dest, uint8_t length) {
    for (int i=0; i<=length; i++) {
        uint32_t old = FetchCell(dest);
        uint32_t data = FetchCell(src);
//        printf("[%X]=[%X]:%X ", dest, src, data);
        WriteROM(old & data, dest);
        src += 4;
        dest += 4;
    } return;
}

// Receive a stream of RAM words from the AXI bus.
// The only thing on the AXI bus here is SPI flash.
// An external function could be added in the future for other stuff.
// src is a cell address, length is 0 to 255 meaning 1 to 256 words.
static void ReceiveAXI(uint32_t src, uint32_t dest, uint8_t length) {
    dest -= ROMsize;
    if (dest < 0) goto bogus;            // below RAM address
    if (dest >= (RAMsize-length)) goto bogus;  // won't fit
    if (src >= (SPIflashSize+AXIRAMsize-length)) goto bogus;
    memmove(&AXI[dest], &RAM[src], length+1);  // can read all of AXI space
    return;
bogus: error = -9;                    // out of range
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
    memset(RAM, 0, RAMsize*sizeof(uint32_t));  // clear RAM
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
			case opSUB:
			    DX = (uint64_t)N - (uint64_t)T;
#ifdef TRACEABLE
                Trace(New, RidT, T, (uint32_t)DX);  New=0;
                Trace(0, RidCY, CARRY, (uint32_t)(DX>>32));
#endif // TRACEABLE
                T = (uint32_t)DX;
                CARRY = (uint32_t)(DX>>32);
                SNIP();	                                break;	// -
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
                Trace(New, RidT, T, (unsigned) T / 2);  New=0;
                Trace(0, RidCY, CARRY, T&1);
#endif // TRACEABLE
			    T = T / 2;   CARRY = T&1;               break;	// u2/
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
                M = (T*2) | (CARRY&1);
#ifdef TRACEABLE
                Trace(0, RidT, T, M);
                Trace(0, RidCY, CARRY, T>>31);

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
                ReceiveAXI(N/4, T/4, IMM);  goto ex;	        // @as
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
			case opSKIPLT: if ((signed)T >= 0) break;           // +if:
                goto ex;
			case opLIT: SDUP();
#ifdef TRACEABLE
                Trace(0, RidT, T, IMM);
#endif // TRACEABLE
                T = IMM;  goto ex;                              // lit
			case opUP: M = UP;  	                            // up
                goto GetPointer;
			case opStoreAS:  // ( src dest -- src dest ) imm length
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
ex: if (error) {
        tiffIOR = error;                // tell Tiff there was an error
        RDUP(PC<<2);
        PC = 2;                         // call an error_interrupt
        DebugReg = error;
        error = 0;
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

