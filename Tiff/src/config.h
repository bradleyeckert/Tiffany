//===============================================================================
// config.h
//===============================================================================
#ifndef __CONFIG_H__
#define __CONFIG_H__

// Sizes of internal memories in 32-bit cells
#define StackSpace 128                 /* combined stack space at bottom of RAM */
#define RAMsize    0x200                       /* should be an exact power of 2 */
#define ROMsize    0x2000                      /* should be an exact power of 2 */

#define SPIflashCapacity   18    /* Log2 of flash size in bytes, minimum is 12 */
#define SPIflashSize (1<<(SPIflashCapacity-2))  /* Must be a multiple of 0x400 */
#define AXIRAMsize 0x100                                 /* RAM on the AXI bus */

// Instruments the VM to allow Undo and Redo
 #define TRACEABLE
#define TraceDepth 12           /* Log2 of the trace buffer size, 13*2^N bytes */

// number of rows in the CPU register dump, minimum 9, maximum 12
#define DumpRows         10
#define StartupTheme      0

#define OKstyle  2     /* Style of OK prompt: 0=classic, 1=openboot, 2=depth */
// #define VERBOSE     /* for debugging the quit loop, etc. */

// Number of lines in `locate`
#define LocateLines  10

// put headers in IROM
#define HeadPointerOrigin  ((ROMsize-4096)*4)    /* Lowest SPI code address */
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

#if (SPIflashSize & 0x3FF) // To match SPI flash sectors
#error SPIflashSize must be a multiple of 1024 (0x400)
#endif

#endif
