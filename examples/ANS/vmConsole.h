//==============================================================================
// vmConsole.h
//==============================================================================
#ifndef __VMCONSOLE_H__
#define __VMCONSOLE_H__

#ifdef defined(__linux__) || defined(__APPLE__)
void CookedMode();
void RawMode();
#endif // __linux__

uint32_t vmKeyFormat(uint32_t dummy);   // Terminal arrow key format
uint32_t vmQkey(uint32_t dummy);
uint32_t vmKey(uint32_t dummy);
uint32_t vmEmit(uint32_t dummy);

#endif // __VMCONSOLE_H__
