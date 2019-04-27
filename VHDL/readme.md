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

|:--------|:---:|:--:|:---------------------------------|
| clk     | in  | 1  | System clock                     |
| reset   | in  | 1  | Active high, synchronous reset   |

### Flash read port

|:--------|:---:|:--:|:---------------------------------|
| caddr   | out | 24 | Code memory address              |
| cready  | in  | 1  | Code memory data ready           |
| cdata   | in  | 32 | Code memory read data            |

The external memory controller will monitor for changes in `caddr` on each cycle.
It outputs `cready` along with valid data.
If the M32 changes the flash read address, the following cycle it may check `cready`.

### SPI control port

SPIxfer is done by bit-bang so there's no SPI hardware. I2C is included.
The T[0] is the value to write to the bit selected by T[3:1].
Data is returned in T[0] is the line selected by T[2:1].

|:--------|:---:|:-:|:----------------------:|
| bbout   | out | 8 | Bit-banged output      |
| bbin    | in  | 8 | Bit-banged input       |

### Byte stream in

|:--------|:---:|:--:|:----------------------------------|
| kreq    | out | 1  | KEY request                       |
| kdata_i | in  | 8  | KEY input data                    |
| kack    | in  | 1  | KEY is finished                   |

External hardware is responsible for flow control, etc.
For a UART, CTS and RTS. For a FTDI FIFO, the normal handshake lines.

### Byte stream out

|:--------|:---:|:--:|:----------------------------------|
| ereq    | out | 1  | EMIT request                      |
| edata_o | out | 8  | EMIT output data (UTF-8)          |
| eack    | in  | 1  | EMIT is finished                  |

### AXI4 busses

Basic streaming busses as described by Xilinx.

## Architecture

The internal RAM uses synchronous read/write, which means a read must be started the cycle before the data is needed.
A read from external ROM is a special case that latches the address and then cycles through a FSM to get the data.
The idea is to keep external ROM out of the critical path.
Fetches and pops read from RAM. Stores and pushes write to RAM.
Some opcodes want to read and write simultaneously, but not to the same address.

The opcode decoder for initiating reads also latches control lines that control the next pipeline stage.
It may delay reading (stalling slot execution) to allow necessary writes or other activity.
A `state' signal controls the behavior of the opcode decoder.


