//===============================================================================
// accessvm.h
//===============================================================================
#ifndef __ACCESSVM_H__
#define __ACCESSVM_H__

#include <stdio.h>
#include <stdint.h>
#include "config.h"

extern uint32_t DbgPC;                             // last PC returned by VMstep

uint32_t DbgGroup (uint32_t op0, uint32_t op1,   // Execute an instruction group
                   uint32_t op2, uint32_t op3, uint32_t op4);

uint32_t PopNum (void);                                    // Pop from the stack
void PushNum (uint32_t N);                                  // Push to the stack
void StoreROM (uint32_t N, uint32_t addr);                       // Write to ROM
void StoreString(char *s, int32_t address);            // Store unbounded string
void FetchString(char *s, int32_t address, uint8_t length);        // get string
int Rdepth(void);                                          // return stack depth
int Sdepth(void);                                            // data stack depth
void CommaC (uint32_t x);                         // append a word to code space
void CommaC8 (uint8_t c);
void CommaD (uint32_t x);                         // append a word to data space
void CommaH (uint32_t x);                       // append a word to header space
void CommaHeader (char *name, uint32_t xte, uint32_t xtc, int Size, int flags);

uint32_t SearchWordlist(char *name, uint32_t WID);
void AddWordlistHead (uint32_t wid, char *name);
uint32_t iword_FIND (void);                     // ( addr len -- addr len | 0 ht )
uint32_t WordFind (char *name);                                     // C version

char *GetXtName(uint32_t xt);                              // look up name of xt

void ReplaceXTs(void);    // Replace XTs with executable code ( NewXT OldXT -- )

void CreateTrace(void);                  // allocate memory for the trace buffer
void DestroyTrace(void);                     // free memory for the trace buffer

void InitializeTermTCB(void);                           // Initialize everything
void InitializeTIB(void);             // Initialize just the terminal task input
void DumpRegs(void);            // clear screen and dump register and stack data
void Disassemble(uint32_t addr, uint32_t length);
void MakeTestVectors(FILE *ofp, int length, int format);
void WipeTIB (void);                                         // clear TIB memory

#define DataStackCol     1
#define ReturnStackCol  13
#define RegistersCol    25
#define RAMdumpCol      40
#define ROMdumpCol      55

#endif // __ACCESSVM_H__
