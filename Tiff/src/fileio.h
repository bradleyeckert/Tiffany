#include <stdio.h>
#include <stdint.h>

int BinaryLoad(char* filename);                     // Load ROM from binary file
void ReloadFile(void);                    // reload the latest loaded image file
void SaveROMasC (char *filename);
void SaveAXIasC (char *filename, int alignment);        // alignment is in cells
