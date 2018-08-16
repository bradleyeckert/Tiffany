# MachineForth ISA

The MachineForth paradigm, discovered by the late Jeff Fox, is a method for programming Forth stack machines in the most direct way possible, by making machine opcodes a part of the high level language. The programmer is writing to an actual machine rather than a virtual one. It’s a different way of programming. With the right ISA, optimizations are minimal so this virtualization is unnecessary. An ISA for a dialect of MachineForth is presented.

## The ISA
The stack machine used by MachineForth is based on small, zero-operand opcodes operating on the tops of stacks and corresponding to Forth names. This is different from Novix-style stack machines, which use wide instructions equivalent to stack machine microcode executing in one cycle. While this tends to be efficient, it requires virtualization of the language just like any other machine. When it comes to executing an application in a modern stack machine, instruction execution time is even more meaningless than it classically has been. When your computer consists of Verilog or VHDL code, the compute-intensive parts of the application are in hardware. RTL is the new machine code. The compiler is simple enough that you can trivially add custom opcodes.

MISC computers are minimal instruction set computers. They were pioneered by Chuck Moore, Jeff Fox, and GreenArrays using an asynchronous design flow. Since industry design flows are based on synchronous logic, a practical stack machine should use leverage synchronous memories. This affects the ISA. With synchronous memories, you need to start a read a cycle before it’s needed. This forces an extra pipeline stage, but also affords more sophisticated decoding.

The MISC paradigm executes small instructions very fast, from an instruction group. The instruction group is fetched from memory, and then the opcodes are executed in sequence within the group. An opcode can conditionally loop or skip the sequence, removing the need to change the PC to execute a loop or a short conditional.

An instruction word may be any number of bits. 16, 18 and 32 are popular sizes. Optimal semantic density is in the 20 bit range due to more slots being discarded at higher widths. This affects only the size, not the speed. Since the desired hardware talks to an AXI bus, the chosen word size is 32-bit to match AXI's minimum bus width.

The model's instruction size affects the ISA as well as the low level implementation, so to be the most useful we bet on a single horse: 32-bit. Word size affects how much memory you can address. In an embedded system, even a cheap one, there can be megabytes of data. Even phone apps weigh in at 10MB for a small one. Yes, that's ridiculous. However, supposing a CPU should be able to address that, a word size of at least 24 bits would be needed. Large SPI flash needs even more address bits, and keep in mind you need to add another address bit every two years. So, 32-bit is just about optimal for "small systems".

### Instruction Group
The ISA uses 6-bit opcodes in a 32-bit instruction group. Opcodes that use immediate data take that data from the remainder of the instruction group. A slot pointer steps through the opcodes in sequence. It can be conditionally reset to cause a loop, or set to the end to skip the rest of the opcodes in the group.

| Slot | Bits  | Range   |
|:----:|:-----:| -------:|
| 0    | 31:26 | 0 to 63 |
| 1    | 25:20 | 0 to 63 |
| 2    | 19:14 | 0 to 63 |
| 3    | 13:8  | 0 to 63 |
| 4    | 7:2   | 0 to 63 |
| 5    | 1:0   | 0 to 3 |

Slot 5 only fits 4 opcodes. Immediate data is registered, so there is time to sign it.
Immediate data is taken from the remainder of the IR. IMM = IR[?:1]; IR[0]: sign of data, 1s complement if '1'.
The number of bits depends on the slot position or the opcode. It can be 26, 20, 14, 8, or 2 bits.

ALU operations take their operands from registers for minimum input delay. Since the RAM is synchronous read/write, the opcode must be pre-decoded. The pre-decoder initiates reads. The main decoder has a registered opcode to work with, so the decode delay isn’t so bad. The pre-read stage of the pipeline allows time for immediate data to be registered, so the execute stage sees no delay. Opcodes have time to add the immediate data to registers, for more complex operations. One can index into the stack, for example.

Preliminary opcodes:

- *opcode conditionally skips the rest of the slots*
- **opcode uses the rest of the slots as signed immediate data**

