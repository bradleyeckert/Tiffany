//===============================================================================
// config.h
//===============================================================================
#ifndef __CONFIG_H__
#define __CONFIG_H__

// Sizes of internal memories in 32-bit cells
#define RAMsize   0x400                         /* must be an exact power of 2 */
#define ROMsize   0x800                         /* must be an exact power of 2 */
#define AXIsize   0x4000             /* SPI flash, must be a multiple of 0x400 */

// Copy internal ROM writes to SPI flash
#define BootFromSPI

// Default file to load from
#define DefaultFile "tiff.f"

// Instruments the VM to allow Undo and Redo
#define TRACEABLE
#define TraceDepth 12           /* Log2 of the trace buffer size, 13*2^N bytes */

// Tell TIF to copy ROM writes to SPI flash (not used yet)
//#define BootFromSPI             /* Defined if SPI gets a copy of the ROM image */

// number of rows in the CPU register dump, minimum 9, maximum 12
#define DumpRows         11

//#define MONOCHROME
// Console color scheme, comment out if no colors.
#ifndef MONOCHROME
#define InterpretColor  "\033[1;33m"
#define ErrorColor      "\033[1;31m"
#define FilePathColor   "\033[1;34m"
#define FileLineColor   "\033[1;32m"
#define AnonAttribute   "\033[1;4m"
#define NoJumpAttribute "\033[1;34m"
#endif

#define OKstyle  2     /* Style of OK prompt: 0=classic, 1=openboot, 2=depth */
// #define VERBOSE     /* for debugging the quit loop, etc. */

// A word is reserved for a forward jump to cold boot, kernel starts at 000004.
// These are byte addresses.
#define CodePointerOrigin  4                  /* Kernel definitions start here */
#define HeadPointerMin    ((ROMsize+RAMsize)*4)     /* Lowest SPI code address */
#define HeadPointerOrigin  0x8000       /* Headers are in AXI space above code */

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
