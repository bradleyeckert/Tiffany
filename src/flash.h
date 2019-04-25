//==============================================================================
// flash.h
//==============================================================================
#ifndef __FLASH_H__
#define __FLASH_H__

void FlashInit (int UseFile);
void FlashBye  (int UseFile);
uint32_t FlashRead (uint32_t addr);
int FlashWrite (uint32_t x, uint32_t addr);
uint32_t SPIflashXfer (uint32_t n);

extern int AutoFlashFile;

#endif // __FLASH_H__
