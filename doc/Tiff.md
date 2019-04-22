# Tiff, a PC host for Mforth

Tiff is a straight C console application that implements a "minimal" Forth.
“Tiff” is short for “Tiffany”, the main character in the film “Bride of Chucky”.
Trust me, the C code is a slasher horror show.

Tiff uses a simulated Mforth CPU to execute compiled code as needed. It’s designed to load a Forth system mostly from scratch,
gradually handing off all control to the simulated CPU. 
The resulting ROM image is binary compatible with models implemented in your embedded C application
or with an FPGA or ASIC, which runs the same Forth system (big or small) as Tiff.

## Input Streams

The interpreter can have an arbitrary input buffer. By default it's TIB. Blocks may also be used as input, or even arbitrary sized chunks of external flash.
The interpreter gets its buffer parameters from three cells fixed in memory:

```
1 cell	TIBB	Buffer start address
1 cell	TIBS	Length of text in the buffer
1 cell	TOIN	Index (>IN) to the next byte to be interpreted
```

When files are INCLUDED, the TIB filled by the keyboard is cleared because files are loaded into the TIB a line at a time. Line length is restricted to the size of the TIB. Tiff saves filenames, file handles, and line numbers on a stack (in C space) when nesting files. When a file is opened, that data structure is pushed onto the filename stack. When the EOF is reached, the top of the filename stack is discarded and that file is closed. The TIB is cleared and then filled from the previous file or the keyboard. If Tiff hits an error, the filename stack is dumped to stdout. This is in `Tiff.c`.

## Interleaved vs Non-interleaved
Code and head spaces in a Forth may be interleaved or not. The current implementation does not. They use different chunks of code memory.

Portability: Interleaving rules out multiple entry points. A useful word for multiple entry is “;:”. For example:

```
: FOO  ( – ) MyLiteral [
: BAR  ( n – ) blah blah
;
```

On a non-interleaved system, FOO falls straight through to BAR. On an interleaved system, FOO would crash into a header.

For coding to compile correctly on both interleaved and non-interleaved systems, the use of HERE in table creation must avoid being stepped on by headers. For example:

```
CDATA HERE
1 , 10 , 100 , 1000 , 10000 , 100000 , 1000000 , 10000000 ,
EQU POWERS_10
```

Tiff uses three dictionary pointers: CP, HP, and DP corresponding to code space, head space and data space. The Tiff model erases RAM at power-up. As Forth code is loaded, it loads non-zero values into various RAM locations. When outputting code to the target, it includes code to initialize these non-zero values. You can think of all RAM as IDATA.

## The Interpreter

The interpreter loop takes text tokens from the input stream until it’s exhausted. This stream is usually the TIB, filled by either the keyboard or the next line of a file, or a 1K block buffer. Since it’s a dual xt system, FIND is replaced by HFIND ( c-addr u –- ht | c-addr u 0 ), where ht is the header token. If the counted name string is not found, the ht is 0.

If not found, an attempt is made to convert the string to a number. This where we break with classic Forth and its use of decimal. Decimal does not produce a double number. Instead, it (to be implemented) converts the number to IEEE floating point format. The number is pushed onto the stack. If STATE is -1 (compiling), a LitPending flag is set. A literal will be compiled by the next instruction, if can integrate it, or a literal opcode will be compiled.

Finally, if it can’t be converted to a number, then it’s an error. The tiffIOR variable is set to -13 and the rest of the input buffer is discarded.

The Tiff version of the interpreter implements a host-only keyword search if the word is not found. This is to avoid cluttering head space with headers that are only used for host side keywords such as debugging tools and settings. This uses a simple array of minimal header structures in C space.

### Interpreting

When a word is interpreted, xte is fetched and executed. If the word is a constant, the execution function may read the value from w and push it onto the stack. If the word needs to return more than a cell, w would point to a data structure containing the data. For most Forth definitions, xte is the address of the executable code of that word.

### Compilation

When a word is compiled, xtc is fetched and executed. If the word is a constant, the compilation function may compile a literal copy of w. For most Forth definitions, xtc simply compiles a call to the address in w. When a call is compiled, a pointer to its opcode is saved. Tail call optimization is performed by EXIT by clearing a bit of that opcode rather than compiling an EXIT.

The compilation of primitives, such as zero-operand instructions, is handled by a compilation function that compiles the opcode on w. Literals are saved in a list for use by operators that can use them. For example, the compilation function of + will pull from the literal list and then compile a +I instruction.
This dual-xt header scheme allows for efficient code generation, but it’s a little incompatible with Forth’s “tick”. Tiff returns xte. What we want is a primitive below tick, which returns the address of xtc (H1) in the header structure. Let’s call this H’ and the returned token xth. You could then 2@ an xth to get ( w xtc ). Typical word compilation would use H’ 2@ FFFFFF AND EXECUTE where xtc is the xte of COMPILE,.

You may notice that some headers will need to be created (in the VMgen) before the xte of COMPILE, is known.
For example, an xtc of -2 in the VMgen is used as Tiff's version of `COMPILE,`. 
All xts of internal Tiff functions are negative numbers.
These will be cleaned up in `wean.f`. 

A key feature of xtc tokens is that some of them have strategic addresses such that
flipping a bit from `1` to `0` changes the behavior to something else.
IMMEDIATE makes use of such a patch, which is a flash memory hack.
You get to change `1` bits to `0` but not vice versa.
Smokey, this is not ‘Nam. This is bowling. There are rules.

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
Tiff loads header space with a few headers that have native functions (numbered by index into a function table). It has its own version of QUIT, which implements a simple version of the Forth interpreter. Later on, when the system has been compiled, a new QUIT is defined (using EXEC: instead of :) to change the xte in QUIT’s header. Since all of the headers are in the ROM image, they will be intact when the ROM image is exported as C or Assembly source code for embedded VMs.

