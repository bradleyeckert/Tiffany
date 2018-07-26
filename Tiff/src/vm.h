//================================================================================
// tick.h: Header file for tick.c
// This is the VM and user variable address definition
//================================================================================
#ifndef __VM_H__
#define __VM_H__
#include <stdint.h>
#include "config.h"

//================================================================================

// Defined in vm.c:
int32_t VMstep(uint32_t IR, int RunState);  // Execute an instruction group
uint32_t DebugReg;                               // debugger mailbox for VM

// Defined in tiffUser.c:
int tiffKEYQ (void);                                      // Check for a key press
int tiffEKEY (void);                                  // Raw console keyboard keys
void tiffEMIT (uint8_t c);                               // Output char to console
uint32_t UserFunction (uint32_t T, uint32_t N, int fn );

// Defined in tiff.c:
void tiffQUIT(char *s);                                    // The C-side QUIT loop

//================================================================================
#define opNOOP       (000)
#define opDUP        (001)
#define opEXIT       (002)
#define opADD        (003)
#define opSKIP       (004)
#define opGETR       (005)
#define opEXITE      (006)
#define opAND        (007)
#define opSKIPNZ     (010)
#define opOVER       (011)
#define opPOP        (012)
#define opXOR        (013)
#define opSKIPZ      (014)
#define opA          (015)
#define opRDROP      (016)
#define opSKIPMI     (020)
#define opStoreAS    (021)
#define opFetchA     (022)
#define opSKIPGE     (024)
#define opTwoStar    (025)
#define opFetchAplus (026)
#define opNEXT       (030)
#define opRSHIFT1    (031)
#define opWfetchA    (032)
#define opSetA       (033)
#define opREPT       (034)
#define opTwoDiv     (035)
#define opCfetchA    (036)
#define opSetB       (037)

#define opSP         (040)
#define opCOM        (041)
#define opStoreA     (042)
#define opSetRP      (043)
#define opRP         (044)
#define opPORT       (045)
#define opStoreBplus (046)
#define opSetSP      (047)
#define opUP         (050)
#define opWstoreA    (052)
#define opSetUP      (053)
#define opLITplus    (054)
#define opCstoreA    (056)
#define opUSER       (060)
#define opNIP        (063)
#define opJUMP       (064)
#define opFetchAS    (066)
#define opLIT        (067)
#define opSWAP       (070)
#define opDROP       (072)
#define opROT        (073)
#define opCALL       (074)
#define opOnePlus    (075)
#define opPUSH       (076)

#endif
