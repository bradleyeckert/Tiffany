//==============================================================================
// fileio.h
//==============================================================================
#ifndef __FILEIO_H__
#define __FILEIO_H__
#include <stdio.h>
#include <stdint.h>

void MakeFromTemplate (char *infile, char *outfile);
void SaveHexImage (int flags, char *filename);   // save ROM/flash image to file
void LoadHexImage (char *filename);

#endif // __FILEIO_H__