|         | 0         | 1          | 2         | 3         | 4         | 5         | 6         | 7         |
|:-------:|:---------:|:----------:|:---------:|:---------:|:---------:|:---------:|:---------:|:---------:|
| **0**   | nop       | dup        | exit      | +         | user      | 0<        | r>        | 2/        |
| **1**   | ifc:      | 1+         | swap      | -         |           | c!+       | c@+       | u2/       |
| **2**   | no:       | 2+         | ifz:      | **jmp**   |           | w!+       | w@+       | and       |
| **3**   |           | **litx**   | >r        | **call**  |           | 0=        | w@        | xor       |
| **4**   | rept      | 4+         | over      | c+        |           | !+        | @+        | 2\*       |
| **5**   | -rept     |            | rp        | drop      |           | rp!       | @         | 2\*c      |
| **6**   | -if:      |            | sp        | **@as**   |           | sp!       | c@        | port      |
| **7**   | +if:      | **lit**    | up        | **!as**   |           | up!       | r@        | invert    |
| *mux*   | *none*    | *T+offset* | *XP / N*  | *N +/- T* | *user*    | *0= / N*  | *mem bus* | *logic*   |

The opcode map is optimized for LUT4 implementation. opcode[2:0] selects T from a 7:1 mux (column).
opcode[5:3] selects the row within the column, sometimes with some decoding.
There are 2 or 3 levels of logic between registers and the T mux.

Group 1: 32-bit Adder/Subtractor

Group 2: Level 1 is 10-bit adder and mux-select decode, level 2 is 2:1 mux {sum, imm}.

Group 3: Levels 1&2 are a 4:1 mux {RP,SP,UP,N}.

Group 4: User.

Group 5: Level 1 is logic {and,xor,or,invert} and mux-select decode, level 2 is 4:1 mux: {logic, port, 2/, 2*}.

Group 6: Levels 1&2 are T's zero test (2-bit result) and mux select decode, level 3 is 2:1 mux {0=, N}.

Group 7: Memory read result

```
!+  ( n a -- a+4 )  T = T+4,  N = mem
@+  ( a -- a+4 n )  T = mem,  N = T+4
@   ( a -- n )      T = mem
```

Branches compile a conditional skip past `jmp`.

- `if` compiles `ifz: jmp`. Jump if T=0.
- `+if` compiles `-if: jmp`. Jump if T<0. Does not consume top of stack.
- `ifnc` compiles `ifc: jmp`. Jump if carry flag is set. Does not consume top of stack.

`begin ... +until` is handy for loops. Negate the loop count before `begin`.

### Summary

Basic stack
- `nop`   ( -- ) Displays as `.`
- `drop`  ( x -- )
- `dup`   ( x -- x x )
- `over`  ( n m -- n m n )
- `swap`  ( n m -- m n )
- `r@`    ( -- x )
- `r>`    ( -- x ) ( R: x -- )
- `>r`    ( x -- ) ( R: -- x )
- `lit`   ( -- Imm )
- `litx`  ( x -- x<<24 + Imm )

ALU
- `+`     ( n m -- n+m ) carry out
- `-`     ( n m -- n+m ) carry out
- `c+`    ( n m -- n+m ) carry in and carry out
- `c-`    ( n m -- n+m ) carry in and carry out
- `1+`    ( n -- n+1 )
- `2+`    ( n -- n+2 )
- `cell+` ( n -- n+4 )
- `2/`    ( n -- m ) carry out
- `u2/`   ( n -- m ) carry out
- `2*`    ( n -- m ) carry out
- `2*c`   ( n -- m ) carry in and carry out
- `0=`    ( n -- flag )
- `0<`    ( n -- flag )
- `and`   ( n m -- n&m )
- `xor`   ( n m -- n^m )
- `invert`( n -- ~n )

