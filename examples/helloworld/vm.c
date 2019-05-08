#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "vm.h"
#include "vmUser.h"
#include "flash.h"
#include <string.h>

// This file serves as the official specification for the Mforth VM.
// It is usable as a template for generating a C testbench or embedding in an app.

//
#define ROMsize 8192
//
#define RAMsize 256
//
#define SPIflashBlocks 256
//
#define EmbeddedROM

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

//
static const uint32_t InternalROM[647] = {
/*0000*/ 0x54000281, 0x54000284, 0x54000000, 0xD090C220, 0x9FB08800, 0x3FB08800,
/*0006*/ 0x5FB08800, 0x05FB8328, 0x9FB08800, 0x05FD8328, 0x3FB08800, 0xD0908800,
/*000C*/ 0xD09D0220, 0xD21D0220, 0x10408800, 0x7FC2BC08, 0xF1FF0A08, 0xF3908800,
/*0012*/ 0xF3B08800, 0xF0AF1FF0, 0x7DF08800, 0x28AF0AF0, 0x7FC08800, 0x28A4BC7C,
/*0018*/ 0xF12F1F08, 0xEFB08800, 0xE7908800, 0x7FC2BC7C, 0x7FC2BC2A, 0x7DFE7928,
/*001E*/ 0x2950001B, 0xEFBEC220, 0x07A08800, 0x04220000, 0x07005BD2, 0x05B08800,
/*0024*/ 0x90220000, 0x6E408800, 0x6E490220, 0x92408800, 0xD09B0220, 0x24927D03,
/*002A*/ 0xD1308800, 0x9BCBBC08, 0x9E7EC220, 0xC3424800, 0x08800000, 0xD3CD13D2,
/*0030*/ 0x7C220000, 0x7DFF0A0C, 0xF0A8C220, 0xD1FD0928, 0xE64C3B26, 0xEC220000,
/*0036*/ 0xC1500033, 0x08800000, 0x38220000, 0xF4000004, 0x66E08800, 0xF4000008,
/*003C*/ 0x66E08800, 0xF400000C, 0xA6EF4008, 0x67424350, 0x50220000, 0x044119B8,
/*0042*/ 0xF3B08800, 0xF400000A, 0xF40001E7, 0xD27EC220, 0xF4000010, 0xF40001E7,
/*0048*/ 0xD27EC220, 0xB8800000, 0xF4FFFFFF, 0x4C220000, 0xE79D0320, 0xC3BEFB08,
/*004E*/ 0xEFCD0928, 0x87C7FC7C, 0x7C220000, 0x28A24AE4, 0xE4320000, 0xC3B7DF7E,
/*0054*/ 0xEFBEE17E, 0x2BC2B90C, 0x4B90FC7C, 0xF2C6C800, 0xC3B7C220, 0xECAECAEC,
/*005A*/ 0x85F08800, 0xF400000C, 0x46E08800, 0x28AECAEC, 0x7C220000, 0xE796E420,
/*0060*/ 0xEBB08800, 0xEFB2A17E, 0x28AD09D0, 0xC3B7C220, 0x7E17C220, 0x281383F7,
/*0066*/ 0x05F0CAD0, 0x4DF08800, 0xD090CCF5, 0x4F427408, 0xF1500068, 0xE796EC20,
/*006C*/ 0xEB4243B2, 0xEEC08800, 0xF150006B, 0xD09D0800, 0xC1500074, 0xD09F1FF0,
/*0072*/ 0x98A9DFA0, 0x2BBEFB08, 0xEFBEC220, 0xD09D0800, 0xC150007A, 0xD09F1FF0,
/*0078*/ 0x38A3DFA0, 0x2BBEFB08, 0xEFBEC220, 0xD09D0800, 0xC1500084, 0x2417C3F0,
/*007E*/ 0x483F0AD0, 0x24800000, 0x7F4274F0, 0xD09D3CE4, 0xDB93FB28, 0x27054080,
/*0084*/ 0xEFBEC220, 0x7F9E5D68, 0xE9500089, 0x29D0007B, 0x5400008A, 0x29D00075,
/*008A*/ 0x08800000, 0xE7A54090, 0xF34249F0, 0x7FC20000, 0x4BC3E800, 0x2BBEFB08,
/*0090*/ 0xEFBEC220, 0x07AEFB08, 0xE797402F, 0xF4000003, 0x4FA54097, 0xF4000000,
/*0096*/ 0x5400008B, 0x0416DF50, 0x534249F0, 0x4BC9E800, 0x2BBEFB08, 0xD09D30EE,
/*009C*/ 0xF400003F, 0x4F427C20, 0x12803CEE, 0x08800000, 0xD09D30EE, 0xF400003F,
/*00A2*/ 0x4F427C20, 0x72803CEE, 0x08800000, 0x0416FD1F, 0xD0800000, 0x7C47CC20,
/*00A8*/ 0xE392837C, 0xE0920000, 0x28A24800, 0xC15000A7, 0xEDF7FB28, 0x2BC08800,
/*00AE*/ 0xE79D090C, 0xEF8540BE, 0xF400001F, 0xD1C10800, 0x7DFF0CF0, 0x338540B9,
/*00B4*/ 0x052D090C, 0xEC800000, 0xE12D090C, 0x04CD14EC, 0x540000BB, 0x4B4243F4,
/*00BA*/ 0x13B20000, 0x28A24800, 0xC15000B2, 0xEFBF0CD2, 0xEFBEFD00, 0xD0108800,
/*00C0*/ 0xE796DFE4, 0x7DD0002D, 0x7DD00036, 0x29D000AE, 0xF0AB0800, 0xE95000C7,
/*00C6*/ 0xD0920000, 0xF0AB0800, 0xE95000CA, 0xD0920000, 0x08800000, 0x05FE796C,
/*00CC*/ 0x7C17DD2D, 0x7DD00036, 0x29D000AE, 0xF0AB0800, 0xE95000D2, 0xD0920000,
/*00D2*/ 0xF0AB0800, 0xE95000D9, 0xD09E4800, 0xE95000D9, 0x49FF0AF0, 0xD090FCD0,
/*00D8*/ 0x27420000, 0x2BB08800, 0x06C05F20, 0xE95000DE, 0xD097DD33, 0x28800000,
/*00DE*/ 0x7C1B0800, 0xE95000E1, 0x48320000, 0x29D000AE, 0x2BA540E4, 0xF3427C20,
/*00E4*/ 0x08800000, 0xE6CF15DA, 0x740000E5, 0xEC220000, 0x740000E5, 0xF3B08800,
/*00EA*/ 0xE797406B, 0xEBC20000, 0xEC220000, 0xE79F1D6B, 0xEBC20000, 0xEC220000,
/*00F0*/ 0xE7974068, 0xEBC20000, 0xEC220000, 0xE79F1D68, 0xEBC20000, 0xEC220000,
/*00F6*/ 0x7FC483F0, 0x2B424308, 0xE742437C, 0xD090CA20, 0x54000068, 0x740000A5,
/*00FC*/ 0xEC220000, 0xE796EC7C, 0x7400002D, 0xF1D0002D, 0x740000A5, 0x2BA54103,
/*0102*/ 0x74000033, 0x08800000, 0x7DD000FD, 0x295000DA, 0x74000104, 0xF3B08800,
/*0108*/ 0x34000001, 0x54000109, 0x08800000, 0xF3D00000, 0xD1F20000, 0xE7A54117,
/*010E*/ 0xF34274F0, 0x38A6FD07, 0xD0800000, 0x25F07D01, 0x4F427DED, 0x94B88320,
/*0114*/ 0x4FC71B28, 0xC1500111, 0xEDF5410D, 0xEFB2B408, 0xE5FF4004, 0x65FF41EB,
/*011A*/ 0xD2E7FD00, 0x47D001EB, 0xD27EDD30, 0x2BD001EB, 0xD27ECAEC, 0x2816C220,
/*0120*/ 0x07AEC220, 0xF40001EB, 0xD2EBCA20, 0xF40001EB, 0xD27ECAF0, 0x7F7EFCEC,
/*0126*/ 0x28AF0220, 0x08800000, 0x04D00002, 0x08800000, 0xF4000021, 0x740000FB,
/*012C*/ 0x74000128, 0x0C800000, 0x74000127, 0x05D00128, 0x7400006B, 0xE950012E,
/*0132*/ 0xEC220000, 0xF3D0FFFF, 0x4C334000, 0x08800000, 0x74000133, 0xEC220000,
/*0138*/ 0x74000127, 0xF4060000, 0x34000000, 0xE9500138, 0xF4010000, 0x54000136,
/*013E*/ 0x54000162, 0x3950013E, 0x040A0D02, 0x4A325B1B, 0xF4000500, 0x5400013F,
/*0144*/ 0xF4000503, 0x5400013F, 0xF4000000, 0x34000000, 0x54000127, 0x74000146,
/*014A*/ 0xE9500149, 0xF4020000, 0x34000000, 0x08800000, 0x000004E0, 0x00000508,
/*0150*/ 0x00000510, 0x00000518, 0x00000524, 0xF4000538, 0xF40001CF, 0xD27EC220,
/*0156*/ 0x104F41CF, 0xD2E0EE7E, 0xF4000000, 0x54000156, 0xF4000001, 0x54000156,
/*015C*/ 0xF4000002, 0x54000156, 0xF4000003, 0x54000156, 0xF4000004, 0x54000156,
/*0162*/ 0x07AEFB08, 0xD0920000, 0x25F38800, 0x74000158, 0x2B054164, 0xEFB08800,
/*0168*/ 0xF4000020, 0x54000158, 0xC3B08800, 0x74000168, 0xD09D0800, 0x5400016A,
/*016E*/ 0x08800000, 0x07D00009, 0xD03B3D06, 0xD130FD37, 0x0C220000, 0xF40000CB,
/*0174*/ 0xD3D00197, 0xD27EC220, 0xF4000197, 0xD017EED0, 0x27404A9C, 0xECFEC220,
/*017A*/ 0x07D001E7, 0xD2E7C800, 0xE9500183, 0xF4000000, 0x49D000AE, 0x2BC7DDAE,
/*0180*/ 0xF1D0016F, 0x74000176, 0x28220000, 0x29D000AE, 0xF1D0016F, 0x74000176,
/*0186*/ 0x0416C220, 0x7400017A, 0xE797402F, 0x93A54187, 0x08800000, 0xB3A5418E,
/*018C*/ 0xF400002D, 0x74000176, 0x08800000, 0xEFBF4197, 0xD2EF40CB, 0xD39D090E,
/*0192*/ 0xE7424320, 0x7400016A, 0x5400013E, 0x7C17DD36, 0x74000173, 0x74000187,
/*0198*/ 0x29D0018B, 0x7400018F, 0x29500192, 0xF4000000, 0xF1500195, 0x7DD00022,
/*019E*/ 0x29500195, 0xF4000000, 0x74000195, 0x54000168, 0xF4000000, 0x5400019F,
/*01A4*/ 0xF40001E7, 0xD2EF400A, 0x6FA541A8, 0x540001A2, 0x74000022, 0x5400019F,
/*01AA*/ 0xB95001A4, 0x74000173, 0xD0920000, 0x7DD0017A, 0x28920000, 0xC15001AD,
/*01B0*/ 0xEDD00187, 0x5400018F, 0xF40001E7, 0xD2E7DD46, 0xF4000000, 0xF1D001AB,
/*01B6*/ 0x2BD001E7, 0xD27EC800, 0x7400013E, 0x54000168, 0x7C104A0C, 0x7C800000,
/*01BC*/ 0x0526C800, 0xE95001DE, 0x74000127, 0x7400015E, 0xE95001BE, 0x74000160,
/*01C2*/ 0x07D0000D, 0x6E420000, 0xE95001C7, 0xECAEFCD0, 0x24308800, 0x07D00008,
/*01C8*/ 0x6E420000, 0xE95001D3, 0xEF9E5B20, 0xE95001D2, 0xD09D0800, 0x540001D0,
/*01CE*/ 0x445B1B07, 0x445B1B20, 0xF4000738, 0x39D0013E, 0x540001DD, 0x07D00020,
/*01D4*/ 0xD090C800, 0xC15001DC, 0xEFD001B1, 0xD3690800, 0xE95001DA, 0x05D00158,
/*01DA*/ 0xF0F20000, 0x540001DD, 0xEFB20000, 0x540001BC, 0x2BBF3427, 0x08800000,
/*01E0*/ 0x9FB08800, 0x7F4F40FF, 0x4D2F4003, 0x4C410420, 0x7400009B, 0xD0AF4003,
/*01E6*/ 0xD139FB08, 0xD0920000, 0x7F9DB920, 0x740001E1, 0x27C27C28, 0x270541E8,
/*01EC*/ 0xEFBEC220, 0xF4000000, 0xF400019C, 0xD0FEC220, 0xF4000001, 0xF400019C,
/*01F2*/ 0xD0FEC220, 0xF400019C, 0xD3620000, 0xE95001F8, 0xF40001DF, 0xD0220000,
/*01F8*/ 0xF40001DB, 0xD0220000, 0x05FB8800, 0x740001E0, 0xF4000004, 0x29500007,
/*01FE*/ 0xF40001DF, 0xD15001FA, 0xF40001E3, 0xD15001FA, 0xF40001DB, 0xD017EE9C,
/*0204*/ 0xEFD00004, 0x29500007, 0xF400019C, 0xD3620000, 0xE950020A, 0x540001FE,
/*020A*/ 0x54000202, 0x05FB8800, 0x740001E1, 0x4AE24A9C, 0xEC220000, 0xF40001DF,
/*0210*/ 0xD150020B, 0xF40001E3, 0xD150020B, 0xF40001DB, 0xD017EE3C, 0xEFD00001,
/*0216*/ 0x29500007, 0xF400019C, 0xD3620000, 0xE950021B, 0x5400020F, 0x54000213,
/*021C*/ 0xE7D001DB, 0xD2E6D308, 0xF40001DF, 0xD2EF3D00, 0xD1D001FE, 0x06E90800,
/*0222*/ 0x7400021C, 0xE9500225, 0x85500221, 0x041741FE, 0x06E92420, 0x7400021C,
/*0228*/ 0xE950022B, 0x99D001FE, 0x54000226, 0x08800000, 0xF40001C7, 0xD2EF41BF,
/*022E*/ 0xD27EC800, 0xF4000197, 0xD3D000CC, 0x74000091, 0xF4000000, 0xA4800000,
/*0234*/ 0x7400021E, 0x7F42520C, 0x514F39F0, 0x740001E0, 0x2BC90800, 0xE9500234,
/*023A*/ 0xEC220000, 0x54000240, 0x6C65480C, 0x57206F6C, 0x646C726F, 0xFFFFFF21,
/*0240*/ 0xF40008F0, 0x39D0013E, 0x7400015A, 0xF400000A, 0xF4000000, 0xF3425F7C,
/*0246*/ 0x49D001A4, 0x74000051, 0x54000246, 0x5400015A, 0x00000003, 0xFFFFFE04,
/*024C*/ 0xFFFFFE00, 0xFFFFFDF0, 0xFFFFFD00, 0x00000004, 0xFFFFFE18, 0x0000000A,
/*0252*/ 0x0000A488, 0x0000094C, 0xFFFFFF38, 0x00000006, 0xFFFFFE2C, 0xFFFFFF34,
/*0258*/ 0x00000538, 0x00000001, 0x0000000C, 0xFFFFFE6C, 0x0000000C, 0x00000001,
/*025E*/ 0xFFFFFE48, 0x001A0001, 0x00000003, 0xFFFFFE5C, 0x0000A44C, 0x00050003,
/*0264*/ 0xFFFFFF34, 0x00000001, 0xFFFFFF34, 0x0000A464, 0x00000000, 0xFFFFFF38,
/*026A*/ 0xF40001FF, 0xD01D0920, 0x74000091, 0xF4000928, 0x9B9E4910, 0x103F1FF0,
/*0270*/ 0x28120000, 0xE9500274, 0x7E629D6F, 0x5400026E, 0x2BD00197, 0xD27EC800,
/*0276*/ 0xF40001FF, 0xD3FF42FF, 0xD37F4213, 0xD2FF4197, 0xD2E7C800, 0xF4010000,
/*027C*/ 0xF40001DF, 0xD27EC800, 0xF4018000, 0xF40001E3, 0xD27EC220, 0x7400026A,
/*0282*/ 0x7400023B, 0x54000108, 0x7400026A, 0x7400023B, 0x54000108};
//
uint32_t FetchROM(uint32_t addr) {
//
  if (addr < 647) {
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

#ifdef EmbeddedROM
    static uint32_t RAM[RAMsize];
#else
    static uint32_t * ROM;
    static uint32_t * RAM;
#endif // EmbeddedROM
    static uint32_t AXI[AXIRAMsize];

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
		case 2: return (RP-ROMsize)*4;
		case 3: return (SP-ROMsize)*4;
		case 4: return (UP-RAMsize)*4;
		case 5: return PC*4;
		default: return 0;
	}
}

