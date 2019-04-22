//==============================================================================
// flash.h
//==============================================================================
#ifndef __FLASH_H__
#define __FLASH_H__

void FlashInit (void);
void FlashBye (void);
uint32_t FlashRead (uint32_t addr);
int FlashWrite (uint32_t x, uint32_t addr);
uint32_t SPIflashXfer (uint32_t n);

extern int InhibitFlashSave;

#endif // __FLASH_H__
