//===============================================================================
// config.h
//===============================================================================
#ifndef __CONFIG_H__
#define __CONFIG_H__

// Sizes of internal memories in 32-bit cells
#define RAMsize   0x400                         /* must be an exact power of 2 */
#define ROMsize   0x800                         /* must be an exact power of 2 */
#define AXIsize   0x4000             /* SPI flash, must be a multiple of 0x400 */

// Instruments the VM to allow Undo and Redo
#define TRACEABLE
#define TraceDepth 12           /* Log2 of the trace buffer size, 13*2^N bytes */

// Tell TIF to copy ROM writes to SPI flash (not used yet)
//#define BootFromSPI             /* Defined if SPI gets a copy of the ROM image */

// Console color scheme, comment out if no colors
#define InterpretColor  "\033[1;33m"
#define ErrorColor      "\033[1;31m"
#define FilePathColor   "\033[1;34m"
#define FileLineColor   "\033[1;32m"

#define CodePointerOrigin  0                  /* Kernel definitions start here */
#define HeadPointerOrigin  (ROMsize*2)       /* Headers start halfway into ROM */
#define DataPointerOrigin ((ROMsize+0x140)*4) /* Data starts after stack space */

// Wordlists can be hashed into a number of different threads
#define WordlistStrands  3                           /* must be a prime number */

//===============================================================================
// Sanity checks

#if (RAMsize & (RAMsize-1))
#error RAMsize must be a power of 2
#endif

#if (ROMsize & (ROMsize-1))
#error ROMsize must be a power of 2
#endif

#if (AXIsize & 0x3FF) // To match SPI flash sectors
#error AXIsize must be a multiple of 1024 (0x400)
#endif

#endif
