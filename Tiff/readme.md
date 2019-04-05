# Tiff, a PC host for Mforth

Tiff is a C console application that implements a minimal Forth. “Tiff” is short for “Tiffany”, the main character in the film “Bride of Chucky”.
This is the Forth that Chuck should have written. I'm not as smart a programmer as Chuck Moore (who is?) but I'm giving it a go.
Forth needs to work with the rest of the world, which means being highly embeddable.

Tiff uses a simulated Forth CPU to execute compiled code as needed. It’s designed to load a Forth system mostly from scratch,
gradually handing off all control to the simulated CPU. The resulting ROM image is binary compatible with the FPGA or ASIC based CPU,
which runs the same Forth system (big or small) as Tiff.

Tiff is intended to generate C (or assembly) code that encapsulates the Forth application.
The code exports the state of Tiff (dictionaries etc) to something you can include in a C project.
The rationale for this is that Forth's utility as a stand-alone platform is diminishing.
It's better to host a smaller Forth in a C system that leverages the libraries and middleware of today's complex IDEs.
This brings also Forth-like scripting and interactivity to C applications.

## Host Forth

The Host Forth is implemented in generic C. I use Code::Blocks to compile for Windows and execute it in ConEmu for decent VT220 emulation. I also compile and test it under Ubuntu Linux. Tiff is the name of the console application.

It implements an interpreter and a number of words using C. They use data structures in the Forth VM whenever possible. This tends to be more wordy than simply using C data structures, but it allows C functions to be replaced by Forth versions as they become available. For example, when the C version of the interpreter fills the TIB with text using C's `getline`, the Forth system running in the VM sees that same text in its TIB.

After loading up a system, Tiff can be used to run a Forth application, but it's meant to generate a Forth system in C source that will be embedded in another C project. You would compile and run that C application to run the Forth.

## Target Forth

A stack machine is simulated by the VM, so the code looks like machine code that's made from Forth primitives. The dictionary uses a dual-xt model. Depending on `state`, one xt or the other of the word from the input stream is executed. A dual-xt model allows for a classical search order, which is the feature of Forth most useful in creating domain-specific languages.

Memory spaces are oriented toward high speed implementation. This isn't C. There isn't one big, flat space that hardware mediates the best it can. There are four basic memory types:

- A relatively small internal ROM that never changes.
- A relatively small internal RAM that is accessible at high speed.
- A large external flash memory (whether on- or off-chip).
- A large external RAM such as SDRAM.

The latter two are lumped into the "AXI memory space". An AXI memory location can be read 32 bits at a time for execution or data-read, for example from flash memory. There are also two CPU instructions that perform AXI transfers to and from internal RAM. AXI tends to have some overhead that make it inefficient for single-word transfers. Burst transfers are much better. An app that uses data in SDRAM would manually cache in RAM and then access that RAM.

## Flash-based

Run-time code space, when it's writable, is assumed to be flash. You can change '1's to '0's but not vice versa. You also shouldn't try writing '0's twice, as some chips will over-charge the bit's gate charge making for unreliable behavior. Certain xts are chosen such they can be flipped to other xts by clearing select bits. For example, an xt can be flipped (by IMMEDIATE) from `compile` to `interpret`.

## Dual-xt

Normally, STATE selects which xt to execute. There are three fields in the header: xte, xtc, and w as described by Anton Ertl in his paper on dual-xt Forths.

There are two or three possible memory spaces that can be used by `create` and similar words. The normative cross compiler standard published by FORTH Inc. (in collaboration with MPE Ltd) provides CDATA, IDATA and UDATA to select these. Good enough. That standard went down the dual-wordlist road, but otherwise has a lot of good ideas. Because of the memory model, internal RAM is all IDATA and AXI space is CDATA or (maybe) UDATA.

The dictionary implements a search order of up to 8 contexts. This isn't some single-wordlist toy Forth.

## Try it out

- Linux: Navigate to the folder with Makefile. "make" builds `tiff` in the ./forth folder. You may have to go get missing libraries: GCC will complain and tell you how. Go to ./forth and type "./tiff".
- Windows Unsafe: An executable is already in the `forth` folder. Do you trust executables downloaded from the Internet? Just type "tiff" on the command line.
- Windows Safe: Compile from everything in the `src` folder. Code::Blocks is fairly easy. If you know what you're doing, you can `make` from the command line using the provided Makefile.

Tiff.f is loaded by Tiff. It saves its internal ROM at the end to provide a demo "Hello World" app. You can compile the files in the `demo` folder to make the demo app. It prints "Hello World" and quits, as you can see by typing "0 EXECUTE".

There are two dictionaries. Case sensitivity is off by default. Blank-delimited tokens are evaluated as follows:

- If it's found in the Forth dictionary, execute it with the VM or compile it.
- If it's found in the Tiff's internal dictionary (defined in C), execute it natively.
- Otherwise, push a number onto the stack or compile a literal.

WORDS and XWORDS list the words in the Forth dictionary.

You can SEE any word's disassembly or LOCATE its source text. DASM ( addr len ) disassembles `len` instruction groups. Try "0 400 DASM".

IWORDS lists the words in Tiff's internal dictionary. If you want to see their source code, to drill down into what they do, start in Tiff.c.

There is a low level debugger. Try this: "10 100 DBG UM*". The debugger window shows registers while you single step through instruction groups.
The trace buffer is reversible so you can undo to step backwards. If you want to see the registers while at the command line, +CPU turns the low level dashboard on and -CPU turns it off.




