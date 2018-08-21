# Blocks

Blocks are based on SPI flash. They are used for source code. Blocks make for easy editing. The block buffer should be 4K bytes to match the sector size. This buffer should be in AXI space.

Reading from blocks uses memory instructions, so the TIB can be in SPI flash. The length of the TIB can be one or more blocks as specified by `load` or `thru`.

The header ID will use a file ID of 255 and the 16-bit line number will be the >IN pointer divided by 64. This allows up to a 16M byte chunk of source code. When displaying this "line number", it can be translated to a block number and line number.

A block can be 64 rows of 64 columns. They may contain utf-8, which leaves blank space (in the terminal) at the end of a line.

# Files

Files can be in flash, using a file system. ZMODEM can be used to upload and download files, so the editor needn't be in the system. The file system can be fairly simple. The first block could contain a list of filenames and associated pointers and lengths.

Such data structures can be constructed through Forth "keyboard entry" to allow "files" to be put into the system before there is code to do the job. Zmodem is a serial terminal feature. Command lines don't have it. However, an app can pipe data into stdin for that. This needs to be investigated.


