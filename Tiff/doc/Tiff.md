# Tiff, a PC host for Mforth

Tiff is a straight C console application that implements a minimal Forth. “Tiff” is short for “Tiffany”, the main character in the film “Bride of Chucky”.

Tiff uses a simulated Mforth CPU to execute compiled code as needed. It’s designed to load a Forth system mostly from scratch, 
gradually handing off all control to the simulated CPU. The resulting ROM image is binary compatible with the FPGA or ASIC based CPU,
which runs the same Forth system (big or small) as Tiff.

Note: Not actively updated.

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

### Flash-friendly Wordlist Structure
![Header Links](https://github.com/bradleyeckert/Mforth/blob/master/Tiff/doc/header.png "Header Lists in Flash")

The above illustration shows a header list in flash memory, made with links that are resolved once. The “First GILD” section of the list has its wordlist pointers resolved. The second section may or may not have its wordlist resolved, depending on whether or not GILD has been executed. In a non-hashed list, a wordlist would contain one pointer. Hashing a list is relatively easy. The index into the wordlist pointer array (of several threads) is derived from the name.

The second wordlist is searched first. WID is a ROM address that’s in the search order list. The links are followed forward to find the most recently defined WID block. If that block is programmed, the list is traversed from a pointer in the list. Otherwise, the head pointer is obtained by following the forward links.

A section of lexicon can be removed without wiping out the whole dictionary (of previously defined words) by erasing that section. GILD creates new wordlist pointers for the headers of the next section of lexicon. It could align that to 4KB sector boundaries for SPI flash.

![Header Detail](https://github.com/bradleyeckert/Mforth/blob/master/Tiff/doc/headstruct.png "Header Detail")

## Interleaved vs Non-interleaved
Code and head spaces may be interleaved or not. Code memory is divided between internal ROM and external flash. In the case of an FPGA implementation, a quad-rate SPI flash can be clocked at the same frequency as the CPU. It takes eight beats to read in a 32-bit code word. That’s a lot slower than the single cycle access of internal ROM. It’s also much cheaper than internal ROM. Tiff should have separate head and code spaces when building the ROM image.

In an FPGA or ASIC, the SPI flash should be loadable through JTAG or other interface. In manufacturing, the FPGA bitstream is loaded through JTAG. If possible, the same JTAG should be able to load the SPI. Xilinx has LUT6 elements that nicely fit 6-bit opcodes. The low-cost devices boot from SPI flash.

On the target, internal ROM is fixed. All add-on code is in external flash, so interleaving is okay there. Interleaving rules out multiple entry points, however. A useful word for multiple entry is “;:”. For example:

```
: FOO  ( – ) MyLiteral
;: BAR  ( n – ) blah blah
;
```

On a non-interleaved system, FOO falls straight through to BAR. On an interleaved system, :; compiles a forward jump (over the header) to BAR. For coding to compile correctly on both interleaved and non-interleaved systems, the use of HERE in table creation must avoid being stepped on by headers. For example:

```
CDATA HERE
1 , 10 , 100 , 1000 , 10000 , 100000 , 1000000 , 10000000 ,
EQU POWERS_10
```

So, interleaved systems have workarounds. Tiff uses three dictionary pointers: CP, HP, and DP corresponding to code space, head space and data space. The pointer used by HERE (and ORG) is determined by the keywords CDATA, HDATA, and DATA. The programming model excludes the initialized RAM (IDATA) supplied by typical Forth cross compilers because the model is different. The Tiff model erases RAM at power-up. As Forth code is loaded, it loads non-zero values into various RAM locations. When outputting code to the target, it includes code to initialize these non-zero values.

The target system erases RAM and processes a run-length compressed table in ROM to initialize the RAM to the same state that Tiff had when creating the table. This sparsely-populated RAM uses run length compression controlled by a stream of 8-bit bytecodes:

```
00h = terminate initialization
04h to 07h = set address (1 to 4 bytes)
30h to 7Fh = loop stream backwards by 1 to 64 byte positions
80h to FFh = set repeat counter, 0 to 127, for looping backwards
```

## The Interpreter

The interpreter loop takes text tokens from the input stream until it’s exhausted. This stream is usually the TIB, filled by either the keyboard or the next line of a file, or a 1K block buffer. Since it’s a dual xt system, FIND is replaced by LOCATE ( c-addr u – ht | c-addr u 0 ), where ht is the header token. If the counted name string is not found, the ht is 0. Otherwise, ht points to the H2 word of the header. Depending on STATE, the xt at either H2 or H4 is executed.

If not found, an attempt is made to convert the string to a number. This where we break with classic Forth and its use of decimal. Decimal does not produce a double number. Instead, it converts the number to IEEE floating point format. Built-in C-style radix and quotes are also supported. The number is pushed onto the stack. If STATE is -1 (compiling), a LitPending flag is set. A literal will be compiled by the next instruction, if can integrate it, or a literal opcode will be compiled. 

Finally, if it can’t be converted to a number, then it’s an error. The tiffIOR variable is set to -13 and the rest of the input buffer is discarded.

The Tiff version of the interpreter implements a host-only keyword search if the word is not found. This is to avoid cluttering head space with headers that are only used for host side keywords such as debugging tools and settings. This uses a simple array of minimal header structures in C space.

### Interpretation

When a word is interpreted, xte is fetched and executed. If the word is a constant, the execution function may read the value from w and push it onto the stack. If the word needs to return more than a cell, w would point to a data structure containing the data. For most Forth definitions, xte is the address of the executable code of that word.
###Compilation
When a word is compiled, xtc is fetched and executed. If the word is a constant, the compilation function may compile a literal copy of w. For most Forth definitions, xtc simply compiles a call to the address in w. When a call is compiled, a pointer to its opcode is saved. Tail call optimization is performed by EXIT by clearing a bit of that opcode rather than compiling an EXIT.

The compilation of primitives, such as zero-operand instructions, is handled by a compilation function that compiles the opcode on w. Literals are saved in a list for use by operators that can use them. For example, the compilation function of + will pull from the literal list and then compile a +I instruction.
This dual-xt header scheme allows for efficient code generation, but it’s a little incompatible with Forth’s “tick”. Tiff returns xte. What we want is a primitive below tick, which returns the address of xtc (H1) in the header structure. Let’s call this H’ and the returned token xth. You could then 2@ an xth to get ( w xtc ). Typical word compilation would use H’ 2@ FFFFFF AND EXECUTE where xtc is the xte of COMPILE,.
You may notice that some headers will need to be created (in the VMgen) before the xte of COMPILE, is known. These will be cleaned up afterwards: An xtc of -2 in the VMgen is used as a placeholder for COMPILE,. There are other placeholder values, all of which are negative numbers.
When the target VM creates a header at run time, undefined fields are -1 to allow patching the flash memory later.

### Loading

Within Tiff are header, code and data spaces that will be compiled as ASCII text into a ROM source file for the VM. A copy of the VM switch interpreter is used by the VM to enable words to be interpreted as they are defined. Stacks and data are kept within the VM.
To execute OVER, for example, the VM would note the SP and call the ROM code for OVER. The VM runs in a tight loop until SP matches or exceeds its original value. It would push and pop data directly by passing commands to the VM.

Tiff only understands how to execute a word in the VM and append code to header or code space. It uses variables such as BASE, STATE, TIB, etc. in their actual locations in data space for the interpreter loop run by C vmgen in order to fetch and interpret code before any interpretation code has been compiled. The headers are patched as needed to switch compilation tasks over to the VM from the C environment. Header xts may be either compiled Forth words or C functions. The C functions use negative numbers as an index into a function table, to distinguish from positive Forth code addresses.

For example, \ discards all TIB content to the end-of-line. The C version manipulates data space to pull this off, but once \ is compiled the xts are modified to use the new version. The new \ uses the existing header rather than creating a new one. The EXEC: keyword uses the existing header and patches its xte and its xtc. The default xtc does a simple compile. COMP: is similar but only patches xtc.

Tiff ends up being a full Forth, built from the input file. If the output file is missing, the embedded VM isn’t generated. Since the sandbox only talks to your machine through an API list. The VM’s API function uses stdio to connect to the console for things like TYPE and KEY. 

When the VM is generated, source code is created for the ROM array, which is an ASCII representation of the binary code space image. Code and headers are kept in separate sections of the ROM array to allow groups of words to be beheaded and buried to save on code space. 

Keywords recognized by Tiff are defined at run-time using the NewHeader function, which creates a new header.

Code sections use two pointers for HERE: CP and HP for code and header spaces. 
STDIO is used for I/O in Tiff. The VM invokes them via API calls. The hard VM emulates getch and putch in hardware, which operates a UART directly. Terminals operate in cooked (or canonical) mode or raw mode. It appears that the mode can’t be switched in real time by an escape sequence. Raw mode requires the target to handle line editing, cut and paste, etc. That’s a bit limiting, given the nice clipboard integration that modern terminal emulators have. Cooked mode allows for a dumber ACCEPT. Cut and paste in raw mode needs to be researched.

### Compile Order
As much is done by the VM as possible. It starts out brain dead. Tiff loads header space with a few headers that have native functions (numbered by index into a function table). It has its own version of QUIT, which implements a simple version of the Forth interpreter. Later on, when the system has been compiled, a new QUIT is defined (using EXEC: instead of :) to change the xte in QUIT’s header. Since all of the headers are in the ROM image, they will be intact when the ROM image is exported as C or Assembly source code for embedded VMs.

