//===============================================================================
// config.h
//===============================================================================
#ifndef __CONFIG_H__
#define __CONFIG_H__

// Sizes of internal memories in 32-bit cells
#define StackSpaceDefault 128         /* combined stack space at bottom of RAM */
#define RAMsizeDefault  0x400                 /* should be an exact power of 2 */
#define ROMsizeDefault  0x2000                /* should be an exact power of 2 */
#define FlashBlksDefault  256                               /* # of 4K sectors */

// These use malloc, so can be arbitrarily large.
#define MaxROMsize      0x40000                   /* 4M bytes maximum ROM size */
#define MaxRAMsize      0x40000                   /* 4M bytes maximum RAM size */
#define MaxFlashCells   0x80000                 /* 8M bytes maximum Flash size */

// Instruments the VM to allow Undo and Redo
#define TRACEABLE
#define TraceDepth 12           /* Log2 of the trace buffer size, 13*2^N bytes */

// number of rows in the CPU register dump, minimum 9, maximum 12
#define DumpRows           10
#define StartupTheme        0                 /* 0=monochrome, 1=color (VT220) */

#define OKstyle  1                   /* Style of OK prompt: 0=classic, 1=depth */
// #define VERBOSE     /* for debugging the quit loop, etc. */

// Number of lines in `locate`
#define LocateLines  10

#endif
