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

// Instruments the VM to allow Undo and Redo
#define TRACEABLE
#define TraceDepth 12           /* Log2 of the trace buffer size, 13*2^N bytes */

// Tell TIF to copy ROM writes to SPI flash (not used yet)
//#define BootFromSPI             /* Defined if SPI gets a copy of the ROM image */

//#define MONOCHROME
// Console color scheme, comment out if no colors
#ifndef MONOCHROME
#define InterpretColor  "\033[1;33m"
#define ErrorColor      "\033[1;31m"
#define FilePathColor   "\033[1;34m"
#define FileLineColor   "\033[1;32m"
#endif

#define OKstyle  1     /* Style of OK prompt: 0=classic, 1=openboot, 2=depth */

#define CodePointerOrigin  0                  /* Kernel definitions start here */
#define DataPointerOrigin ((ROMsize+0x140)*4) /* Data starts after stack space */
#define HeadPointerOrigin ((ROMsize+RAMsize)*4)  /* Headers start in AXI space */

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