Control Flow
- `call`  ( R: -- PC ) PC = Imm.
- `exit`  ( R: PC -- ) Pop PC from return stack.
- `ifz:`  ( flag -- ) Skip the rest of the slots if flag <> 0.
- `jmp`   ( -- ) Jump: PC = Imm.
- `no:`   ( -- ) Skip the rest of the slots. Displays as `_`
- `rept`  ( -- ) Go back to slot 0.
- `-rept` ( n ? -- n' ? ) Go back to slot 0 if N<0; N=N+1.
- `-if:`  ( n -- n ) Skip remaining slots if T>=0.
- `+if:`  ( n -- n ) Skip remaining slots if T<0.

Memory
- `!+`    ( n a -- a+4 ) 32-bit
- `@+`    ( a -- a+4 n )
- `@`     ( a -- n )
- `w!+`   ( n a -- a+2 ) 16-bit
- `w@+`   ( a -- a+2 n )
- `w@`    ( a -- n )
- `c!+`   ( n a -- a+1 ) 8-bit
- `c@+`   ( a -- a+1 n )
- `c@`    ( a -- c )
- `rp`    ( n -- a+n ) Return stack pointer
- `sp`    ( n -- a+n ) Data stack pointer
- `up`    ( n -- a+n ) User pointer
- `rp!`   ( a -- )
- `sp!`   ( a -- )
- `up!`   ( a -- )

Interface
- `user`  ( n1 n2 -- n1 n3 ) User function selected by Imm.
- `@as`   ( asrc adest -- asrc adest ) Imm = burst length.
- `!as`   ( asrc adest -- asrc adest ) Imm = burst length.
- `port`  ( n -- m ) Used for debugging access.

### Sample usage
- Local fetch: lit rp @
- User variable fetch: lit up @
- User variable store: lit up !+ drop
- ABS: |-if invert 1+ |

Conditional skip instructions skip the remainder of the instruction group, which could be up to 5 slots. This eliminates the branch overhead for short IF THEN statements and allows for more complex combinations of conditional branches and calls. The syntax of the skip instructions uses vertical bars to delineate the opcodes that are intended to fit in one instruction group. The compiler will skip to the next group if there are insufficient slots to fit it, or complain if it’s too big.

The SP, RP, and UP instructions are used to address PICK, Local, and USER variables respectively. After loading the address into A using one of these, you can use any word/halfword/byte fetch or store opcode.

Jumps and calls use unsigned absolute addresses of width 2, 8, 14, 20, or 26 bits, corresponding to an addressable space of 16, 1K, 64K, 4M, or 256M bytes. Most applications will be under 64K bytes, leaving the first three slots available for other opcodes. Many calls will be into the the kernel, in the lower 1K bytes. That leaves four slots for other opcodes.

The RAM used by the CPU core is relatively small. To access more memory, you would connect the AXI4 bus to other memory types such as single-port SRAM or a DRAM controller. Burst transfers use a !AS or @AS instruction to issue the address (with burst length=Imm) and stream that many words to/from external memory. Code is fetched from the AXI4 bus when outside of the internal ROM space. Depending on the implementation, AXI has excess latency to contend with. This doesn’t matter if most time is spent in internal ROM.

In a hardware implementation, the instruction group provides natural protection of atomic operations from interrupts, since the ISR is held off until the group is finished. A nice way of handling interrupts in a Forth system, since calls and returns are so frequent, is to redirect return instructions to take an interrupt-hardware-generated address instead of popping the PC from the return stack. This is a great benefit in hardware verification, as verifying asynchronous interrupts is much more involved. As a case in point, the RTX2000 had an interrupt bug.

A typical Forth kernel will have a number of sequential calls, which take four bytes per call. This is a little bulky, especially if the equivalent macro can fit in a group. The call and return overhead is eight clock cycles, so it’s also slow. Using the macro sequence should replace the call when possible. Code that’s inlineable is copied directly except for its `;`, leaving that slot open for the next instruction.

Hardware multiply and divide, if provided, are accessed via the USER instruction. `um*` takes about 500 cycles when done by shift-and-add.
