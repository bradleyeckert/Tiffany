# Command Line parameters

- `tiff.exe -t [filename]` Low level testing of CPU
- `tiff.exe -b filename [command line]` Load binary file and enter Forth interpreter
- `tiff.exe [command line]` Enter Forth interpreter

## The Terminal
Tiff uses raw keyboard input and outputs to a VT100 terminal. Is that too much to ask? Only for Microsoft, but Windows 10 finally supports VT100 emulation if you enable it. Even so, ConEmu presents a very nice command line. If you use Windows, download it. For Mac and Linux, VT100 is a subset of Xterm. If your screen looks like it threw up a bunch of garbage, your console doesn't support escape sequences. Shame on them.

## Low Level Testing
The internal ROM is erased to -1. If a filename exists, it loads it into ROM in binary format. At this point, you don't need to know much to start playing. You can edit a binary file to create executable ROM or you can type an 8-digit hex number and press X to execute it and see the stack effects. The tester operates like a postfix-style calculator. Hex digits are shifted into a 32-bit parameter. Non-digits are commands. Non-implemented commands display the key code so you know what the more esoteric key codes do. You might want to use them for something. So far, these (case-insensitive) commands are implemented:

- `0-9, A-F` Push hex digit into parameter P.
- `Enter` Clears P.
- `G` Goto: PC = P.
- `O` Pop P from the data stack.
- `P` Push P to the data stack.
- `R` Refresh the screen, useful after a Dump.
- `S` or `SPACE` Step instruction group in ROM.
- `U` Dumps 128 bytes starting from address P.
- `X` Execute the instruction group in P.
- '@' Fetch the cell at address P, leave it in P.
- `ESC` Quits the tester.
- `\` Resets the VM.

Undo and Redo are supported if TRACEABLE is defined. This keeps a log of all state changes so that execution can be stepped backward and forward. It's definitely cute. Not tested much, though.

## The Forth Interpreter
The command line is formed by concatenating any remaining argv[] strings from the C command line, separating them with blanks. This trick is necessary because C doesn't give you the raw command line. Since C strips out quotes, it looks for other quotation characters (like open-quote and close-quote) and replaces them with a normal quote.

As of 7/28/18, the interpreter recognizes some keywords (using cheap C tricks) in the absence of a Forth dictionary structure. That will be next.

These keywords are: `bye`, `\`, `.`, `+cpu`, and `-cpu`. The latter two enable and disable the raw CPU display used in low level debugging.

## To Do
Stack reporting, color change for Forth output.

Real headers in ROM space, and Mforth compilation.

Split screen allowing scrolling Forth on the bottom and the detailed register and stack dump on top.
