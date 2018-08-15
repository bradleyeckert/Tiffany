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
#define termTCB(n)  ((ROMsize + 256 + (n))*4)      /* Terminal USER variables */

// error message handling
char ErrorString[260];   // String to include in error message
void ErrorMessage (int error);
extern int printed;

// forward references for tiff.c
uint32_t tiffPARSENAME (void);                             // ( -- addr length )
uint32_t tiffPARSE (void);
void initFilelist(void);

// used by main.c, defined in tiff.c
// might move to vmaccess.c later, with memory and stack access functions.
uint32_t vmTEST(void);
extern char *DefaultFile;

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
// 8-bit variables are used here to save RAM
#define WIDS        termTCB(16)+0  /* number of WID entries in context stack  */
#define CALLED      termTCB(16)+1  /* set if last explicit opcode was a call  */
#define SLOT        termTCB(16)+2  /* current slot position, these are a pair */
#define LITPEND     termTCB(16)+3  /* literal-pending flag                    */
#define COLONDEF    termTCB(17)+0  /* colon definition is in progress         */
// Compiler internal state
#define CALLADDR    termTCB(18)  /* address+slot of last compiled CALL        */
#define NEXTLIT     termTCB(19)  /* Next literal to be compiled               */
#define IRACC       termTCB(20)  /* IR accumulator                            */
#define HEAD        termTCB(21)  /* Points to header of last found word       */
#define CONTEXT     termTCB(22)  /* 8 cells of context                        */
#define FORTHWID    termTCB(30)  /* Forth wordlist                            */
#define HLD         termTCB(31)  /* Numeric input pointer                     */
#define TIB         termTCB(32)  /* Terminal Input Buffer                     */
// support 132-column text files plus a little extra in case of zero-terminator
#define MaxTIBsize  136          /* Maximum bytes allowed in TIB              */
#define PADsize     64
#define PAD         (TIB + MaxTIBsize)

#define DataPointerOrigin (TIB + MaxTIBsize + PADsize) /* Data starts after TIB */

#if (MaxTIBsize & 3)
#error Please make MaxTIBsize a multiple of 4
#endif

//==============================================================================

struct FileRec {
    char Line[MaxTIBsize+1];
    char FilePath[MaxTIBsize+1];
    FILE *fp;
    uint32_t LineNumber;
    int FID;
};

#endif
