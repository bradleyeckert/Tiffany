#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "vm.h"
#include <string.h>

#define IMM    (IR & ~(-1<<slot))

//
#define ROMsize 2048
//
#define RAMsize 512

//
int tiffIOR; // error code
//
static const uint32_t ROM[482] = {
/*0000*/ 0x4C0001D9, 0x96B09000, 0x36B09000, 0x56B09000, 0x05AB8318, 0x96B09000,
/*0006*/ 0x05AD8318, 0x36B09000, 0xFC909000, 0xFC9FC240, 0x9E709000, 0x68A18A08,
/*000C*/ 0x29A28608, 0x2A209000, 0x2AB09000, 0x28629A28, 0x69A09000, 0x18628628,
/*0012*/ 0x68A09000, 0x186F8A68, 0x2BE29A08, 0xAEB09000, 0x8A209000, 0x68A18A68,
/*0018*/ 0x68A18A1A, 0x69A8A218, 0x19300017, 0x6C00000F, 0x6C000017, 0x6C000011,
/*001E*/ 0x4C000017, 0x6C00001B, 0x4C00001B, 0x6C000017, 0xAEB09000, 0x6C000017,
/*0024*/ 0x4C000019, 0xAEBAC240, 0x05209000, 0x04240000, 0x07805F08, 0x05FFC240,
/*002A*/ 0x7DD09000, 0x7DD74240, 0x75D09000, 0xFC914240, 0x449E4003, 0xFD709000,
/*0030*/ 0x6A71AF08, 0x98AB8A08, 0x965AC240, 0xC3F25000, 0x09000000, 0x69A2860C,
/*0036*/ 0x2868C240, 0xFCAFD7FE, 0x68240000, 0xFDAFC918, 0x89DC2B26, 0xAC240000,
/*003C*/ 0xC1300039, 0x09000000, 0xE4000004, 0xCAE09000, 0xE4000008, 0xCAE09000,
/*0042*/ 0xE400000C, 0xEAEE4008, 0xC8B1C708, 0x0679F2B8, 0x2AB09000, 0xE400000A,
/*0048*/ 0xE4002218, 0x96B09000, 0xE4000010, 0xE4002218, 0x96B09000, 0xB9000000,
/*004E*/ 0xE4FFFFFF, 0x5C240000, 0x18624688, 0x88340000, 0xC2B69A6A, 0xAEBAE16A,
/*0054*/ 0xE400000C, 0xAAE09000, 0x186AC6AC, 0x68240000, 0xFC940000, 0xE2BAEB08,
/*005A*/ 0x24A68A40, 0x98695AA0, 0x1ABAEB08, 0xFC940000, 0xE2BAEB08, 0x24A68A40,
/*0060*/ 0x38635AA0, 0x1ABAEB08, 0x2BF25000, 0xE2BAEB08, 0x24A68A40, 0xF8A36818,
/*0066*/ 0xAEBAC240, 0xE400003F, 0x5FF278AE, 0x24A40000, 0x9E82AB08, 0x09000000,
/*006C*/ 0xE400003F, 0x5FF278AE, 0x24A40000, 0x3E82AB08, 0x09000000, 0xE4000000,
/*0072*/ 0xE400001F, 0xFD000000, 0x6A76AF40, 0x22218368, 0x20940000, 0x18625000,
/*0078*/ 0xC1300074, 0xADA6AB18, 0x18A09000, 0xAEBAF900, 0xFC109000, 0x8A22EB40,
/*007E*/ 0x2130007B, 0xE400001F, 0xFCF9D000, 0x69A6AF18, 0xBC84C086, 0x07E2EB40,
/*0084*/ 0x23E2D000, 0x4C000088, 0xF8BE4000, 0x9EB40000, 0x18625000, 0xC1300081,
/*008A*/ 0xAEB2AF08, 0x04505A40, 0x4930008F, 0xFC969B39, 0x19000000, 0x68115000,
/*0090*/ 0x49300092, 0xF8340000, 0x19B0007D, 0x1924C095, 0x2BF24A40, 0x09000000,
/*0096*/ 0x8852938B, 0x6C000096, 0xAC240000, 0x6C000096, 0x2AB09000, 0x8A27C540,
/*009C*/ 0x48B14240, 0x2AB14240, 0x2930009B, 0x8A27C540, 0x48B14240, 0xAC509000,
/*00A2*/ 0x2930009F, 0x8A26C09F, 0x48A40000, 0xAC240000, 0x8A229B9F, 0x48A40000,
/*00A8*/ 0xAC240000, 0x8A26C09B, 0x48A40000, 0xAC240000, 0x8A229B9B, 0x48A40000,
/*00AE*/ 0xAC240000, 0x68AF8328, 0x18B09000, 0x88B68B18, 0x4C00009B, 0x2930009B,
/*00B4*/ 0x2930009F, 0x6C000071, 0xAC240000, 0x8A27C568, 0x6C000033, 0x29B00033,
/*00BA*/ 0x6C000071, 0x1924C0BD, 0x6C000039, 0x09000000, 0x69B000B7, 0x1930008B,
/*00C0*/ 0x6C0000BE, 0x2AB09000, 0x89AE4004, 0xC9AE6214, 0xB9AE4000, 0xAB902214,
/*00C6*/ 0x96B6C038, 0x1B902214, 0x96B1AB19, 0x7C240000, 0x052AC240, 0xE4002214,
/*00CC*/ 0xBAD19000, 0xE4002214, 0x96B18A68, 0xD6B2AB18, 0x18A09000, 0x09000000,
/*00D2*/ 0x09000000, 0x04400004, 0x09000000, 0xE400000A, 0x6C0000B5, 0x6C0000D3,
/*00D8*/ 0x0D000000, 0x6C0000D1, 0x05B000D3, 0x6C00009F, 0x493000D9, 0xAC240000,
/*00DE*/ 0x6C0000D1, 0x04400003, 0x493000DE, 0x10000002, 0xAC240000, 0x4C000121,
/*00E4*/ 0x393000E3, 0xFF0A0D02, 0x325B1B04, 0xFFFFFF4A, 0xE4000394, 0x4C0000E4,
/*00EA*/ 0xE4000398, 0x4C0000E4, 0x04400000, 0x09000000, 0x04400001, 0x09000000,
/*00F0*/ 0x00000378, 0x000003A0, 0x000003A8, 0x000003B0, 0x000003B8, 0xE40003C0,
/*00F6*/ 0xE4002234, 0x96B09000, 0x9E7E6234, 0xB83B9A08, 0xE4000000, 0x4C0000F8,
/*00FC*/ 0xE4000001, 0x4C0000F8, 0xE4000002, 0x4C0000F8, 0xE4000003, 0x4C0000F8,
/*0102*/ 0xE4000004, 0x4C0000F8, 0x89AFC9FC, 0x289286DA, 0xE4000006, 0x6C000067,
/*0108*/ 0x69B00104, 0xE400003F, 0x5C60C240, 0x6C000104, 0x079000C0, 0x5F900080,
/*010E*/ 0x7DD40000, 0x49300111, 0xAD30010B, 0x07900080, 0x5D24C120, 0x07900020,
/*0114*/ 0x5D24C11E, 0x07900010, 0x5D24C11B, 0xE4000007, 0x5DB00106, 0x6C000106,
/*011A*/ 0x4C000106, 0xE400000F, 0x5DB00106, 0x4C000106, 0xE400001F, 0x5D300106,
/*0120*/ 0x09000000, 0x07F27F14, 0xFD24C126, 0x6C00010B, 0x6C0000FA, 0x4C000121,
/*0126*/ 0xAEB09000, 0xE4000020, 0x4C0000FA, 0xFC940000, 0xE2B09000, 0x6C000127,
/*012C*/ 0x2530012A, 0x09000000, 0xE4000009, 0x89B0009F, 0xE4000007, 0x5C3E4030,
/*0132*/ 0x0C240000, 0xE4002350, 0xE4002284, 0x96B09000, 0xE4002284, 0xBBF27F04,
/*0138*/ 0xE4002284, 0x96B36B08, 0xE4000000, 0xE4002218, 0xB9B0007D, 0x6B902218,
/*013E*/ 0xB9B0007D, 0x29B0012E, 0x6C000136, 0x18240000, 0x6C00013A, 0x8A26C037,
/*0144*/ 0x7524C142, 0x09000000, 0x1524C149, 0xE400002D, 0x6C000136, 0x09000000,
/*014A*/ 0xAEBE6284, 0xBB902350, 0x88B09000, 0x88B6C129, 0x4C0000E3, 0x68169B3C,
/*0150*/ 0x6C000133, 0x6C000142, 0x19B00146, 0x6C00014A, 0x1930014D, 0xE4000000,
/*0156*/ 0x2930014F, 0x69B00028, 0x1930014F, 0xE4000000, 0x6C00014F, 0x4C000127,
/*015C*/ 0xE4000000, 0x4C000159, 0xE4002218, 0xBB90000A, 0x7D24C162, 0x4C00015C,
/*0162*/ 0x6C000028, 0x4C000159, 0xB930015E, 0x6C000133, 0xFC940000, 0x69B0013A,
/*0168*/ 0x18940000, 0xC1300167, 0xADB00142, 0x4C00014A, 0xE4002218, 0xB9A6C04A,
/*016E*/ 0xE4000000, 0x29B00165, 0x1B902218, 0x96B6C0E3, 0x4C000127, 0x10000005,
/*0174*/ 0x09000000, 0x69B00173, 0xAF902284, 0x05A96B18, 0x39A39AD8, 0x6C000173,
/*017A*/ 0xAC66C173, 0xAC618340, 0x6C000173, 0xAC240000, 0xE4002284, 0x06E68A68,
/*0180*/ 0x69B00173, 0xAFE96B18, 0x39A39AD8, 0x6C000173, 0xAC66C173, 0xAC618340,
/*0186*/ 0x6C000173, 0xAC6E6284, 0x96B09000, 0xE4000005, 0x6C000173, 0xAF9001FF,
/*018C*/ 0x4C000173, 0x6C000189, 0xE4000001, 0x5DD40000, 0x4930018D, 0x09000000,
/*0192*/ 0xE400009F, 0x6C000173, 0x05F6C173, 0x05F6C173, 0x05F4C173, 0xE4000106,
/*0198*/ 0x6C000173, 0xAF900020, 0xE4000100, 0x6C00017E, 0xE4000104, 0x6C000173,
/*019E*/ 0xAD30018D, 0xE4000106, 0x6C000173, 0xACAE4002, 0xE4000000, 0x6C00017E,
/*01A4*/ 0xFC9FDA38, 0xF9D40000, 0x493001AD, 0xE4000100, 0x0DB00173, 0xAF900104,
/*01AA*/ 0x6C000173, 0xADB0018D, 0x1AB09000, 0x6C000173, 0xAC64C1A4, 0x09000000,
/*01B0*/ 0x052AEBAE, 0x8BFE40FF, 0x5C989A40, 0x6C0000A3, 0x68A8BE40, 0x6C00019F,
/*01B6*/ 0x2BE0C618, 0x28B4C1B0, 0x09000000, 0x2B902284, 0x96BE6284, 0x2B900004,
/*01BC*/ 0x4C0001B0, 0xE40000FF, 0x6C000173, 0x28D09000, 0xE400000B, 0xE4000000,
/*01C2*/ 0x6C00017E, 0x05B00173, 0xAF902284, 0x6C0001BD, 0x6C0001BD, 0x6C0001BD,
/*01C8*/ 0x6C0001BD, 0xAF9001FF, 0x6C000173, 0xAF902284, 0xB8240000, 0x1B902218,
/*01CE*/ 0x96BE6200, 0xF7902100, 0x0790000C, 0xEA5AF540, 0xE40021F0, 0x07900008,
/*01D4*/ 0xEA5AF904, 0x2ED6C0D2, 0x6C0000F5, 0xE4002218, 0xB9A4C047, 0x6C0001CD,
/*01DA*/ 0x4C0001DE, 0x6C65480B, 0x57206F6C, 0x646C726F, 0xE400076C, 0x39B000E3,
/*01E0*/ 0x10000006, 0x09000000};
//
uint32_t FetchROM(uint32_t addr) {
//
  if (addr < 482) {
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
    unsigned long cyclecount;

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
//
#ifdef __NEVER_INCLUDE__
    static uint32_t ROM[ROMsize];
//
#endif
    uint32_t AXI[SPIflashSize+AXIRAMsize];

    static void SDUP(void)  { RAM[--SP & (RAMsize-1)] = N;  N = T; }
    static void SDROP(void) { T = N;  N = RAM[SP++ & (RAMsize-1)]; }
    static void SNIP(void)  { N = RAM[SP++ & (RAMsize-1)]; }
    static void RDUP(uint32_t x)  { RAM[--RP & (RAMsize-1)] = x; }
    static uint32_t RDROP(void) { return RAM[RP++ & (RAMsize-1)]; }

#endif // TRACEABLE

//
#ifdef __NEVER_INCLUDE__
/// Tiff's ROM write functions for populating internal ROM.
/// A copy may be stored to SPI flash for targets that boot from SPI.
/// An MCU-based system will have ROM in actual ROM.
/// The target system does not use WriteROM, only the host.

int WriteROM(uint32_t data, uint32_t address) { // EXPORTED
    uint32_t addr = address / 4;
    if (address & 3) return -23;        // alignment problem
    if (addr < ROMsize)  {
        ROM[addr] = data;
        return 0;
    }
    if (addr < SPIflashSize) {          // only flash region is writable
        AXI[addr] = data;
        return 0;
    }
    return -9;                          // out of range
}
//
#endif

/// The VM's RAM and ROM are internal to this module.
/// They are both little endian regardless of the target machine.
/// RAM and ROM are accessed only through execution of an instruction
/// group. Writing to ROM is a special case, depending on a USER function
/// with the restriction that you can't turn '0's into '1's.


// Send a stream of RAM words to the AXI bus.
// The only thing on the AXI bus here is SPI flash.
// An external function could be added in the future for other stuff.
// dest is a cell address, length is 0 to 255 meaning 1 to 256 words.
// *** Modify to only support the AXIRAMsize region after SPI flash.
static void SendAXI(int src, unsigned int dest, uint8_t length) {
    uint32_t old, data;     int i;
    src -= ROMsize;
    if (src < 0) goto bogus;            // below RAM address
    if (src >= (RAMsize-length)) goto bogus;
    if (dest >= (SPIflashSize-length)) goto bogus;
    for (i=0; i<=length; i++) {
        old = AXI[dest];		        // existing flash data
        data = RAM[src++];
        if (~(old|data)) {
            tiffIOR = -60;              // not erased
            return;
        }
        AXI[dest++] = old & data;
    } return;
bogus: tiffIOR = -9;                    // out of range
}

// Receive a stream of RAM words from the AXI bus.
// The only thing on the AXI bus here is SPI flash.
// An external function could be added in the future for other stuff.
// src is a cell address, length is 0 to 255 meaning 1 to 256 words.
static void ReceiveAXI(unsigned int src, int dest, uint8_t length) {
    dest -= ROMsize;
    if (dest < 0) goto bogus;            // below RAM address
    if (dest >= (RAMsize-length)) goto bogus;  // won't fit
    if (src >= (SPIflashSize+AXIRAMsize-length)) goto bogus;
    memmove(&AXI[dest], &RAM[src], length+1);  // can read all of AXI space
    return;
bogus: tiffIOR = -9;                    // out of range
}

// Generic fetch from ROM or RAM: ROM is at the bottom, RAM is in middle, AXI is at top
static int32_t FetchX (int32_t addr, int shift, int mask) {
    if (addr < ROMsize) {
        return (ROM[addr] >> shift) & mask;
    } else {
        if (addr < (ROMsize + RAMsize)) {
            return (RAM[addr-ROMsize] >> shift) & mask;
        } else if (addr < (SPIflashSize+AXIRAMsize)) {
            return (AXI[addr] >> shift) & mask;
        } else return 0;
    }
}

// Generic store to RAM: ROM is at the bottom, RAM wraps.
static void StoreX (uint32_t addr, uint32_t data, int shift, int mask) {
    uint32_t temp;
    addr = addr & (RAMsize-1);
    temp = RAM[addr] & (~(mask << shift));
#ifdef TRACEABLE
    temp = ((data & mask) << shift) | temp;
    Trace(New, addr, RAM[addr], temp);  New=0;
    RAM[addr] = temp;
#else
    RAM[addr] = ((data & mask) << shift) | temp;
#endif // TRACEABLE
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
    memset(OpCounter, 0, 64);           // clear opcode profile counters
    cyclecount = 0;                     // cycles since POR
#endif // TRACEABLE
    PC = 0;  RP = 64;  SP = 32;  UP = 64;
    T=0;  N=0;  DebugReg = 0;
    memset(RAM, 0, RAMsize);            // clear RAM
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
        if (OpCounter[opcode] != 0xFFFFFFFF) OpCounter[opcode]++;
        New = 1;  // first state change in an opcode
        if (!Paused) cyclecount += 1;
#endif // TRACEABLE
        switch (opcode) {
			case opNOP:									break;	// nop
			case opDUP: SDUP();							break;	// dup
			case opEXIT:
                M = RDROP() >> 2;
#ifdef TRACEABLE
                Trace(New, RidPC, PC, M);  New=0;
                if (!Paused) cyclecount += 3;  // PC change flushes pipeline
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
                Trace(New, RidT, T, (signed)T / 2);  New=0;
                Trace(0, RidCY, CARRY, T&1);
#endif // TRACEABLE
			    T = (signed)T / 2;  CARRY = T&1;        break;	// 2/
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
                Trace(0, RidCY, CARRY, ~(uint32_t)(DX>>32));
#endif // TRACEABLE
                T = (uint32_t)DX;
                CARRY = ~(uint32_t)(DX>>32);
                SNIP();	                                break;	// -
			case opCstorePlus:    /* ( n a -- a' ) */
                StoreX(T>>2, N, (T&3)*8, 0xFF);
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
                if (!Paused) cyclecount += 3;
				// PC change flushes pipeline in HW version
#endif // TRACEABLE
                // Jumps and calls use cell addressing
			    PC = IMM;  goto ex;                             // jmp
			case opWstorePlus:    /* ( n a -- a' ) */
                StoreX(T>>2, N, (T&2)*8, 0xFFFF);
#ifdef TRACEABLE
                Trace(0, RidT, T, T+2);
#endif // TRACEABLE
                T += 2;   SNIP();                       break;  // w!+
			case opWfetchPlus:  SDUP();  /* ( a -- a' c ) */
                M = FetchX(N>>2, (N&2) * 8, 0xFFFF);
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
                if (!Paused) cyclecount += 3;
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
                M = FetchX(T>>2, (T&2) * 8, 0xFFFF);
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
                StoreX(T>>2, N, 0, 0xFFFFFFFF);
#ifdef TRACEABLE
                Trace(0, RidT, T, T+4);
#endif // TRACEABLE
                T += 4;   SNIP();                       break;  // !+
			case opFetchPlus:  SDUP();  /* ( a -- a' c ) */
                M = FetchX(N>>2, 0, 0xFFFFFFFF);
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
                M = (T>>2) & (RAMsize-1);
#ifdef TRACEABLE
                Trace(New, RidRP, RP, M);  New=0;
#endif // TRACEABLE
			    RP = M;  SDROP();                       break;	// rp!
			case opFetch:  /* ( a -- n ) */
                M = FetchX(T>>2, 0, 0xFFFFFFFF);
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
                break;	// port
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
                SendAXI(N/4, T/4, IMM);  goto ex;               // !as
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
ex: return PC;
}

// write to the debug mailbox
void SetDbgReg(uint32_t n) {  // EXPORTED
    DebugReg = n;
}

// read from the debug mailbox
uint32_t GetDbgReg(void) {  // EXPORTED
    return DebugReg;
}

// Only use for testbench
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
