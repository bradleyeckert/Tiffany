# MachineForth ISA

The MachineForth paradigm, discovered by the late Jeff Fox, is a method for programming Forth stack machines in the most direct way possible, by making machine opcodes a part of the high level language. The programmer is writing to an actual machine rather than a virtual one. It’s a different way of programming. With the right ISA, optimizations are minimal so this virtualization is unnecessary. An ISA for a dialect of MachineForth is presented.

## The ISA
The stack machine used by MachineForth is based on small, zero-operand opcodes operating on the tops of stacks and corresponding to Forth names. This is different from Novix-style stack machines, which use wide instructions equivalent to stack machine microcode executing in one cycle. While this tends to be efficient, it requires virtualization of the language just like any other machine. When it comes to executing an application in a modern stack machine, instruction execution time is even more meaningless than it classically has been. When your computer consists of Verilog or VHDL code, the compute-intensive parts of the application are in hardware. RTL is the new machine code. The compiler is simple enough that you can trivially add custom opcodes.

MISC computers are minimal instruction set computers. They were pioneered by Chuck Moore, Jeff Fox, and GreenArrays using an asynchronous design flow. Since industry design flows are based on synchronous logic, a practical stack machine should use leverage synchronous memories. This affects the ISA. With synchronous memories, you need to start a read a cycle before it’s needed. This forces an extra pipeline stage, but also affords more sophisticated decoding.

The MISC paradigm executes small instructions very fast, from an instruction group. The instruction group is fetched from memory, and then the opcodes are executed in sequence within the group. An opcode can conditionally loop or skip the sequence, removing the need to change the PC to execute a loop or a short conditional.

The minimum instruction word size in bits is the base 2 log of the maximum address plus the opcode width. For 6-bit opcodes and a 1MB address range, that would be 26 bits. The primary motivation for the group-based MISC architecture is the ease with which it can be simulated so as to port applications to MCUs or to run simulations at near real speed. This also affects the design, as data widths should match that of simulation targets. Since 32 bits are the standard in MCUs, a 32-bit instruction word and 32-bit data are used.

### Instruction Group
The ISA uses 6-bit opcodes in a 32-bit instruction group. Opcodes that use immediate data take that data from the remainder of the instruction group. A slot pointer steps through the opcodes in sequence. It can be conditionally reset to cause a loop, or set to the end to skip the rest of the opcodes in the group.

| Slot | Bits  | Range   |
| ---- |:-----:| -------:|
| 0    | 31:26 | 0 to 63 |
| 1    | 25:20 | 0 to 63 |
| 2    | 19:14 | 0 to 63 |
| 3    | 13:8  | 0 to 63 |
| 4    | 7:2   | 0 to 63 |
| 5    | 1:0   | 0 to 3 |

Slot 5 only fits 4 opcodes. Immediate data is registered, so there is time to sign it.
Immediate data is taken from the remainder of the IR. IMM = IR[?:1]; IR[0]: sign of data, 1s complement if '1'.
The number of bits depends on the slot position or the opcode. It can be 26, 20, 14, 8, or 2 bits.

ALU operations take their operands from registers for minimum input delay. Since the RAM is synchronous read/write, the opcode must be pre-decoded. The pre-decoder initiates reads. The main decoder has a registered opcode to work with, so the decode delay isn’t so bad. Opcodes that read from RAM should be grouped together for easy decoding from the opcode, which is already delayed by a logic level. So, for example, rd_enable is opcode[5] and the rd_addr mux is controlled by opcode[4:1]. Non-reading opcodes are decoded from a registered opcode. The pre-read stage of the pipeline allows time for immediate data to be registered, so the execute stage see no delay. Opcodes have time to add the immediate data to registers, for more complex operations. One can index into the stack, for example.

Preliminary opcodes:

- *opcode conditionally skips the rest of the slots*
- **opcode uses the rest of the slots as signed immediate data**

