//================================================================================
// vm.h
//================================================================================
#ifndef __VM_H__
#define __VM_H__
#include <stdint.h>
#include "config.h"

//================================================================================

// Defined in vm.c, the basic debug and simulation interface.
uint32_t VMstep(uint32_t IR, int Paused);   // Execute an instruction group
void VMpor(void);                           // Reset the VM
void SetDbgReg(uint32_t n);                 // write to the debug mailbox
uint32_t GetDbgReg(void);                   // read from the debug mailbox
uint32_t vmRegRead(int ID);                 // quick read of VM register
uint32_t FetchCell(uint32_t addr);
uint16_t FetchHalf(uint32_t addr);
uint8_t FetchByte(uint32_t addr);
void StoreCell (uint32_t x, uint32_t addr);
void StoreHalf (uint16_t x, uint32_t addr);
void StoreByte (uint8_t x, uint32_t addr);

// Different host and target behaviors:
// Host: Writes to ROM image, looking for non-blank violations.
//     For example, to clear bit 5 in a word you would write 0xFFFFFFDF.
// Target: Triggers an error interrupt if writing to bad address space.
//     0 to ROMsize-1 is unwritable.
int WriteROM(uint32_t data, uint32_t address);

// Defined in vm.c, used for development only. Not on the target system.
void Trace(unsigned int Type, int32_t ID, uint32_t Old, uint32_t New);
void UnTrace(int32_t ID, uint32_t old);
extern int tiffIOR;                         // error detected when not 0
extern uint32_t cyclecount;                 // elapsed clock cycles in hardware
extern uint32_t maxRPtime;                  // max cycles between RP! occurrences
extern uint32_t ProfileCounts[ROMsize];     // profiler data
extern uint32_t OpCounter[64];              // dynamic instruction count

// used by flash.c to simulate SPI flash
extern uint32_t ROM[(SPIflashBlocks<<10)];

// Defined in vmUser.c, used by the debugger in accessvm.c.
uint32_t UserFunction (uint32_t T, uint32_t N, int fn );

//================================================================================

#define opNOP        (000)
#define opDUP        (001)  // dup
#define opEXIT       (002)  // exit
#define opADD        (003)  // +
#define opUSER       (004)  // user
#define opZeroLess   (005)  // 0<
#define opPOP        (006)  // r>
#define opTwoDiv     (007)  // 2/

#define opSKIPNC     (010)  // ifc:  slot=end if no carry
#define opOnePlus    (011)  // 1+
#define opSWAP       (012)  // swap
#define opSUB        (013)  // -
#define opCstorePlus (015)  // c!+  ( c a -- a+1 )
#define opCfetchPlus (016)  // c@+  ( a -- a+1 c )
#define opUtwoDiv    (017)  // u2/

#define opSKIP       (020)  // no:  skip remaining slots
#define opTwoPlus    (021)  // 2+
#define opSKIPNZ     (022)  // ifz:
#define opJUMP       (023)  // jmp
#define opWstorePlus (025)  // w!+  ( n a -- a+2 )
#define opWfetchPlus (026)  // w@+  ( a -- a+2 n )
#define opAND        (027)  // and

#define opLitX       (031)  // litx
#define opPUSH       (032)  // >r
#define opCALL       (033)  // call
#define opZeroEquals (035)  // 0=
#define opWfetch     (036)  // w@  ( a -- n )
#define opXOR        (037)  // xor

#define opREPT       (040)  // rept  slot=0
#define opFourPlus   (041)  // 4+
#define opOVER       (042)  // over
#define opADDC       (043)  // c+  with carry in
#define opStorePlus  (045)  // !+  ( n a -- a+4 )
#define opFetchPlus  (046)  // @+  ( a -- a+4 n )
#define opTwoStar    (047)  // 2*

#define opMiREPT     (050)  // -rept  slot=0 if T < 0
#define opRP         (052)  // rp
#define opDROP       (053)  // drop
#define opSetRP      (055)  // rp!
#define opFetch      (056)  // @
#define opTwoStarC   (057)  // 2*c

#define opSKIPGE     (060)  // -if:  slot=end if T >= 0
#define opSP         (062)  // sp
#define opFetchAS    (063)  // @as
#define opSetSP      (065)  // sp!
#define opCfetch     (066)  // c@
#define opPORT       (067)  // port  ( n -- m ) swap T with port

#define opLIT        (071)  // lit
#define opUP         (072)  // up
#define opStoreAS    (073)  // !as
#define opSetUP      (075)  // up!
#define opRfetch     (076)  // r@
#define opCOM        (077)  // com

#endif

