#include <stdio.h>
#include <stdint.h>
#include "config.h"

uint32_t DbgPC; // last PC returned by VMstep

uint32_t DbgGroup (uint32_t op0, uint32_t op1,   // Execute an instruction group
                   uint32_t op2, uint32_t op3, uint32_t op4);

uint32_t FetchSP (void);                                    // Get stack pointer
uint32_t PopNum (void);                                    // Pop from the stack
void PushNum (uint32_t N);                                  // Push to the stack
uint32_t FetchCell (uint32_t addr);                      // Read from RAM or ROM
void StoreCell (uint32_t N, uint32_t addr);                      // Write to RAM
void StoreROM (uint32_t N, uint32_t addr);                       // Write to ROM
uint8_t FetchByte (uint32_t addr);                       // Read from RAM or ROM
uint16_t FetchHalf (uint32_t addr);                      // Read from RAM or ROM
void StoreByte (uint8_t N, uint32_t addr);                       // Write to RAM
void EraseSPIimage (void);                   // Erase ROM and/or SPI flash image
void FetchString(char *s, unsigned int address, uint8_t length);   // get string
void StoreString(char *s, unsigned int address);       // Store unbounded string
int Rdepth(void);                                          // return stack depth
int Sdepth(void);                                            // data stack depth
void CommaC (uint32_t X);                         // append a word to code space
void CommaD (uint32_t X);                         // append a word to data space
void CommaH (uint32_t X);                       // append a word to header space
void CommaHstring (char *s);                     // compile string to head space
void CommaDstring (char *s);                     // compile string to data space
void CommaCstring (char *s);                     // compile string to code space
void CommaXstring (char *s, void(*fn)(uint32_t), int flags);   // generic string

void CreateTrace(void);                  // allocate memory for the trace buffer
void DestroyTrace(void);                     // free memory for the trace buffer

void InitializeTermTCB(void);                           // Initialize everything
void InitializeTIB(void);             // Initialize just the terminal task input
void DumpRegs(void);            // clear screen and dump register and stack data


#define DataStackCol     1
#define ReturnStackCol  13
#define RegistersCol    25
#define RAMdumpCol      38
#define ROMdumpCol      51
#define DumpRows         9