op[5:3] |           | T +- N    | XP + IMM  | T+offset  | logic     | 0= / N    | user      | mem       |
|       | 0         | 1         | 2         | 3    / N  | 4         | 5         | 6         | 7         |
|:-----:|:---------:|:---------:|:---------:|:---------:|:---------:|:---------:|:---------:|:---------:|
| **0** | nop       | +         | dup       | over      | and       | up!       | user      | rot       |
| **1** | skip      | -         |           | 1+        | xor       | c!+       | 2/        | c@+       |
| **2** | +skip     |           | rp        | 2+        | or        | w!+       | sp        | w@+       |
| **3** | -skip     |           | up        | swap      | invert    | 0=        | carry     | w@        |
| **4** | rjmp      | jmp       | rept      | 4+        | r@        | !+        |           | @+        |
| **5** | exit      |           | next      |           | r>        | rp!       | u2/       | @         |
| **6** | rcall     | call      |           | >r        | lit       | sp!       | @as       | c@        |
| **7** |           |           | port      | -bran     | litx      | drop      | !as       |           |

The opcode map is optimized for LUT4 implementation. opcode[5:3] selects from a 7:1 mux (column).
opcode[2:0] selects the row within the column, sometimes with some decoding.

Group 1: 32-bit Adder/Subtractor

Group 2: Level 1 is 2:1 mux {RP,UP}, level 2 is 10-bit adder.

Group 3: Level 1 is 10-bit adder and mux select decode, level 2 is 2:1 mux {sum, N}.

Group 4: Level 1 is logic {and,xor,or,invert}, level 2 is 4:1 mux: {logic, port, imm, R}.

Group 5: Levels 1&2 are T's zero test (2-bit result) and mux select decode, level 3 is 2:1 mux {0=, N}.

Group 6: 4:1 mux: {user, 2/, sp, carry}

Group 7: Memory read result

```
!+  ( n a -- a+4 )  T = T+4,  N = mem
@+  ( a -- a+4 n )  T = mem,  N = T+4
@   ( a -- n )      T = mem
```


### Sample usage
- Local fetch: Lit_Offset RP @
- User variable fetch: Lit_Uservar UP @
- User variable store:  Lit_Uservar UP !+ DROP
- ABS: -IF| INVERT 1+ |

Conditional skip instructions skip the remainder of the instruction group, which could be up to 5 slots. This eliminates the branch overhead for short IF THEN statements and allows for more complex combinations of conditional branches and calls. The syntax of the skip instructions uses vertical bars to delineate the opcodes that are intended to fit in one instruction group. The compiler will skip to the next group if there are insufficient slots to fit it, or complain if it’s too big.

The SP, RP, and UP instructions are used to address PICK, Local, and USER variables respectively. After loading the address into A using one of these, you can use any word/halfword/byte fetch or store opcode.

Jumps and calls use unsigned absolute addresses of width 2, 8, 14, 20, or 26 bits, corresponding to an addressable space of 16, 1K, 64K, 4M, or 256M bytes. Most applications will be under 64K bytes, leaving the first three slots available for other opcodes. Many calls will be into the “reptilian brain” part of the kernel, in the lower 1K bytes. That leaves four slots for other opcodes.

The RAM used by the CPU core is relatively small. To access more memory, you would connect the AXI4 bus to other memory types such as single-port SRAM or a DRAM controller. Burst transfers use a !AS or @BS instruction to issue the address (with burst length=R) and stream that many words to/from external memory. Code is fetched from the AXI4 bus when outside of the internal ROM space. Depending on the implementation, AXI has excess latency to contend with. This doesn’t matter if most time is spent in internal ROM.

In a hardware implementation, the instruction group provides natural protection of atomic operations from interrupts, since the ISR is held off until the group is finished. A nice way of handling interrupts in a Forth system, since calls and returns are so frequent, is to redirect return instructions to take an interrupt-hardware-generated address instead of popping the PC from the return stack. This is a great benefit in hardware verification, as verifying asynchronous interrupts is much more involved. As a case in point, the RTX2000 had an interrupt bug.

A typical Forth kernel will have a number of sequential calls, which take four bytes per call. This is a little bulky, especially if the equivalent macro can fit in a group. The call and return overhead is eight clock cycles, so it’s also slow. Using the macro sequence should replace the call when possible. Code that’s inlineable is copied directly except for its ;, leaving that slot open for the next instruction.

;| doesn’t flush the IR, so the remaining slots are allowed to execute while the branch is in progress. It takes 4 cycles to complete a branch, so that many slots may be used. The compiler will enforce this limit so that the CPU doesn’t have to deal with exiting too early.

Hardware multiply and divide, if provided, are accessed via the USER instruction.
