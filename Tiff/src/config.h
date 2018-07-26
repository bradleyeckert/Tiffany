//===============================================================================
// config.h
//===============================================================================
#ifndef __CONFIG_H__
#define __CONFIG_H__

// Sizes of internal memories in 32-bit cells
#define RAMsize 1024
#define ROMsize 4096

#define CodePointerOrigin  0                  /* Kernel definitions start here */
#define HeadPointerOrigin  (ROMsize*2)       /* Headers start halfway into ROM */
#define DataPointerOrigin ((ROMsize+0x140)*4) /* Data starts after stack space */

// Wordlists can be hashed into a number of different threads
#define WordlistStrands  3                           /* must be a prime number */

// Sanity check
#if (ROMsize & 0xFF)
#error ROMsize must be a multiple of 0x100
#endif

#endif
