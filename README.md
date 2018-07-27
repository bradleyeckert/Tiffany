# Mforth
A full-featured portable Forth model for small MCUs and FPGAs.

## Group-oriented Stack Machines
Although RISC-V and other CPUs are available for embedded system use, they have a complexity cost in terms of software and hardware.
They tend to be a bit bulky for mere mortals to build up the CPU and the tools from scratch. Computers and compilers don't have to be complex. Group oriented stack machines use small, implicit opcodes in an instruction group. The opcodes operate on stacks rather than registers, so each opcode can be 5 or 6 bits. They are packed into an instruction word between 24 and 32 bits wide.

The opcodes of this stack machine map directly to Forth primitives. This leads to a diferent way of programming. Instead of writing to an abstract virtual machine, you're writing to an actual machine. You don't need that abstraction because the machine will always be there in whatever model is most convenient. It's a "Run Anywhere" machine model. Optimizations are minimal because the language of the machine is so closely matched to the Forth language.

Executing an instruction in a group allows looping and conditional skips inside the group. While the group executes a loop, no instructions need to be fetched from code space. Although this kind of stack machine has less semantic density per clock cycle than register machines (after code-in-a-blender optimizations), that doesn't matter with a machine that's trivial to customize to the application.

## Platforms for Mforth
SPI flash is a very useful device for expansion of code space. This is especially true for Forth, since it spends most of its time in internal ROM. An MCU with attached SPI flash is provides a huge system at minimal cost. RAM is the only thing that's constrained, which is okay since Forth is very stingey with RAM. At slightly more cost, an FPGA can replace the MCU. The ISA is oriented toward implementation in an FPGA using a block RAM for stack space. 

Instruction set simulation is very simple, so it can run quickly in even an MCU. This allows a VM to run as a sandbox in an MCU-based system that provides middleware and/or OS services. Mixed language systems provide the best of both worlds. The use of C to leverage MCU development flows and libraries. The use of Forth for extensible applications in the spirit of Lua but with a VM that's crazy small and tight.

A C console application provides a minimal Forth that has a built-in VM model for executing code. This VM model simulates the instruction set of the FPGA or other versions of the CPU so that if a full Forth system is built up in that environment, a text file in C, assembly, Verilog, or VHDL can be generated to provide the same system on embedded hardware. This is simpler than traditional cross complation because the host runs target code at compile time. Code that's binary compatible with the target can be tested on the desktop.

Simplicity is more than antidote to complexity in the modern age. It's a Zen practice in its own right that benefits developers as thinking beings. People are shaped by their tools.
