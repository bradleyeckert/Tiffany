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

// Return stack has a generous 32 cells.
// With CATCH using 4 cells in the terminal, 16 doesn't do it.

#define TiffRP0     ((ROMsize + 255)*4)             /* Initial stack pointers */
#define TiffSP0     ((ROMsize + 223)*4)                    /* Byte addressing */
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

// substitute for strlwr
void UnCase(char *s);

//==============================================================================
// Definitions of terminal variables addresses.
// Only one terminal task is supported, but it's in a cooperative task loop.

/* Common to all tasks.                                                       */
#define STATUS      termTCB(0)   /* -> multitasker status function            */
#define FOLLOWER    termTCB(1)   /* -> next header in task list               */
#define RP0         termTCB(2)   /* initial return stack pointer              */
#define SP0         termTCB(3)   /* initial data stack pointer                */
#define TOS         termTCB(4)   /* multitasker state-of-stacks               */
/* Terminal task only, located at a fixed location in RAM.                    */
#define HANDLER     termTCB(5)   /* exception stack frame pointer             */
#define BASE        termTCB(6)   /* numeric conversion base                   */
#define HP          termTCB(7)   /* head space pointer                        */
#define CP          termTCB(8)   /* code space pointer                        */
#define DP          termTCB(9)   /* data space pointer                        */
#define STATE       termTCB(10)  /* compiler state                            */
#define CURRENT     termTCB(11)  /* the wid which is compiled into            */
#define SOURCEID    termTCB(12)  /* input source, 0(keybd) +(file) -(blk)     */
#define PERSONALITY termTCB(13)  /* address of personality array              */
#define TIBS        termTCB(14)  /* number of chars in TIB                    */
#define TIBB        termTCB(15)  /* pointer to tib (paired with TIBS)         */
#define TOIN        termTCB(16)  /* offset into TIB                           */
// 8-bit variables are used here to save RAM
#define WIDS        termTCB(17)+0  /* number of WID entries in context stack  */
#define CALLED      termTCB(17)+1  /* set if last explicit opcode was a call  */
#define SLOT        termTCB(17)+2  /* current slot position, these are a pair */
#define LITPEND     termTCB(17)+3  /* literal-pending flag                    */
#define COLONDEF    termTCB(18)+0  /* colon definition is in progress         */
#define CASEINSENS  termTCB(18)+1  /* case-insensitive flag                   */
#define TEMPTOIN    termTCB(18)+2  /* save 16-bit >IN in case of error        */
// Compiler internal state
#define CALLADDR    termTCB(19)  /* address+slot of last compiled CALL        */
#define NEXTLIT     termTCB(20)  /* Next literal to be compiled               */
#define IRACC       termTCB(21)  /* IR accumulator                            */
#define HEAD        termTCB(22)  /* Points to header of last found word       */
#define CONTEXT     termTCB(23)  /* 8 cells of context                        */
#define FORTHWID    termTCB(31)  /* Forth wordlist                            */
#define HLD         termTCB(32)  /* Numeric input pointer                     */
#define TIB         termTCB(33)  /* Terminal Input Buffer                     */
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
