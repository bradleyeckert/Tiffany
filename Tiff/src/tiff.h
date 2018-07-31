//==============================================================================
// tick.h: Header file for tick.c
//==============================================================================
#ifndef __TICK_H__
#define __TICK_H__
#include <stdint.h>
#include "config.h"             /* ROMsize */

/* Cell addresses (Byte addressing multiplies by 4)                           */
/* 0000000 to ROMsize-1             Internal ROM                              */
/* ROMsize to (ROMsize+RAMsize-1)   Internal RAM                              */
/* The VM restricts the stacks to the bottom 1K bytes of vmRAM.               */

#define TiffRP0     ((ROMsize + 255)*4)             /* Initial stack pointers */
#define TiffSP0     ((ROMsize + 240)*4)                    /* Byte addressing */
#define termTCB(n)  ((ROMsize + 256 + n)*4)        /* Terminal USER variables */

// error message handling
char ErrorString[64];   // String to include in error message
void ErrorMessage (int error);

// used by main.c, defined in tiff.c
// might move to vmaccess.c later, with memory and stack access functions.
void vmTEST(void);

//==============================================================================
// Definitions of terminal variables addresses.
// Only one terminal task is supported, but it's in a cooperative task loop.

/* Common to all tasks.                                                       */
#define STATUS      termTCB(0)   /* -> multitasker status function            */
#define FOLLOWER    termTCB(1)   /* -> next header in task list               */
#define SP0         termTCB(2)   /* initial stack pointer                     */
#define RP0         termTCB(3)   /* initial return stack pointer              */
#define HANDLER     termTCB(4)   /* exception stack frame pointer             */

/* Terminal task only, located at a fixed location in RAM.                    */
#define BASE        termTCB(5)   /* numeric conversion base                   */
#define HP          termTCB(6)   /* head space pointer                        */
#define CP          termTCB(7)   /* code space pointer                        */
#define DP          termTCB(8)   /* data space pointer                        */
#define TIBS        termTCB(9)   /* number of chars in TIB                    */
#define TIBB        termTCB(10)  /* pointer to tib (paired with TIBS)         */
#define TOIN        termTCB(11)  /* offset into TIB                           */
#define SOURCEID    termTCB(12)  /* input source, 0(keybd) +(file) -(blk)     */
#define STATE       termTCB(13)  /* compiler state                            */
#define HEAD        termTCB(14)  /* current definition's header address       */
#define CURRENT     termTCB(15)  /* the wid which is compiled into            */
#define PERSONALITY termTCB(16)  /* address of personality array              */
#define BLK         termTCB(17)  /* block number for LOAD                     */
#define WIDS        termTCB(18)  /* number of WID entries in context stack    */
#define CONTEXT     termTCB(19)  /* 8 cells of context                        */
#define TIB         termTCB(28)  /* Terminal Input Buffer                     */
#define MaxTIBsize  (4*(64-28))  /* Maximum bytes allowed in TIB              */

//==============================================================================
#endif
