//===============================================================================
// config.h
//===============================================================================
#ifndef __CONFIG_H__
#define __CONFIG_H__

// The AXIRAMsize is not used, feature not implemented.
// The idea was to have RAM in the AXI space, after SPI flash.
#define AXIRAMsize 0                                     /* RAM on the AXI bus */
#define  MaxFlashCells  (SPIflashBlocks<<10)

#endif
