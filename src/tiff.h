//==============================================================================
// tiff.h: Header file for tiff.c
//==============================================================================
#ifndef __TIFF_H__
#define __TIFF_H__
#include <stdint.h>
#include "config.h"

/* Cell addresses (Byte addressing multiplies by 4)                           */
/* 0000000 to ROMsize-1             Internal ROM                              */
/* ROMsize to (ROMsize+RAMsize-1)   Internal RAM                              */

#define TiffRP0     ((-RAMsize + StackSpace-4)*4)   /* Initial stack pointers */
#define TiffSP0     ((-RAMsize + StackSpace/2)*4)          /* Byte addressing */
//#define termTCB(n)  ((ROMsize + StackSpace + (n))*4)   /* Terminal USER vars */
#define termTCB(n)  ((-RAMsize + StackSpace + (n))*4)   /* Terminal USER vars */

extern int StackSpace;

void ErrorMessage (int error, char *s);
void initFilelist(void);

// used by main.c, defined in tiff.c
uint32_t vmTEST(void);
void tiffQUIT (char *cmdline);
void iword_COLD(void);
extern char *DefaultFile;
extern int HeadPointerOrigin;

// reference to TiffUser function when Linux is used for I/O
void CookedMode(void);

// substitute for strlwr, converts string to uppercase
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
#define WIDS        termTCB(18)  /* number of WID entries in context stack    */
#define CALLED      (WIDS + 1)   /* set if last explicit opcode was a call    */
#define SLOT        (WIDS + 2)   /* current slot position, these are a pair   */
#define LITPEND     (WIDS + 3)   /* literal-pending flag                      */
#define COLONDEF    termTCB(19)  /* colon definition is in progress           */
#define CASESENS    (COLONDEF+1) /* case-sensitive flag                       */
#define NOECHO      (COLONDEF+2) /* inhibit ACCEPT echoing                    */
#define THEME       (COLONDEF+3) /* color scheme for VT220, 0=mono            */
// Compiler internal state
#define CALLADDR    termTCB(20)  /* address+slot of last compiled CALL        */
#define NEXTLIT     termTCB(21)  /* Next literal to be compiled               */
#define IRACC       termTCB(22)  /* IR accumulator                            */
#define HEAD        termTCB(23)  /* Points to header of last found word       */
#define LINENUMBER  termTCB(24)  /* 16-bit line number, if used.              */
#define FILEID      (LINENUMBER+2)  /* 8-bit file ID, if used.                */
#define SCOPE       (LINENUMBER+3)  /* 8-bit SCOPE flag {dp, cp, hp}          */
#define CONTEXT     termTCB(25)  /* 8 cells of context                        */
#define HLD         termTCB(26)  /* Numeric input pointer                     */
#define TIB         termTCB(27)  /* Terminal Input Buffer                     */
// support 132-column text files plus a little extra in case of zero-terminator
#define MaxTIBsize  136          /* Maximum bytes allowed in TIB              */
#define PADsize     64
#define PAD         (TIB + MaxTIBsize)
#define FORTHWID    (PAD + PADsize)       /* Forth wordlist                   */
#define DataPointerOrigin (FORTHWID + 4)  /* Data starts after TIB            */

// The first free cell in data space is right after the main wordlist WID.
// If your app uses more wordlists, you might put their WIDs here so as to keep
// all WIDs grouped together. This would support MARKER.

#if ((MaxTIBsize & 3) || (PADsize & 3))
#error Please make MaxTIBsize and PADsize a multiple of 4
#endif

//==============================================================================

struct FileRec { // for Tiff "include"
    char Line[MaxTIBsize+1];
    char FilePath[MaxTIBsize+1];
    FILE *fp;
    uint32_t LineNumber;
    int FID;
};

#endif
