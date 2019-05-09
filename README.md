# Tiff

An embeddable Forth model for small MCUs and FPGAs.
The VM forms a sandbox for a C system,
relying on C libraries for middleware and other wheels that you don't want to reinvent.
It's set up to compile to flash memory. Code space is flash (big) so that data space can be small.

Try `bin/tiff.exe` to play, if you like to live dangerously.
If you're a Linux user, compile from source using the Makefile.
On Windows, I just pull all files in `src` into a Code::Blocks console project.

Windows users would do well to run this console app under Conemu.
Color looks so much better. `theme=color` turns it on.
You can also enable UTF-8 in Conemu, since Tiff supports that too.
There are `make.bat` files in the example folders to make launching dead simple.

`tiff` is a C99 console application that implements the Forth `QUIT` loop. It provides a small number of keywords to compile code.
It uses a simulated Tiff CPU (`vm.c`) to execute compiled code as needed.
The `QUIT` loop keeps its internal state in the VM's memory instead of using C ints, etc.
This allows internal `tiff` C functions to be replaced by Forth definitions as they become available.
A complete ANS Forth can be built by replacing all of the internal C functions with Forth and defining a QUIT loop.

You can bootstrap an ANS Forth or just use part of Forth for your application.
The resulting ROM image can be saved as a hex file with "\<flags\> save-hex \<filename\>".

The ROM image is binary compatible with models implemented in your embedded C application
or with an FPGA or ASIC, which runs the same Forth system (big or small) as `tiff`.
`tiff` can boot from the hex file with `tiff -c myfile.hex`, which avoids most of `tiff`:
it just coldboots and runs the VM, just like a Tiff chip would.

## Command Line Syntax

`./tiff [cmds] ["forth command line"]`

Cmds are 2-character commands starting with '-'. They are:

| Cmd | Parameter    | Usage                                       |
| --- |:-------------|:------------------------------------------- |
| -f  | \<filename\> | Change startup INCLUDE filename from `mf.f` |
| -h  | \<addr\>     | Set header start address in cells           |
| -r  | \<n\>        | Set RAM size in cells                       |
| -m  | \<n\>        | Set ROM size in cells                       |
| -s  | \<n\>        | Set terminal stack region in cells          |
| -b  | \<n\>        | Set SPI flash 4k block count                |
| -i  | \<filename\> | Initialize flash image from file            |
| -o  | \<filename\> | Save flash image upon exit                  |
| -c  | \<filename\> | Hex file for cold booting (note `save-hex`) |
| -t  |              | Enable test mode if cold booting            |

Any other command produces a list of commands instead of launching the app.

## Why Forth?

tl;dr: Forth is FUN!

Programmers are shaped by their tools.
Simplicity is more than antidote to complexity in the modern age.
It's a Zen practice in its own right that benefits developers as thinking beings.
The Forth flow is about programming in the moment.
You can type in a word at the console and immediately test it.
Of course, you could code up a bunch of words in a file and compile the file,
but that's not the Forth flow.
Files are where you collect the useful concepts that worked.

The `ok>` prompt means something. You are here now.

Forth is a mathematics of implicit informatics. Parameters are passed implicitly between functions,
which seems to lead to a lower information entropy (H) in the program.
No other language offers this except by extreme complexity under the hood. Sorry, no cheating.

Programmers have a love/hate relationship with Forth. 25% love it, 25% hate it, and the rest don't care.
Nowadays, everyone is a coder. Coding is taught to children.
What if 25% of them would love the Forth paradigm?

The problem with the Forth world is insularity.
You can have all the libraries you want as long as they're Forth.
Fortunately, the raw speed of Forth is mostly irrelavant if C libraries can be leveraged.
That means you can write your app in Forth, but execute it in a sandbox in your C application.
Lua does that too. But how much fun is Lua? Try teaching Lua to a 10-year-old.

## Small Instruction Stack Machines

Although RISC-V and other CPUs are available for embedded system use,
they have a complexity cost in terms of software and hardware.
They tend to be a bit bulky for mere mortals to build up the CPU and the tools from scratch.
Computers and compilers don't have to be complex.
Small instruction stack machines use small, implicit opcodes in an instruction group.
The opcodes operate on stacks rather than registers, so each opcode can be 5 or 6 bits.
They are packed into an instruction word between 24 and 32 bits wide.

The opcodes of this stack machine map directly to Forth primitives.
The advantage here is that instead of writing to an abstract virtual machine,
you're writing to an actual machine.
You don't need that abstraction because the machine will always be there
in whatever implementation is most convenient.
It's a "Run Anywhere" machine model.
Optimizations are minimal because the language of the machine is so closely matched to the Forth language.

Executing an instruction in a group allows looping and conditional skips inside the group.
While the group executes a loop, no instructions need to be fetched from code space.
In practice maybe not too useful, but it's nice for `move` and `fill`.

Although this kind of stack machine has less semantic density per clock cycle than register machines
(after code-in-a-blender optimizations),
that doesn't matter with a machine that's trivial to customize to the application.

## Platforms for Tiff

SPI flash is a very useful device for expansion of code space.
This is especially true for Forth, since it spends most of its time in internal ROM.
An MCU with attached SPI flash provides a huge system at minimal cost.
RAM is the only thing that's constrained, which is okay since Forth is very stingey with RAM.
At slightly more cost, an FPGA can replace the MCU.
The ISA is oriented toward implementation in an FPGA using a block RAM for stack and data space.

Instruction set simulation is very simple, so it can run quickly in even an MCU.
This allows a VM to run as a sandbox in an MCU-based system that provides middleware and/or OS services.
Mixed language systems provide the best of both worlds.
The use of C to leverage MCU development flows and libraries.
Today's MCUs are as complex as the streets of London. Do you want to be a London cabbie?
Riding the wave of modern SDKs is becoming a necessity.

A C console application provides a minimal Forth that has a built-in VM model for executing code.
This VM model simulates the instruction set of the CPU so that
if a full Forth system can be built up in that environment, a text file in C, assembly, Verilog, or VHDL
can be generated to provide the same system on embedded hardware.
This is simpler than traditional cross complation because the host runs target code at compile time.

The VHDL folder contains a Forth processor that runs in ModelSIM. You can run the examples in a VHDL simulator.
The vanilla RTL is fully synthesizable, with RAMs and ROMs being inferred by FPGA tools.

## What's programming in Tiff like?

Tiff starts up with a relatively blank slate.
It compiles the headers it needs to handle keywords, but code space is blank.
There's no separation between assembly language and Forth since Forth is the assembly.
I find myself using tokens I know are opcodes or short opcode sequences.
They are often free (in terms of code size) if used before literals and calls,
which affects how I code.

Tiff is basically a cut-down ANS Forth. You can write an application in it.
ANS Forth is one such application. It's a rather large beast, but fun to ride.
To make a standalone app, you would generate C source suitable for embedding,
then compile that with a C compiler.

## About the name

Mforth and MachineForth were taken and/or overloaded.
What Forth would Chuck Moore have written if he were more tasteless and less smart?
This one, of course! So, Tiffany from the film "Bride of Chucky". Perfect for this hack job.

