//===============================================================================
// config.h
//===============================================================================
#ifndef __CONFIG_H__
#define __CONFIG_H__

// Sizes of internal memories in 32-bit cells
#define StackSpace 128                /* combined stack space at bottom of RAM */
#define RAMsize    0x200                      /* should be an exact power of 2 */
#define ROMsize    0x2000                     /* should be an exact power of 2 */

#define SPIflashBlocks  256                                 /* # of 4K sectors */
#define AXIRAMsize 0x100                                 /* RAM on the AXI bus */

// Instruments the VM to allow Undo and Redo
 #define TRACEABLE
#define TraceDepth 12           /* Log2 of the trace buffer size, 13*2^N bytes */

// number of rows in the CPU register dump, minimum 9, maximum 12
#define DumpRows         10
#define StartupTheme      0

#define OKstyle  1                   /* Style of OK prompt: 0=classic, 1=depth */
// #define VERBOSE     /* for debugging the quit loop, etc. */

// Number of lines in `locate`
#define LocateLines  10

// put headers in IROM
//#define HeadPointerOrigin  0x3000    /* leave 12K for code */
//#define HeadPointerOrigin  ((ROMsize+RAMsize)*4)    /* Lowest SPI code address */

//===============================================================================
// Sanity checks

#if (RAMsize & (RAMsize-1))
#error RAMsize must be a power of 2
#endif

#if (ROMsize & (ROMsize-1))
#error ROMsize must be a power of 2
#endif

#if (ROMsize & (RAMsize-1))
#error ROMsize must be a multiple of RAMsize
#endif

#endif
