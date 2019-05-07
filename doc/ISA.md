# MachineForth ISA

The MachineForth paradigm, discovered by Chuck Moore and Jeff Fox,
is a method for programming Forth stack machines in the most direct way possible,
by making machine opcodes a part of the high level language.
The programmer is writing to an actual machine rather than a virtual one.
It’s a different way of programming.
With the right ISA, optimizations are minimal so this virtualization is unnecessary.
An ISA for a dialect of MachineForth is presented below.

## Comparisons with other Forth CPUs

James Bowman's J1 and J1B. The J1 is about 10 years old. J1B is the 32-bit version.
16-bit instructions lead to compact code. It's a zero-operand WISC with 16-bit instructions.
Very small, very awesome. I didn't copy it because:

- Small address range. 13-bit calls and jumps, cover a 16KB address space.
- Stacks are not in memory, so multitasking is limited.
- It simulates slowly. I wanted a sandbox that would run on ARM.

Richard James Howe's H2 is a J1-like CPU. It has a Forth implementation.

Charles Ting's eP32 CPU. Uses 6-bit opcodes packed in a 32-bit word.

- Stacks are not in memory, so multitasking is not as tidy.
- Design and ISA details are behind a paywall.
- Has some more powerful instructions to support multiply and divide.

Brad Eckert's SC20. Uses 8-bit opcodes packed into 16-bit, 32-bit, or 48-bit instruction groups.
I wrote that one in 2010 and ported SwiftForth to it.

- Stacks have small caches to allow conventional Forth multitasking.
- The ISA has a CISC look and feel. Code is very compact.
- With the stack cache, simulation of the ISA using C or assembly wasn't fast.

Mforth (this one)

- 26-bit jumps/calls support 256MB apps.
- 24-bit xts in header space span 64MB.
- 6-bit opcodes packed in a 32-bit word.
- Compatible with VM sandboxes in conventional computers.
- Not eForth. Supports multiple search orders and wordlists.
- Designed for arbitrarily large apps residing in and compiling to flash memory.
- Built around synchronous Block RAM. Which has some minor drawbacks:
- Keeping stacks in data memory increases power consumption, but FPGAs are power hogs anyway.
- Synchronous RAM means reads have to be started even earlier so sometimes wait states must be inserted.

## The Virtual Machine

The files `vm.c` and `vmUser.c` implement the virtual machine on any C platform.
The `VMstep` function accepts a 32-bit instruction group, executes it, and returns the value of the PC.
This model makes for easier VM development.
Automated test vector generation creates a testbench that can be used to test versions coded in a language other than C.
Typically this means assembly. The assembly function is called from C for inclusion in the testbench.
The testbench can run on a small MCU, listing errors so that you can go back and see which opcode had problems.
It can also run in a VHDL testbench to verify hardware versions of the ISA.

The VM uses a 64-way jump to execute one of (up to) 64 stack instructions.
Most of them are 6-bit instructions. Some, such as CALL and JMP, use the rest of the instruction group as data.
The VM can be coded in assembly. The overhead added by the VM causes Forth code to run slower than native machine code.
Perhaps a 1:10 speed difference. This is where integration with C comes into play. Any bottlenecks can be cleared that way.
Also note that modern software routinely pisses away 2 or 3 orders of magnitude of CPU performance,
so it's surprising how much inefficiency you can actually get away with.

The VM, when implemented software, is a sandbox. No amount of abose will crash the host. You can only hang the VM.
The `main.c` app for a simple "hello world" app is:

```
#include <stdio.h>
#include "vm.h"

uint32_t FetchROM(uint32_t addr);

int main() {
    uint32_t PC = 0;
    VMpor();
    while (1) {
        uint32_t IR = FetchROM(PC);
        PC = VMstep(IR, 0);
    }
}
```

## ISA Licensing

