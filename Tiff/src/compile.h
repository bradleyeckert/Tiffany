//==============================================================================
// compile.h: Header file for compile.c
//==============================================================================
#ifndef __COMPILE_H__
#define __COMPILE_H__
#include <stdint.h>
#include "config.h"

void InitIR (void);
void Literal (int32_t N);
void tiffFUNC (int n, uint32_t ht);

#endif
