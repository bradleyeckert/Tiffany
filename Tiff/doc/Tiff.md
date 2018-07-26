# Tiff, a PC host for Mforth

Tiff is a straight C console application that implements a minimal Forth. “Tiff” is short for “Tiffany”, the main character in the film “Bride of Chucky”.

Tiff uses a simulated Mforth CPU to execute compiled code as needed. It’s designed to load a Forth system mostly from scratch, 
gradually handing off all control to the simulated CPU. The resulting ROM image is binary compatible with the FPGA or ASIC based CPU,
which runs the same Forth system (big or small) as Tiff.

## Input Streams

Input Streams
Although Tiff doesn’t use blocks, the target system does, so that should be considered now. A 1K block buffer is used by block input. LOAD and THRU need their input state saved and restored. This involves saving the block system byte address and >IN when LOAD is started and restoring them when finished.

So, the interpreter has two different input buffers: TIB and BLOCK. BLOCK may take on different addresses depending on how much RAM is dedicated to buffers. The interpreter gets its buffer parameters from three cells fixed in memory:

```
1 cell	TIBB	Buffer start address
1 cell	TIBS	Length of text in the buffer
1 cell	TOIN	Index (>IN) to the next byte to be interpreted
```

When files are INCLUDED, the TIB filled by the keyboard is cleared because files are loaded into the TIB a line at a time. Line length is restricted to the size of the TIB. Tiff saves filenames, file handles, and line numbers on a stack (in C space) when nesting files. When a file is opened, that data structure is pushed onto the filename stack. When the EOF is reached, the top of the filename stack is discarded and that file is closed. The TIB is cleared and then filled from the previous file or the keyboard. If Tiff hits an error, the filename stack is dumped to stdout.

SOURCEID identifies the source of the input stream to be used when re-filling a buffer. 0 = keyboard, 1 = command line (*argv[]), 2 = file (top of file stack), negative for blocks.

The QUIT loop doesn’t use CATCH and THROW. In a loop, it resets the stacks, points TIBB to TIB, and executes the buffer at TIBB until an error is encountered. 

```
int tiffIOR = 0;
void tiffQUIT (void) {
   while (1) {
      // reset stacks and input streams
      // reset STATE
      do {
         switch(SOURCEID) {
         case 0: // load TIBB from keyboard
         default: // load TIBB using getline(&buffer, &buffer_size, file)
         }
         tiffINTERPRET(); // interpret the TIBB until it’s exhausted
         if (!SOURCEID) printf(“ ok\n”);
      } while (tiffIOR == 0);
      // display error type 
      if (tiffIOR != -99999) break; // produced by BYE
      switch (tiffIOR) {
      default: printf(“\nError %d”, tiffIOR);
      }
   } 
}
```

## Headers

Header data structures are held in a ROM image, in several linked lists. Each list is a wordlist. Word visibility is controlled by setting up the search order, a list of wordlists to search in order. Headers use dual execution tokens. The target VM has a restriction on writing to ROM, which Tiff emulates: It may not change ‘0’ bits to ‘1’ bits or write ‘0’ bits twice, as per flash memory restrictions.

The header data structure is cell-aligned in such a way as to avoid memory alignment problems on machines that are picky about alignment. Little endian byte order is assumed. A 24-bit fetch uses a 32-bit fetch followed by AND with 0xFFFFFF. The compilation scheme uses dual xts and a value. The 32-bit cells are:

```
32-bit link to list of words that reference this, -1 if none.
32-bit link to list of words that this references, -1 if none.
32-bit link to next header structure, -1 if none.
32-bit link to previous header structure, -1 if none.
32-bit xt of compilation semantics, xtc.
32-bit value, -1 if unused, w.
32-bit xt of execution semantics, xte.
8-bit number of code words in the definition.
8-bit source file ID. 0 = keyboard, 1 = blocks, 2-255 = text file (see file list).
16-bit source line number.	
2-bit flags:  IMMEDIATE and CALL-ONLY 
6-bit Name Length.
NAME text, 1 to 32 bytes.
Padding (0xFF bytes) for 4-byte alignment.
```

The beginning of a header list (the tail) contains a link value of -1, which marks the end of a search. Head space in Tiff contains several threads, each of which is a linked list. A classical wordlist data structure contains a pointer (or an array of pointers if hashing is used) that gets updated each time a header is added to the dictionary. That’s a problem for flash-based systems. You want to add to the end of the dictionary using a read-only wordlist. The way around this is with a doubly linked list. 

Although the wordlist ID points to the start of the read-only structure, its first cell starts out as -1. The word GILD ( wid – ) resolves it by traversing forward to the end of the list and replacing the -1 with that address. A forward traversal is terminated by hitting blank flash (-1). If hashed wordlists are used, the number of hash threads is the size of the array, in cells, containing -1 at the wordlist structure. The tail of the list is easily found from there.

There is a forward linked list for filenames. If a filename exists in the list, its position in the list is its ID number. If it must be appended, the filename is added to the list and the previously blank forward link (located by traversal) is populated.

### Flash-friendly Wordlist Structure
![Header Links](https://github.com/bradleyeckert/Mforth/new/master/Tiff/doc/header.png "Header Lists in Flash")

The above illustration shows a header list in flash memory, made with links that are resolved once. The “First GILD” section of the list has its wordlist pointers resolved. The second section may or may not have its wordlist resolved, depending on whether or not GILD has been executed. In a non-hashed list, a wordlist would contain one pointer. Hashing a list is relatively easy. The index into the wordlist pointer array (of several threads) is derived from the name.

The second wordlist is searched first. WID is a ROM address that’s in the search order list. The links are followed forward to find the most recently defined WID block. If that block is programmed, the list is traversed from a pointer in the list. Otherwise, the head pointer is obtained by following the forward links.

A section of lexicon can be removed without wiping out the whole dictionary (of previously defined words) by erasing that section. GILD creates new wordlist pointers for the headers of the next section of lexicon. It could align that to 4KB sector boundaries for SPI flash.

![Header Detail](https://github.com/bradleyeckert/Mforth/new/master/Tiff/doc/headstruct.png "Header Detail")

#### NameLength Word bit fields
- [31:16] = 16-bit source line number.
- [15:8] = 8-bit source file ID. 0 = keyboard, 1 = blocks, 2-255 = text file (see Filename list).
- [7] = IMMEDIATE
- [6] = CALL-ONLY
- [5] = Smudge: ‘1’ if smudged, reset to ‘0’ by ;.
- [4:0] = Name Length = 0 to 31 bytes.

The cross reference structure contains lists of cross reference structures. Since the links can only be written once, the list must be traversed in order to add to it. The list is kept short by doubling the element size with each link taken. The HEAD cells are addresses of the corresponding header structure, from which a name and source code location may be obtained.




