//================================================================================
// vm.h
//================================================================================
#ifndef __VM_H__
#define __VM_H__
#include <stdint.h>

//================================================================================

// Defined in vm.c, the basic debug and simulation interface.
uint32_t VMstep(uint32_t IR, int Paused);   // Execute an instruction group
void VMpor(void);                           // Reset the VM
void SetDbgReg(uint32_t n);                 // write to the debug mailbox
uint32_t GetDbgReg(void);                   // read from the debug mailbox

// Defined in vm.c, used for development only. Not on the target system.
int WriteROM(uint32_t data, uint32_t address);
int EraseAXI4K(uint32_t address);         // Erase a 4KB sector in SPI flash image
void Trace(unsigned int Type, int32_t ID, uint32_t Old, uint32_t New);
void UnTrace(int32_t ID, uint32_t old);
extern int tiffIOR;                           // error detection, error when not 0
extern unsigned long cyclecount;

// Defined in tiffUser.c:
int tiffKEYQ (void);                                      // Check for a key press
int tiffEKEY (void);                                  // Raw console keyboard keys
void tiffEMIT (uint8_t c);                               // Output char to console
uint32_t UserFunction (uint32_t T, uint32_t N, int fn );

// Defined in tiff.c:
void tiffQUIT(char *s);                                    // The C-side QUIT loop

//================================================================================

#define opNOP        (000)
#define opDUP        (001)  // dup
#define opEXIT       (002)  // exit
#define opADD        (003)  // +
#define opUSER       (004)  // user
#define opDROP       (005)  // drop
#define opPOP        (006)  // r>
#define opTwoDiv     (007)  // 2/

#define opOnePlus    (011)  // 1+
#define opPUSH       (012)  // >r
#define opSUB        (013)  // -
#define opCstorePlus (015)  // c!+  ( c a -- a+1 )
#define opCfetchPlus (016)  // c@+  ( a -- a+1 c )
#define opUtwoDiv    (017)  // u2/

#define opSKIP       (020)  // no:  skip remaining slots
#define opTwoPlus    (021)  // 2+
#define opOVER       (022)  // over
#define opJUMP       (023)  // jmp
#define opWstorePlus (025)  // w!+  ( n a -- a+2 )
#define opWfetchPlus (026)  // w@+  ( a -- a+2 n )
#define opAND        (027)  // and

#define opLitX       (031)  // litx
#define opSWAP       (032)  // swap
#define opCALL       (033)  // call
#define opZeroEquals (035)  // 0=
#define opWfetch     (036)  // w@  ( a -- n )
#define opXOR        (037)  // xor

#define opREPT       (040)  // rept  slot=0
#define opFourPlus   (041)  // 4+
#define opMiBran     (042)  // -bran
#define opADDC       (043)  // +  with carry in
#define opStorePlus  (045)  // !+  ( n a -- a+4 )
#define opFetchPlus  (046)  // @+  ( a -- a+4 n )
#define opTwoStar    (047)  // 2*

#define opMiREPT     (050)  // -rept  slot=0 if T < 0
#define opRP         (052)  // rp
#define opSUBC       (053)  // -c  subtract with borrow
#define opSetRP      (055)  // rp!
#define opFetch      (056)  // @
#define opDtwoStar   (057)  // d2*

#define opSKIPGE     (060)  // -if:  slot=end if T >= 0
#define opSP         (062)  // sp
#define opFetchAS    (063)  // @as
#define opSetSP      (065)  // sp!
#define opCfetch     (066)  // c@
#define opPORT       (067)  // port  ( n -- m ) swap T with port

#define opSKIPLT     (070)  // +if:  slot=end if T < 0
#define opLIT        (071)  // lit
#define opUP         (072)  // up
#define opStoreAS    (073)  // !as
#define opSetUP      (075)  // up!
#define opRfetch     (076)  // r@
#define opCOM        (077)  // com

#endif

