#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/// The Trace function tracks VM state changes using these parameters:
/// Type of state change: 0 = unmarked, 1 = new opcode, 2 or 3 = new group;
/// Register ID: Complement of register number if register, memory if other;
/// Old value: 32-bit.
/// New value: 32-bit.

void Trace(uint8_t type, int32_t ID, uint32_t old, uint32_t nu){
}

//This is usually used for UNDO
