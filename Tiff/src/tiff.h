//==============================================================================
// tiff.h: Header file for tiff.c
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

#define TiffRP0     ((ROMsize + StackSpace-4)*4)    /* Initial stack pointers */
#define TiffSP0     ((ROMsize + StackSpace/2)*4)           /* Byte addressing */
#define termTCB(n)  ((ROMsize + StackSpace + (n))*4)    /* Terminal USER vars */

// error message handling
void ErrorMessage (int error, char *s);
extern int printed;

// forward references for tiff.c
uint32_t tiffPARSENAME (void);                             // ( -- addr length )
uint32_t tiffPARSE (void);
void initFilelist(void);
int Refill(void);

// used by main.c, defined in tiff.c
// might move to accessvm.c later, with memory and stack access functions.
uint32_t vmTEST(void);
extern char *DefaultFile;

// reference to TiffUser function when Linux is used for I/O
void CookedMode(void);

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
#define PERSONALITY termTCB(12)  /* address of personality array              */
// The following are assumed to be a block for SAVE-INPUT
#define SOURCEID    termTCB(13)  /* input source, 0(keybd) +(file) -(blk)     */
#define TIBS        termTCB(14)  /* number of chars in TIB                    */
#define TIBB        termTCB(15)  /* pointer to tib (paired with TIBS)         */
#define TOIN        termTCB(16)  /* offset into TIB                           */
#define BLK         termTCB(17)  /* Current block, 0 if none                  */
// 8-bit variables are used here to save RAM
#define WIDS        termTCB(18)+0  /* number of WID entries in context stack  */
#define CALLED      termTCB(18)+1  /* set if last explicit opcode was a call  */
#define SLOT        termTCB(18)+2  /* current slot position, these are a pair */
#define LITPEND     termTCB(18)+3  /* literal-pending flag                    */
#define COLONDEF    termTCB(19)+0  /* colon definition is in progress         */
#define CASESENS    termTCB(19)+1  /* case-sensitive flag                     */
#define TEMPTOIN    termTCB(19)+2  /* save 16-bit >IN in case of error        */
// Compiler internal state
#define CALLADDR    termTCB(20)  /* address+slot of last compiled CALL        */
#define NEXTLIT     termTCB(21)  /* Next literal to be compiled               */
#define IRACC       termTCB(22)  /* IR accumulator                            */
#define HEAD        termTCB(23)  /* Points to header of last found word       */
#define LINENUMBER  termTCB(24)  /* 16-bit line number, if used.              */
#define FILEID      termTCB(24)+2  /* 8-bit file ID, if used.                 */
#define SCOPE       termTCB(24)+3  /* 8-bit SCOPE flag {dp, cp, hp}           */
#define CONTEXT     termTCB(25)  /* 8 cells of context                        */
#define FORTHWID    termTCB(33)  /* Forth wordlist                            */
#define HLD         termTCB(34)  /* Numeric input pointer                     */
#define TIB         termTCB(35)  /* Terminal Input Buffer                     */
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
