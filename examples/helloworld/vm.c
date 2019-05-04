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
#define RAMsize 1024
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
static const uint32_t InternalROM[667] = {
/*0000*/ 0x54000295, 0x54000298, 0x54000000, 0xD090C220, 0x9FB08800, 0x3FB08800,
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
/*0042*/ 0xF3B08800, 0xF400000A, 0xF4000DE7, 0xD27EC220, 0xF4000010, 0xF4000DE7,
/*0048*/ 0xD27EC220, 0xB8800000, 0xF4FFFFFF, 0x4C220000, 0xE79D0320, 0xC3BEFB08,
/*004E*/ 0xEFCD0928, 0x87C7FC7C, 0x7C220000, 0x28A24AE4, 0xE4320000, 0xC3B7DF7E,
/*0054*/ 0xEFBEE17E, 0x2BC2B90C, 0x4B90FC7C, 0xF2C6C800, 0xC3B7C220, 0xECAECAEC,
/*005A*/ 0x85F08800, 0xF400000C, 0x46E08800, 0x28AECAEC, 0x7C220000, 0xE796E420,
/*0060*/ 0xEBB08800, 0xEFB2A17E, 0x28AD09D0, 0xC3B7C220, 0x7E17C220, 0x281383F7,
/*0066*/ 0x05F0CAD0, 0x4DF08800, 0xE796EC20, 0xEB4243B2, 0xF3BB0220, 0xF1500068,
/*006C*/ 0xE796EC20, 0xEB4243B2, 0xEEC08800, 0xF150006C, 0xD09D0800, 0xC1500075,
/*0072*/ 0xD09F1FF0, 0x98A9DFA0, 0x2BBEFB08, 0xEFBEC220, 0xD09D0800, 0xC150007B,
/*0078*/ 0xD09F1FF0, 0x38A3DFA0, 0x2BBEFB08, 0xEFBEC220, 0xD09D0800, 0xC1500085,
/*007E*/ 0x2417C3F0, 0x483F0AD0, 0x24800000, 0x7F4274F0, 0xD09D3CE4, 0xDB93FB28,
/*0084*/ 0x27054081, 0xEFBEC220, 0x7F9E5D68, 0xE950008A, 0x29D0007C, 0x5400008B,
/*008A*/ 0x29D00076, 0x08800000, 0xE7A54091, 0xF34249F0, 0x7FC20000, 0x4BC3E828,
/*0090*/ 0xEFBEC220, 0xEFBEC220, 0x07AEFB08, 0xE797402F, 0xF4000003, 0x4FA54098,
/*0096*/ 0xF4000000, 0x5400008C, 0x0416DF50, 0x534249F0, 0x4BC9E828, 0xEFBEC220,
/*009C*/ 0xD09D30EE, 0xF400003F, 0x4F427C20, 0x128F3B08, 0x08800000, 0xD09D30EE,
/*00A2*/ 0xF400003F, 0x4F427C20, 0x728F3B08, 0x08800000, 0x0416FD1F, 0xD0800000,
/*00A8*/ 0x7C47CC20, 0xE392837C, 0xE0920000, 0x28A24800, 0xC15000A8, 0xEDF7FB28,
/*00AE*/ 0x2BC08800, 0xE79D090C, 0xEF8540BF, 0xF400001F, 0xD1C10800, 0x7DF7CC28,
/*00B4*/ 0x338540BA, 0x052D090C, 0xEC800000, 0xE12D090C, 0x04CD14EC, 0x540000BC,
/*00BA*/ 0x4B4243F4, 0x13B20000, 0x28A24800, 0xC15000B3, 0xEFBF0CD2, 0xEFBEFD00,
/*00C0*/ 0xD0108800, 0xE796DFE4, 0x7DD0002D, 0x7DD00036, 0x29D000AF, 0xF0AB0800,
/*00C6*/ 0xE95000C8, 0xD0920000, 0xF0AB0800, 0xE95000CB, 0xD0920000, 0x08800000,
/*00CC*/ 0x05FE796C, 0x7C17DD2D, 0x7DD00036, 0x29D000AF, 0xF0AB0800, 0xE95000D3,
/*00D2*/ 0xD0920000, 0xF0AB0800, 0xE95000DA, 0xD09E4800, 0xE95000DA, 0x49FF0AF0,
/*00D8*/ 0xD090FCD0, 0x27420000, 0x2BB08800, 0x06C05F20, 0xE95000DF, 0xD097DD33,
/*00DE*/ 0x28800000, 0x7C1B0800, 0xE95000E2, 0x48320000, 0x29D000AF, 0x2BA540E5,
/*00E4*/ 0xF3427C20, 0x08800000, 0xE6CF15DB, 0x740000E6, 0xEC220000, 0x740000E6,
/*00EA*/ 0xF3B08800, 0xE797406C, 0xEBC20000, 0xEC220000, 0xE79F1D6C, 0xEBC20000,
/*00F0*/ 0xEC220000, 0xE7974068, 0xEBC20000, 0xEC220000, 0xE79F1D68, 0xEBC20000,
/*00F6*/ 0xEC220000, 0x7FC483F0, 0x2B424308, 0xE742437C, 0xD090CA20, 0x54000068,
/*00FC*/ 0x740000A6, 0xEC220000, 0xE796EC7C, 0x7400002D, 0xF1D0002D, 0x740000A6,
/*0102*/ 0x2BA54104, 0x74000033, 0x08800000, 0x7DD000FE, 0x295000DB, 0x74000105,
/*0108*/ 0xF3B08800, 0x34000006, 0x08800000, 0xF3D00000, 0xD1F20000, 0xE7A54117,
/*010E*/ 0xF34274F0, 0x38A6FD07, 0xD0800000, 0x25F07D01, 0x4F427DED, 0x94B88320,
/*0114*/ 0x4FC71B28, 0xC1500111, 0xEDF5410D, 0xEFB2B408, 0xE5FF4004, 0x65FF4DEB,
/*011A*/ 0xD2E7FD00, 0x47D00DEB, 0xD27EDD30, 0x2BD00DEB, 0xD27ECAEC, 0x2816C220,
/*0120*/ 0x07AEC220, 0xF4000DEB, 0xD2EBCA20, 0xF4000DEB, 0xD27ECAF0, 0x7F7EFCEC,
/*0126*/ 0x28AF0220, 0x08800000, 0x04D00004, 0x08800000, 0xF400000A, 0x740000FC,
/*012C*/ 0x74000128, 0x0C800000, 0x74000127, 0x05D00128, 0x7400006C, 0xE950012E,
/*0132*/ 0xEC220000, 0x74000127, 0x04D00003, 0xE9500133, 0x34000002, 0xEC220000,
/*0138*/ 0x54000177, 0x39500138, 0x040A0D02, 0x4A325B1B, 0xF40004E8, 0x54000139,
/*013E*/ 0xF40004EB, 0x54000139, 0x04D00000, 0x54000127, 0x74000140, 0xE9500142,
/*0144*/ 0x04D00001, 0x08800000, 0x000004CC, 0x000004F0, 0x000004F8, 0x00000500,
/*014A*/ 0x00000508, 0xF4000518, 0xF4000DCF, 0xD27EC220, 0x104F4DCF, 0xD2E0EE7E,
/*0150*/ 0xF4000000, 0x5400014E, 0xF4000001, 0x5400014E, 0xF4000002, 0x5400014E,
/*0156*/ 0xF4000003, 0x5400014E, 0xF4000004, 0x5400014E, 0xE5FD09D0, 0xF09F0ADA,
/*015C*/ 0xF4000006, 0x7400009C, 0x7DD0015A, 0xF400003F, 0x4CA0C220, 0x7400015A,
/*0162*/ 0x07D000C0, 0x4FD00080, 0x6E420000, 0xE9500167, 0xED500161, 0x07D00080,
/*0168*/ 0x4FA54176, 0x07D00020, 0x4FA54174, 0x07D00010, 0x4FA54171, 0xF4000007,
/*016E*/ 0x4DD0015C, 0x7400015C, 0x5400015C, 0xF400000F, 0x4DD0015C, 0x5400015C,
/*0174*/ 0xF400001F, 0x4D50015C, 0x08800000, 0x074274B0, 0xD3A5417C, 0x74000161,
/*017A*/ 0x74000150, 0x54000177, 0xEFB08800, 0xF4000020, 0x54000150, 0xC3B08800,
/*0180*/ 0x7400017D, 0xD09D0800, 0x5400017F, 0x08800000, 0xF4000009, 0xE5D0006C,
/*0186*/ 0xF4000007, 0x4C3F4030, 0x0C220000, 0xF4000CCB, 0xD3D00D97, 0xD27EC220,
/*018C*/ 0xF4000D97, 0xD2ED09D1, 0xF4000D97, 0xD27ECFEE, 0xF4000000, 0xF4000DE7,
/*0192*/ 0xD2E740AF, 0x7FD00DE7, 0xD2E740AF, 0xF1D00184, 0x7400018C, 0x28220000,
/*0198*/ 0x74000190, 0xE797402F, 0x93A54198, 0x08800000, 0xB3A5419F, 0xF400002D,
/*019E*/ 0x7400018C, 0x08800000, 0xEFBF4D97, 0xD2EF4CCB, 0xD39D090E, 0xE7424320,
/*01A4*/ 0x7400017F, 0x54000138, 0x7C17DD36, 0x74000189, 0x74000198, 0x29D0019C,
/*01AA*/ 0x740001A0, 0x295001A3, 0xF4000000, 0xF15001A6, 0x7DD00022, 0x295001A6,
/*01B0*/ 0xF4000000, 0x740001A6, 0x5400017D, 0xF4000000, 0x540001B0, 0xF4000DE7,
/*01B6*/ 0xD2EF400A, 0x6FA541B9, 0x540001B3, 0x74000022, 0x540001B0, 0xB95001B5,
/*01BC*/ 0x74000189, 0xD0920000, 0x7DD00190, 0x28920000, 0xC15001BE, 0xEDD00198,
/*01C2*/ 0x540001A0, 0xF4000DE7, 0xD2E7DD46, 0xF4000000, 0xF1D001BC, 0x2BD00DE7,
/*01C8*/ 0xD27EC800, 0x74000138, 0x5400017D, 0x7C104A0C, 0x7C800000, 0x0526C800,
/*01CE*/ 0xE95001EF, 0x74000127, 0x74000156, 0xE95001CF, 0x74000158, 0x07D0000D,
/*01D4*/ 0x6E420000, 0xE95001D8, 0xECAEFCD0, 0x24308800, 0x07D00008, 0x6E420000,
/*01DA*/ 0xE95001E4, 0xEF9E5B20, 0xE95001E3, 0xD09D0800, 0x540001E1, 0x445B1B07,
/*01E0*/ 0x445B1B20, 0xF400077C, 0x39D00138, 0x540001EE, 0x07D00020, 0xD090C800,
/*01E6*/ 0xC15001ED, 0xEFD00DB1, 0xD3690800, 0xE95001EB, 0x05D00150, 0xF0F20000,
/*01EC*/ 0x540001EE, 0xEFB20000, 0x540001CD, 0x2BBF3427, 0x08800000, 0x9FB08800,
/*01F2*/ 0x7F4F40FF, 0x4D2F4003, 0x4C410420, 0x7400009C, 0xD0AF4003, 0xD139FB08,
/*01F8*/ 0xD0920000, 0x7F9DB920, 0x740001F2, 0x27C27C28, 0x270541F9, 0xEFBEC220,
/*01FE*/ 0xF4000000, 0xF4000D9C, 0xD0FEC220, 0xF4000001, 0xF4000D9C, 0xD0FEC220,
/*0204*/ 0xF4000D9C, 0xD3620000, 0xE9500209, 0xF4000DDF, 0xD0220000, 0xF4000DDB,
/*020A*/ 0xD0220000, 0x05FB8800, 0x740001F1, 0xF4000004, 0x29500007, 0xF4000DDF,
/*0210*/ 0xD150020B, 0xF4000DE3, 0xD150020B, 0xF4000DDB, 0xD017EE9C, 0xEFD00004,
/*0216*/ 0x29500007, 0xF4000D9C, 0xD3620000, 0xE950021B, 0x5400020F, 0x54000213,
/*021C*/ 0x05FB8800, 0x740001F2, 0x4AE24A9C, 0xEC220000, 0xF4000DDF, 0xD150021C,
/*0222*/ 0xF4000DE3, 0xD150021C, 0xF4000DDB, 0xD017EE3C, 0xEFD00001, 0x29500007,
/*0228*/ 0xF4000D9C, 0xD3620000, 0xE950022C, 0x54000220, 0x54000224, 0xE7D00DDB,
/*022E*/ 0xD2E6D308, 0xF4000DDF, 0xD2EF3D00, 0xD1D0020F, 0x06E90800, 0x7400022D,
/*0234*/ 0xE9500236, 0x85500232, 0x0417420F, 0x06E92420, 0x7400022D, 0xE950023C,
/*023A*/ 0x99D0020F, 0x54000237, 0x08800000, 0xF4000DC7, 0xD2EF4DBF, 0xD27EC800,
/*0240*/ 0xF4000D97, 0xD3D000CC, 0x74000092, 0xF4000000, 0xA4800000, 0x7400022F,
/*0246*/ 0x7F42520C, 0x514F39F0, 0x740001F1, 0x2BC90800, 0xE9500245, 0xEC220000,
/*024C*/ 0x54000251, 0x6C65480C, 0x57206F6C, 0x646C726F, 0xFFFFFF21, 0xF4000934,
/*0252*/ 0x39D00138, 0x74000152, 0xF400000A, 0xF4000000, 0xF3425F7C, 0x49D001B5,
/*0258*/ 0x74000051, 0x54000257, 0x08800000, 0x00000003, 0xFFFFF204, 0xFFFFF200,
/*025E*/ 0xFFFFF1F0, 0xFFFFF100, 0x00000004, 0xFFFFF218, 0x0000000A, 0x0000A4A0,
/*0264*/ 0x00000990, 0xFFFFF338, 0x00000006, 0xFFFFF22C, 0xFFFFF334, 0x00000518,
/*026A*/ 0x00000001, 0x0000000C, 0xFFFFF26C, 0x0000000C, 0x00000001, 0xFFFFF248,
/*0270*/ 0x001A0001, 0x00000001, 0xFFFFF250, 0x1A000964, 0x00000003, 0xFFFFF25C,
/*0276*/ 0x0000A464, 0x00050003, 0xFFFFF334, 0x00000001, 0xFFFFF334, 0x0000A47C,
/*027C*/ 0x00000000, 0xFFFFF338, 0xF4000DFF, 0xD01D0920, 0x74000092, 0xF400096C,
/*0282*/ 0x9B9E4910, 0x103F1FF0, 0x28120000, 0xE9500288, 0x7E629D70, 0x54000282,
/*0288*/ 0x2BD00D97, 0xD27EC800, 0xF4000DFF, 0xD3FF4EFF, 0xD37F4E13, 0xD2FF4D97,
/*028E*/ 0xD2E7C800, 0xF4010000, 0xF4000DDF, 0xD27EC800, 0xF4018000, 0xF4000DE3,
/*0294*/ 0xD27EC220, 0x7400027E, 0x7400024C, 0x54000109, 0x7400027E, 0x7400024C,
/*029A*/ 0x54000109};
//
uint32_t FetchROM(uint32_t addr) {
//
  if (addr < 667) {
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
    printf("FlashWrite to %X, you should be using SPI flash write (ROM! etc) instead\n", address);  // writing above ROM space
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

