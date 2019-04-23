//==============================================================================
// fileio.h
//==============================================================================
#ifndef __FILEIO_H__
#define __FILEIO_H__
#include <stdio.h>
#include <stdint.h>

void MakeFromTemplate (char *infile, char *outfile);
void SaveImage (char *filename);               // save ROM/RAM/ROM image to file

#endif // __FILEIO_H__