Microprocessor companies have a depressing habit of asserting IP rights on ISAs.
An ISA is the backend for a tool ecosystem that CPU companies had very little hand in creating.
It's like the auto makers owning the roads.
Closed ISAs hold back the industry. Fortunately, RISCV is helping to de-incentivize them.
Therefore the MachineForth ISA presented here (I'll call it Mforth),
is completely free and open. You get to do whatever you want with it.
The specification is `vm.c` but is summarized below.

## The ISA
The stack machine used by MachineForth is based on small, zero-operand opcodes operating on the tops of stacks and corresponding to Forth names. This is different from Novix-style stack machines, which use wide instructions equivalent to stack machine microcode executing in one cycle. While this tends to be efficient, it requires virtualization of the language just like any other machine. When it comes to executing an application in a modern stack machine, instruction execution time is even more meaningless than it classically has been. When your computer consists of Verilog or VHDL code, the compute-intensive parts of the application are in hardware. RTL is the new machine code. The compiler is simple enough that you can trivially add custom opcodes.

MISC computers are minimal instruction set computers. They were pioneered by Chuck Moore, Jeff Fox, and GreenArrays using an asynchronous design flow. Since industry design flows are based on synchronous logic, a practical stack machine should use leverage synchronous memories. This affects the ISA. With synchronous memories, you need to start a read a cycle before it’s needed. This forces an extra pipeline stage, but also affords more sophisticated decoding.

The MISC paradigm executes small instructions very fast, from an instruction group. The instruction group is fetched from memory, and then the opcodes are executed in sequence within the group. An opcode can conditionally loop or skip the sequence, removing the need to change the PC to execute a loop or a short conditional. It has a Shboom-like feel, although not as sophisticated.

One way that this ISA deviates from MISC is that MISC is more focused on power consumption. You can burn power by accessing either a register file or a RAM. MISC uses stacks instead, where the stacks are implemented using bidirectional shift register logic. Maybe I'm being a little naughty, but a single block RAM is used for stacks and fast user variables. It follows the Forth model from the 1970s, where stacks in RAM are accessible by the app. What are the odds that I'll need a Forth CPU in silicon that minimizes power consumption in the way MISC does?

An instruction word may be any number of bits. 16, 18 and 32 are popular sizes. Optimal semantic density is in the 20 bit range due to more slots being discarded at higher widths. This affects only the size, not the speed. Since the intended hardware talks to an AXI bus, the chosen word size is 32-bit to match AXI's minimum bus width. The AXI bus is used for I/O. @AS and !AS access it.

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
| 5    | 1:0   | 0 to 3  |

Slot 5 only fits 4 opcodes. Immediate data is registered, so there is time to sign it.
Immediate data is taken from the remainder of the IR. IMM = IR[?:1]; IR[0]: sign of data, 1s complement if '1'.
The number of bits depends on the slot position or the opcode. It can be 26, 20, 14, 8, or 2 bits.

### Opcodes

There are a few rules regarding opcode numbering. They are:

1. nop must be 0.
2. call must be convertible to jump by clearing a bit. Preferably bit 3.
3. the most frequently used opcodes should be opcodes 1 thru 3: `EXIT`, `DUP`, `+`.

In the following table:

- *opcode skips or loops the slots*
- **opcode uses the rest of the slots as unsigned immediate data**
- Pops and reads (columns 2, 3, 6, 7) are delayed if a write is in progress to enable the use of single-port RAM.

| 5:3\2:0 | 0         | 1          | 2         | 3         | 4         | 5         | 6         | 7         |
|:-------:|:---------:|:----------:|:---------:|:---------:|:---------:|:---------:|:---------:|:---------:|
| **0**   | nop       | dup        | exit      | +         | 2\*       |           |           |           |
| **1**   | *no:*     | 1+         | r>        |           | 2\*c      | **user**  | c@+       | c!+       |
| **2**   |           | rp         | r@        | and       | 2/        | **jmp**   | w@+       | w!+       |
| **3**   |           | sp         |           | xor       | u2/       | **call**  | w@        | >r        |
| **4**   | *reptc*   | 4+         |           | c+        | 0=        | **litx**  | @+        | !+        |
| **5**   | *-rept*   | up         |           |           | 0<        | **@as**   | @         | rp!       |
| **6**   | *-if:*    | port       |           |           | invert    | **!as**   | c@        | sp!       |
| **7**   | *ifc:*    | over       | *ifz:*    | drop      | swap      | **lit**   |           | up!       |

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
- `c+`    ( n m -- n+m ) carry in and carry out
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
- `invert`( n -- ~n ) Displays as `com`

Control Flow
- `call`  ( R: -- PC ) PC = Imm.
- `exit`  ( R: PC -- ) Pop PC from return stack.
- `ifz:`  ( flag -- ) Skip the rest of the slots if flag <> 0.
- `jmp`   ( -- ) Jump: PC = Imm.
- `no:`   ( -- ) Skip the rest of the slots. Displays as `_`
- `reptc` ( -- ) Go back to slot 0 if carry=0; N=N+1.
- `-rept` ( n ? -- n' ? ) Go back to slot 0 if N<0; N=N+1.
- `-if:`  ( n -- n ) Skip remaining slots if T>=0.

Note: `nop` must immediately follow a `-rept` or `reptc`.

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
- `sp!`   ( a -- ? ) Does not pop the stack.
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

Conditional skip instructions skip the remainder of the instruction group, which could be up to 5 slots.
This eliminates the branch overhead for short IF THEN statements and allows for more complex combinations of conditional branches and calls.
The syntax of the skip instructions uses vertical bars to delineate the opcodes that are intended to fit in one instruction group.
The compiler will skip to the next group if there are insufficient slots to fit it, or complain if it’s too big.

The SP, RP, and UP instructions are used to address PICK, Local, and USER variables respectively. After loading the address into A using one of these, you can use any word/halfword/byte fetch or store opcode.

Jumps and calls use unsigned absolute addresses of width 2, 8, 14, 20, or 26 bits, corresponding to an addressable space of 16, 1K, 64K, 4M, or 256M bytes.
Most applications will be under 64K bytes, leaving the first three slots available for other opcodes.
Many calls will be into the the kernel, in the lower 1K bytes.
That leaves four slots for other opcodes.

The RAM used by the CPU core is relatively small.
To access more memory, you would connect the AXI4 bus to other memory types such as single-port SRAM or a DRAM controller.
Burst transfers use a !AS or @AS instruction to issue the address (with burst length=Imm) and stream words to/from external memory.

RAM has a negative address ANDed with (RAMsize-1) where RAMsize is an exact power of 2.
In `mf` it sits at the top of the memory space.
Negative literals are formed by putting a COM opcode in slot 0. It's pretty good.
I thought about having signed literals, but it adds logic. So, no.

A typical Forth kernel will have a number of sequential calls, which take four bytes per call.
This is a little bulky, especially if the equivalent macro can fit in a group.
The call and return overhead is eight clock cycles, so it’s also slow.
Using the macro sequence should replace the call when possible.
Code that’s inlineable is copied directly except for its `;`, leaving that slot open for the next instruction.

Hardware multiply and divide, if provided, are accessed via the USER instruction. `um*` takes about 500 cycles when done by shift-and-add. An opcode could easily be added to greatly speed this up.

### Interrupts

In a hardware implementation, the instruction group provides natural protection of atomic operations from interrupts,
since the ISR is held off until the group is finished.
There is an error interrupt: When an error occurs, the error number is written to the `port` register,
the PC is pushed to the stack, and PC is set to 2.
The normal Forth practice is to have ISRs to the time-critical stuff and then wake up a high level task to take care of cleanup.
Moving this paradigm into SOC hardware, hardware does the time critical stuff and the interrupt is replaced by
something to wake up the high level cleanup task.

Forth's round-robin multitasker would need modification to fit this:
How does hardware know where to put WAKE and PASS tokens, and what they are?
The CPU can keep STATUS variables at the bottom of RAM and load them based on wire signals.
PASS and WAKE may be compiled at known ROM addresses.
The STATUS variable would be a pointer to the status that gets altered by hardware.
So essentially, there is no need for interrupts.
This greatly simplifies verification. Also, you don't have to worry about microloop latency.

