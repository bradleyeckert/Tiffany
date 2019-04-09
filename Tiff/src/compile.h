//==============================================================================
// compile.h: Header file for compile.c
//==============================================================================
#ifndef __COMPILE_H__
#define __COMPILE_H__
#include <stdint.h>
//#include "config.h"

void InitIR (void);                             // clear internal compiler state
void InitCompiler(void);               // load the dictionary with basic opcodes
void Literal (uint32_t n);                                  // compile a literal
void tiffFUNC (int32_t n);                                 // execute a function
void CompSemi (void);                                          // end definition
void CompExit (void);                                            // compile exit
void Compile (uint32_t addr);                                  // compile a word
void compile (char *name);                                // compile at run time
void NewGroup (void);                           // close out pending instruction
void tiffMACRO (void);                  // convert current definition to a macro
void tiffCALLONLY (void);                 // tag current definition as call-only
void tiffANON (void);                     // tag current definition as anonymous
void ListOpcodeCounts(void);                    // list the opcode count profile
void ListProfile(void);                              // list the ROM hit profile

uint32_t DisassembleIR(uint32_t IR);         // disassemble an instruction group
void NoExecute (void);                                 // ensure we're compiling
void CompLiteral (void);                                      // compile literal
void CompAhead (void);
void CompIfNC  (void);
void CompPlusIf(void);
void CompIf    (void);
void CompElse  (void);
void CompThen  (void);
void CompDefer (void);
void CompBegin (void);
void CompAgain (void);
void CompUntil (void);
void CompWhile (void);
void CompRepeat(void);
void CompQDo   (void);
void CompDo    (void);
void CompLoop  (void);
void CompI     (void);
void CompLeave (void);
void CompPlusUntil (void);
void CompType(uint32_t cp);     // compile code for TYPE
void CompCount(uint32_t cp);
void CompComma (void);

extern uint32_t breakpoint;

#endif
