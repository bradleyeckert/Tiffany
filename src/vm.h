//================================================================================
// vm.h
//================================================================================
#ifndef __VM_H__
#define __VM_H__
#include <stdint.h>
#include "config.h"

//================================================================================

// Defined in vm.c, the basic debug and simulation interface.
void vmMEMinit(char * name);                // Clear all memory
void ROMbye(void);                          // free memory
uint32_t VMstep(uint32_t IR, int Paused);   // Execute an instruction group
void VMpor(void);                           // Reset the VM
void SetDbgReg(uint32_t n);                 // write to the debug mailbox
uint32_t GetDbgReg(void);                   // read from the debug mailbox
uint32_t vmRegRead(int ID);                 // quick read of VM register
uint32_t FetchCell(int32_t addr);
uint16_t FetchHalf(int32_t addr);
uint8_t  FetchByte(int32_t addr);
void StoreCell (uint32_t x, int32_t addr);
void StoreHalf (uint16_t x, int32_t addr);
void StoreByte (uint8_t x,  int32_t addr);

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
extern uint32_t ROMsize;
extern uint32_t RAMsize;
extern uint32_t SPIflashBlocks;
extern uint32_t cyclecount;                 // elapsed clock cycles in hardware
extern uint32_t maxRPtime;                  // max cycles between RP! occurrences
extern uint32_t * ProfileCounts;            // profiler data
extern uint32_t OpCounter[64];              // dynamic instruction count

//================================================================================

#define opNOP        (000)  // nop
#define opDUP        (001)  // dup
#define opEXIT       (002)  // exit
#define opADD        (003)  // +
#define opTwoStar    (004)  // 2*

#define opSKIP       (010)  // no:  skip remaining slots
#define opOnePlus    (011)  // 1+
#define opPOP        (012)  // r>
#define opTwoStarC   (014)  // 2*c
#define opUSER       (015)  // user
#define opCfetchPlus (016)  // c@+  ( a -- a+1 c )
#define opCstorePlus (017)  // c!+  ( c a -- a+1 )

#define opRP         (021)  // rp
#define opRfetch     (022)  // r@
#define opAND        (023)  // and
#define opTwoDiv     (024)  // 2/
#define opJUMP       (025)  // jmp
#define opWfetchPlus (026)  // w@+  ( a -- a+2 n )
#define opWstorePlus (027)  // w!+  ( n a -- a+2 )

#define opSP         (031)  // sp
#define opXOR        (033)  // xor
#define opUtwoDiv    (034)  // u2/
#define opCALL       (035)  // call
#define opWfetch     (036)  // w@  ( a -- n )
#define opPUSH       (037)  // >r

#define opREPTC      (040)  // rept  slot=0 if C=0
#define opFourPlus   (041)  // 4+
#define opADDC       (043)  // c+  with carry in
#define opZeroEquals (044)  // 0=
#define opLitX       (045)  // litx
#define opFetchPlus  (046)  // @+  ( a -- a+4 n )
#define opStorePlus  (047)  // !+  ( n a -- a+4 )

#define opMiREPT     (050)  // -rept  slot=0 if T < 0
#define opUP         (051)  // up
#define opZeroLess   (054)  // 0<
#define opFetch      (056)  // @
#define opSetRP      (057)  // rp!

#define opSKIPGE     (060)  // -if:  slot=end if T >= 0
#define opPORT       (061)  // port  ( n -- m ) swap T with port
#define opCOM        (064)  // invert
#define opHost       (065)  // !as
#define opCfetch     (066)  // c@
#define opSetSP      (067)  // sp!

#define opSKIPNC     (070)  // ifc:  slot=end if no carry
#define opOVER       (071)  // over
#define opSKIPNZ     (072)  // ifz:
#define opDROP       (073)  // drop
#define opSWAP       (074)  // swap
#define opLIT        (075)  // lit
#define opSetUP      (077)  // up!

#endif

