//===============================================================================
// config.h
//===============================================================================
#ifndef __CONFIG_H__
#define __CONFIG_H__

// Sizes of internal memories in 32-bit cells
#define SPIflashCapacity   18    /* Log2 of flash size in bytes, minimum is 12 */
#define SPIflashSize (1<<(SPIflashCapacity-2))  /* Must be a multiple of 0x400 */
// The AXIRAMsize is not used, feature not implemented.
// The idea was to have RAM in the AXI space, after SPI flash.
#define AXIRAMsize 0                                     /* RAM on the AXI bus */

#endif
