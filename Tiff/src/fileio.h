#include <stdio.h>
#include <stdint.h>

int BinaryLoad(char* filename);                     // Load ROM from binary file
void ReloadFile(void);                    // reload the latest loaded image file
void MakeFromTemplate (char *infile, char *outfile);
void SaveImage (char *filename);               // save ROM/RAM/AXI image to file
