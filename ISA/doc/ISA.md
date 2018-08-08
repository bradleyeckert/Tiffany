# MachineForth ISA

The MachineForth paradigm, discovered by the late Jeff Fox, is a method for programming Forth stack machines in the most direct way possible, by making machine opcodes a part of the high level language. The programmer is writing to an actual machine rather than a virtual one. It’s a different way of programming. With the right ISA, optimizations are minimal so this virtualization is unnecessary. An ISA for a dialect of MachineForth is presented.

## The ISA

The stack machine used by MachineForth is based on small, zero-operand opcodes operating on the tops of stacks and corresponding to Forth names. This is different from Novix-style stack machines, which use wide instructions equivalent to stack machine microcode executing in one cycle. While this tends to be efficient, it requires virtualization of the language just like any other machine. When it comes to executing an application in a modern stack machine, instruction execution time is even more meaningless than it classically has been. When your computer consists of Verilog or VHDL code, the compute-intensive parts of the application are in hardware. RTL is the new machine code. The compiler is simple enough that you can trivially add custom opcodes.

MISC computers are minimal instruction set computers. They were pioneered by Chuck Moore, Jeff Fox, and GreenArrays using an asynchronous design flow. Since industry design flows are based on synchronous logic, a practical stack machine should use leverage synchronous memories. This affects the ISA. With synchronous memories, you need to start a read a cycle before it’s needed. This forces an extra pipeline stage, but also affords more sophisticated decoding.

The MISC paradigm executes small instructions very fast, from an instruction group. The instruction group is fetched from memory, and then the opcodes are executed in sequence within the group. The ISA lends itself to very fast instruction set simulation on a PC or MCU, so the binary-compatible model is portable across platforms. This reduced abstraction puts the fun back in Forth.

The best instruction word size is 18 bits, to fit FPGA RAM. This allows 256K addresses.

### Instruction Group

The ISA uses 5-bit opcodes in a 18-bit instruction group. Opcodes that use immediate data take that data from the remainder of the instruction group. A slot pointer steps through the opcodes in sequence. It can be conditionally reset to cause a loop, or set to the end to skip the rest of the opcodes in the group.

| Slot | Bits  | Range   |
| ---- |:-----:| -------:|
| 0    | 17:15 | 0 to 7  |
| 1    | 14:10 | 0 to 31 |
| 2    | 9:5   | 0 to 31 |
| 3    | 4:0   | 0 to 31 |

The skinny slot is at the upper end to allow 15-bit immediate data. Jumps and calls are cell-addressed, allowing 72K bytes code space. SPI flash may be addressed as 3-byte cells, with the upper 6 bits unused (75% utilization). The SPI hardware figures the address using multiply-by-3, which is trivial.

Preliminary opcodes in 2-digit octal format:

|       | 0        |1         | 2        | 3        | 4        | 5        | 6         | 7        |
|:-----:|:--------:|:--------:|:--------:|:--------:|:--------:|:--------:|:---------:|:--------:|
| **0** | nop      | invert   | exit     | +        | **jump** | **call** | **0bran** | **lit**  |
| **1** | dup      | over     | >r       | and      | **user** | **reg!** | **next**  | **reg@** |
| **2** | 2*       | 2/       | r>       | xor      | 1+       | ><       | @a        | !a       |
| **3** | r@       | u2/      | a!       | drop     | swap     | a        | @a+       | !b+      |

- **opcode uses the rest of the slots as unsigned (or signed if 0bran or next) immediate data**
- Any kind of stack/RAM read must be in columns 2, 3, 6, or 7.

The non-obvious opcodes are:

- `><` ( n1 -- n2 ) swaps the upper and lower 9-bit chars of T.
- `user` ( n1 -- n2 ) is a user defined function of T, N, and R.
- `reg@` ( -- n ) fetches from an internal register.
- `reg!` ( n -- ) stores to an internal register.
- `next`  ( -- ) adds signed displacement to PC if R >= 0; R = R - 1.
- `0bran`  ( flag -- ) adds signed displacement to PC if T = 0.

`><` is used with bytes, but also to create long literals. "HiPart lit >< LoPart lit +".

`reg@` pushes data based on two 3-bit octal digits U,L. The lower digit is:

- 0: Fetch from SP, add U offset
- 1: Fetch from RP, add U offset
- 2: Fetch from UP, add U offset
- 3: Fetch from flags[U]: {0, -1, carry (out of + or 2*), NoCarry, T[17], ~T[17], T=0, T<>0}
- 4: Fetch from debug port

`reg!` loads a register based on two 3-bit octal digits. The lower digit is:

- 0: Store to SP
- 1: Store to RP
- 2: Store to UP
- 3: Store to B
- 4: Store to debug port
- 5: Fetch/Store T cells to/from AXI space

### Opcodes (proposed)

```
jump  IMM → PC
lit   IMM →  T → N → RAM[--SP]
call  IMM → PC → R → RAM[--RP]
dup          T → N → RAM[--SP]
over     N → T → N → RAM[--SP]
a        A → T → N → RAM[--SP]
invert ~T → T
u2/   T>>1 → T
2/    T/2 → T
2*    T*2 → T
!as   ReadData → RAM[A] | A+4 → A, R=length	Stream from AXI bus to RAM
swap  N → T → N                         N = {T, N, RAM}
1+    T+1 → T
;     RAM[RP++] → R → PC
r>    RAM[RP++] → R → T → N → RAM[--SP]
@a    RAM[A] → T → N → RAM[--SP]
!a    RAM[SP++] → N → T → RAM[A] | A+4 → A
@as   RAM[A] → WriteData | R=length     Stream from RAM to AXI bus
@b    RAM[B] → T → N → RAM[--SP] | B+4 → B
rot   RAM[SP] → T → N → RAM[SP]
+     RAM[SP++] → N | (T + N) → T
and   RAM[SP++] → N | (T & N) → T
xor   RAM[SP++] → N | (T ^ N) → T
a!    RAM[SP++] → N → T → A             A and B are index registers
b!    RAM[SP++] → N → T → B
drop  RAM[SP++] → N → T
>r    RAM[SP++] → N → T → PC → R → RAM[--RP]

|	  Begin a new opcode group
```

### Sample usage

The SP, RP, and UP instructions are used to address PICK, Local, and USER variables respectively.

The RAM used by the CPU core is relatively small. To access more memory, you would connect the AXI4 bus to other memory types such as single-port SRAM or a DRAM controller. Burst transfers use a !AS or @AS instruction to issue the address (with burst length=R) and stream that many words to/from external memory. Code is fetched from the AXI4 bus when outside of the internal ROM space. Depending on the implementation, AXI has excess latency to contend with. This doesn’t matter if most time is spent in internal ROM.

In a hardware implementation, the instruction group provides natural protection of atomic operations from interrupts, since the ISR is held off until the group is finished. A nice way of handling interrupts in a Forth system, since calls and returns are so frequent, is to redirect return instructions to take an interrupt-hardware-generated address instead of popping the PC from the return stack. This is a great benefit in hardware verification, as verifying asynchronous interrupts is much more involved. As a case in point, the RTX2000 had an interrupt bug.

Hardware multiply and divide, if provided, are accessed via the USER instruction.

