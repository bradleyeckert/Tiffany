//===============================================================================
// config.h
//===============================================================================
#ifndef __CONFIG_H__
#define __CONFIG_H__

// Sizes of internal memories in 32-bit cells
#define RAMsize   0x400
#define ROMsize   0x800

#define TRACEABLE
#define TraceDepth 12           /* Log2 of the trace buffer size, 13*2^N bytes */

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

#if (ROMsize & (RAMsize-1)) // VM stack addressing depends on this
#error ROMsize must be an integer multiple of RAMsize
#endif

#if (ROMsize & 0x3FF) // To match SPI flash sectors
#error ROMsize must be a multiple of 1024 (0x400)
#endif

#endif
