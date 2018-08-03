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
char ErrorString[260];   // String to include in error message
void ErrorMessage (int error);
extern int printed;

// forward references for tiff.c
uint32_t tiffPARSENAME (void);                             // ( -- addr length )
void initFilelist(void);

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
#define STATE       termTCB(9)   /* compiler state                            */
#define CURRENT     termTCB(10)  /* the wid which is compiled into            */
#define SOURCEID    termTCB(11)  /* input source, 0(keybd) +(file) -(blk)     */
#define PERSONALITY termTCB(12)  /* address of personality array              */
#define TIBS        termTCB(13)  /* number of chars in TIB                    */
#define TIBB        termTCB(14)  /* pointer to tib (paired with TIBS)         */
#define TOIN        termTCB(15)  /* offset into TIB                           */
#define CALLADDR    termTCB(16)  /* address+slot of last compiled CALL        */
#define NEXTLIT     termTCB(17)  /* Next literal to be compiled               */
#define WIDS        termTCB(18)  /* number of WID entries in context stack    */
#define CALLED      termTCB(18)+1  /* set if last explicit opcode was a call  */
#define SLOT        termTCB(18)+2  /* current slot position, these are a pair */
#define LITPEND     termTCB(18)+3  /* literal-pending flag                    */
#define IRACC       termTCB(19)  /* IR accumulator                            */
#define CONTEXT     termTCB(20)  /* 8 cells of context                        */
#define FORTHWID    termTCB(28)  /* Forth wordlist                            */
#define TIB         termTCB(29)  /* Terminal Input Buffer                     */
#define MaxTIBsize  (4*(64-29))  /* Maximum bytes allowed in TIB              */

//==============================================================================

struct FileRec {
    char Line[MaxTIBsize+1];
    char FilePath[MaxTIBsize+1];
    FILE *fp;
    uint32_t LineNumber;
    int FID;
};

#endif
