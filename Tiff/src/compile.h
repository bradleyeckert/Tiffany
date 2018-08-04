//==============================================================================
// compile.h: Header file for compile.c
//==============================================================================
#ifndef __COMPILE_H__
#define __COMPILE_H__
#include <stdint.h>
//#include "config.h"

void InitIR (void);                             // clear internal compiler state
void InitCompiler(void);               // load the dictionary with basic opcodes
void Literal (uint32_t N);                                  // compile a literal
void tiffFUNC (int n, uint32_t ht);                        // execute a function
void Semicolon (void);
void NewGroup (void);                           // close out pending instruction
void tiffMACRO (void);
void tiffCALLONLY (void);
void tiffANON (void);

#endif
