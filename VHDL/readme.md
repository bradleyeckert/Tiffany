# **PRELIMINARY**

## M32 ports

The M32 CPU uses inferred block RAM for its internal code and data spaces. External code/data space is read-only. A flash memory controller will supply the data.
Programming the flash is via a SPI control word. Port groups include:

- Clock and reset
- Code memory read port
- SPI control port
- Byte stream in (KEY, KEY?)
- Byte stream out (EMIT, EMIT?)
- AXI4 busses, for streaming via `!as` and `@as`

### Clock and reset

| name    | I/O | width | usage                            |
|:--------|:---:|:-----:|:---------------------------------|
| clk     | in  | 1     | System clock                     |
| reset   | in  | 1     | Active high, synchronous reset   |

### Flash read port

| name    | I/O | width | usage                            |
|:--------|:---:|:-----:|:---------------------------------|
| caddr   | out | 24    | Code memory address              |
| cready  | in  | 1     | Code memory data ready           |
| cdata   | in  | 32    | Code memory read data            |

The external memory controller will monitor for changes in `caddr` on each cycle.
It outputs `cready` along with valid data.
If the M32 changes the flash read address, the following cycle it may check `cready`.

### SPI control port

SPIxfer is done by bit-bang so there's no SPI hardware. I2C is included.
The T[0] is the value to write to the bit selected by T[3:1].
Data is returned in T[0] is the line selected by T[2:1].

| name    | I/O | width | usage                  |
|:--------|:---:|:-----:|:----------------------:|
| bbout   | out | 8     | Bit-banged output      |
| bbin    | in  | 8     | Bit-banged input       |

This is tentative. SPI is simple enough that it may be done in hardware.
Some FPGAs have hard SPI/I2C. Could leverage that. 
But the bit-bang feature should be kept anyway.

### Byte stream in

| name    | I/O | width | usage                            |
|:--------|:---:|:-----:|:---------------------------------|
| kreq    | out | 1     | KEY request                      |
| kdata_i | in  | 8     | KEY input data                   |
| kack    | in  | 1     | KEY is finished                  |

External hardware is responsible for flow control, etc.
For a UART, CTS and RTS. For a FTDI FIFO, the normal handshake lines.

### Byte stream out

| name    | I/O | width | usage                            |
|:--------|:---:|:-----:|:---------------------------------|
| ereq    | out | 1     | EMIT request                     |
| edata_o | out | 8     | EMIT output data (UTF-8)         |
| eack    | in  | 1     | EMIT is finished                 |

### AXI4 busses

Basic streaming busses as described by Xilinx. To Be Implemented.

### Other

Interrupt request pins could be added. 
Not actually interrupts, but signals to jam WAKE tokens into memory to wake up tasks.

`BYE` produces an output signal that can end simulation or go into deep powerdown.

## Architecture

The internal RAM uses synchronous read/write, which means a read must be started the cycle before the data is needed.
A read from external ROM is a special case that latches the address and then cycles through a FSM to get the data.
The idea is to keep external ROM out of the critical path.
Fetches and pops read from RAM. Stores and pushes write to RAM.

The opcode decoder for initiating reads also latches control lines that control the next pipeline stage.
It may delay reading (stalling slot execution) to allow necessary writes or other activity.
A `state` signal controls the behavior of the opcode decoder.

Unnecessary memory accesses should probably be looked at.
Adjacent DUP DROP pairs, for instance, cause delays to be inserted for the sake of waiting for data to be written.
There should be a better option than waiting.

## Usage

So far, in simulation the ANS example uses a testbench to configure synthesizable components into
something that simulates keyboard input, runs the CPU, and directs output to the console.
"see see" disassembles 'see' to the console.

The ANS example (make.bat) compiles a large ROM. 
The testbench "types" "see see" into the Forth interpreter.
ModelSIM chugs along disassembling one line per second in real time.
Most of the time is spent in UM/MOD, which is done with shift-and-add instructions.
Speeding it up in hardware is somewhat trivial but is beside the point.
Grinding it out exercises several opcodes. Give it 32ms of simulation time to finish.

Although you could use the ANS Forth's synthesizable ROM in a real system,
a 32KB ROM is a lot of memory in an FPGA. 
It would be better to save the ROM image as a hex file and flash it into a SPI flash chip.
Although you could execute exclusively from flash, it would be better to cache the bottom 2K or 4K bytes in a block RAM to run faster.

