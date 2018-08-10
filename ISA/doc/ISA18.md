# MachineForth ISA

The MachineForth paradigm, discovered by the late Jeff Fox, is a method for programming Forth stack machines in the most direct way possible, by making machine opcodes a part of the high level language. The programmer is writing to an actual machine rather than a virtual one. It’s a different way of programming. With the right ISA, optimizations are minimal so this virtualization is unnecessary. An ISA for a dialect of MachineForth is presented. The ISA lends itself to very fast instruction set simulation on a PC or MCU, so the binary-compatible model is portable across platforms. This reduced abstraction puts the fun back in Forth.

eForth is used as a guide to which opcodes to implement. A multitasker is also supported.

## The ISA, 18-bit version

Note: This is not used currently.

The stack machine used by MachineForth is based on small, zero-operand opcodes operating on the tops of stacks and corresponding to Forth names. This is different from Novix-style stack machines, which use wide instructions equivalent to stack machine microcode executing in one cycle. While this tends to be efficient, it requires virtualization of the language just like any other machine. When it comes to executing an application in a modern stack machine, instruction execution time is even more meaningless than it classically has been. When your computer consists of Verilog or VHDL code, the compute-intensive parts of the application are in hardware. RTL is the new machine code. The compiler is simple enough that you can trivially add custom opcodes.

MISC computers are minimal instruction set computers. They were pioneered by Chuck Moore, Jeff Fox, and GreenArrays using an asynchronous design flow. Since industry design flows are based on synchronous logic, a practical stack machine should use leverage synchronous memories. This affects the ISA. With synchronous memories, you need to start a read a cycle before it’s needed. This forces an extra pipeline stage, but also affords more sophisticated decoding. For example, the opcode is registered at the same time as signed immediate data. Some of that immediate data can extend the opcode.

The MISC paradigm executes small instructions fast, from an instruction group. The instruction group is fetched from memory, and then the opcodes are executed in sequence within the group.

The best instruction word size is 18 bits, to fit FPGA RAM. 256K 18-bit words amounts to 576K bytes of code space. Data space can be bigger due to a page register.

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
| **0** | nop      | dup      | jmp      | call     | user     | reg@     | -bran     | drop     |
| **1** | xor      | invert   | and      | 0=       | +        | +c       | u2/       | c2/      |
| **2** | exit     | r>       | >r       | over     | next     | r@       | swap      | reg!     |
| **3** | !+       | @+       | @        | lit      | c!+      | c@+      | c@        | rot      |

- **opcode uses the rest of the slots as immediate data**
- Any kind of stack/RAM read must be in columns 2, 3, 6, or 7.

### Immediate data format

Immediate data is taken from the remainder of the IR. These opcodes may be in slots 0, 1 or 2. Slot 3 is possible to use.
IR[0] will still be evaluated, so `lit` will see -1. IR[0]: sign of data, 1s complement if '1'.

- `jmp`, `call`, `-bran`, `next`:  IMM[0]: 0=absolute, 1=relative.
- `lit`, `user`, `reg!`, `reg@`:  IMM is literal, function select, or index.
- `reg@` ( -- n ) fetches from an internal register.
- `reg!` ( n -- ) stores to an internal register.

Immediate data is formed from a LUT4 with inputs of {IR[x], slot[1:0], IR[0]}.

`reg@` pushes register data based on immediate index that feeds mux select lines:

- -2: Fetch flag ~T[17]
- -1: Fetch flag T[17]
- 0: Fetch from SP
- 1: Fetch from RP
- 2: Fetch from UP
- 3: Fetch from debug port

`reg!` loads a register based on immediate index:

- -1: Store to SP
- 0: Store to RP
- 1: Store to UP
- 2: Store to page register (AXI bus upper address)
- 3: Store to debugport
- 4: Fetch N cells from AXI space starting at address T
- 5: Store N cells to AXI space starting at address T

The non-obvious opcodes:

- `user` ( n1 -- n2 ) is a user defined function of T, N, and R.
- `next` is for loops. Jump if R >= 0; R = R - 1.

`0=` is a critical path with LUT4s (3 levels of logic), so it's the only zero test. `if` uses `0=` and `-bran`.
`next` tests R instead of T. Since most `for` loops have a trivial case to test for, the `next` forward branch
is placed at the beginning of a loop. The end of loop uses a backward jump. Post-loop cleanup would be `nop r> drop`.

`lshift` etc. can use a relative jump into a sets of DUP + opcodes. For example, a relative jump is:

`: exec  ( offset -- )  r> + >r ; call-only`

### Sample usage

The RAM used by the CPU core is relatively small. To access more memory, you would connect the AXI4 bus to other memory types such as single-port SRAM or a DRAM controller. Burst transfers use a !AS or @AS instruction to issue the address (with burst length=R) and stream that many words to/from external memory. Code is fetched from the AXI4 bus when outside of the internal ROM space. Depending on the implementation, AXI has excess latency to contend with. This doesn’t matter if most time is spent in internal ROM. The page register extends the address space of AXI to 32-bit. AXI data width is 32-bit, but only 18 bits are used.

In a hardware implementation, the instruction group provides natural protection of atomic operations from interrupts, since the ISR is held off until the group is finished. A nice way of handling interrupts in a Forth system, since calls and returns are so frequent, is to redirect return instructions to take an interrupt-hardware-generated address instead of popping the PC from the return stack. This is a great benefit in hardware verification, as verifying asynchronous interrupts is much more involved. As a case in point, the RTX2000 had an interrupt bug.

Hardware multiply and divide, if provided, are accessed via the USER instruction. The basic opcodes support the slow eForth methods.

## Comparison to Bernd Paysan's B16

A lot of thought went into the B16. It has some useful features. For example, divide and multiply steps. I figure those steps can be managed in hardware. Since I'm shamelessly using block RAM for stacks, it's going to be a bit different. But for reference, here is the B16 ISA:

```
### B16 reduced instruction set:
1, 5, 5, 5 bits
|       | 0        |1         | 2        | 3        | 4        | 5        | 6         | 7        |
|:-----:|:--------:|:--------:|:--------:|:--------:|:--------:|:--------:|:---------:|:--------:|
| **0** | nop      | call     | jmp      | ret      | *jz*     | *jnz*    | jc        | jnc      | (if slot 3, use 16-bit next word as data)
| **1** | *xor*    | invert   | *and*    | *or*     | *+*      | *+c*     | u2/       | c2/      |
| **2** | *!+*     | *@+*     | *@*      | lit      | *c!+*    | *c@+*    | *c@*      | *litc*   | (if slot 1, don't post-increment address)
| **3** | *nip*    | *drop*   | over     | dup      | *>r*     | *r>*     |           |          |
```

- @+  ( a -- a' n )
- !+  ( n a -- a' )
- MOVE step:  ( a1 a2 ) >r @+ r> !+




