# MCU models

The MCUs are synthesizable. V0 has a UART and built-in ROM. V1 has a SPI flash controller.

MCU V0:

- userfn.vhd, user functions
- spram32.vhd, data RAM
- m32.vhd, CPU
- uart.vhd, simple UART
- rom32.vhd, internal ROM (run .\examples\ANS\make.bat to generate)
- mcu_v0.vhd, MCU
- mcu_v0_tb.vhd, testbench

MCU V1:

- userfn.vhd, user functions
- spram32.vhd, data RAM
- m32.vhd, CPU
- uart.vhd, simple UART
- spramEZ.vhd, generic RAM
- spif.vhd, SPI flash interface
- sfprog.vhd, SPI flash programmer
- mcu_v1.vhd, MCU
- conversions.vhd, FMF library
- gen_utils.vhd, FMF library
- s25fl064l.vhd, FMF model
- mcu_v1_tb.vhd, testbench
- testrom.txt, internal ROM (run .\examples\ANS\make.bat to generate)

## M32 ports

The M32 CPU uses inferred block RAM for its internal code and data spaces.
External code is read-only. A flash memory controller should supply the data.
Programming the flash is via a SPI control word. Port groups include:

- Clock and reset
- Code memory read port
- Peripherals port
- AXI4 busses, for streaming via `!as` and `@as` (not implemented)

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

### Dumb Peripheral Bus

Similar to ARM's APB but a little looser and dumber.

| name    | I/O | width | usage                            |
|:--------|:---:|:-----:|:--------------------------------:|
| paddr   | out	| 9     | address                          |
| pwrite  | out | 1     | write strobe                     |
| psel    | out | 1     | start the cycle                  |
| penable | out | 1     | delayed psel                     |
| pwdata  | out | 16    | write data                       |
| prdata  | in  | 16    | read data                        |
| pready  | in  | 1     | ready to continue                |

### AXI4 busses

Basic streaming busses as described by Xilinx/ARM. To Be Implemented.
The idea is to interface to IP ecosystems in a standard kind of way.
The DPB bus is for simple stuff.

### Other

Interrupt request pins could be added.
Not actually interrupts, but signals to jam WAKE tokens into memory to wake up tasks.

`BYE` produces an output signal that can end simulation or put a SOC into deep powerdown.

## Architecture

The internal RAM uses synchronous read/write, which means a read must be started the cycle before the data is needed.
A read from external ROM is a special case that latches the address and then cycles through a FSM to get the data.
Fetches and pops read from RAM. Stores and pushes write to RAM.

The opcode decoder for initiating reads also latches control lines that control the next pipeline stage.
It may delay reading (holding slot execution) to allow necessary writes or other activity.
A `state` signal controls the behavior of the opcode decoder.

Unnecessary memory accesses should probably be looked at.
Adjacent DUP DROP pairs, for instance, cause delays to be inserted for the sake of waiting for data to be written.
There should be a better option than waiting. It doesn't look obvious.

The CPU is implemented as a FSM that spends most of its time in `execute`.
Other states handle user functions, non-cell fetches, control flow changes, etc.

### Hardware Multiply and Divide

The software-only versions of UM\* and UM/MOD take about 700 and 1800 cycles respectively.
At 100 MHz, that's 7 usec and 18 usec.
Maybe it's all you need unless there's a lot of number crunching going on.
But then, why isn't the FPGA fabric doing that?
The `options` are in `main.f`. To remove HW math, change the `userfn` architecture.

## Usage

So far, in simulation the ANS example uses a testbench to configure synthesizable components into
something that simulates keyboard input, runs the CPU, and directs output to the console.
"see see" disassembles 'see' to the console.

The ANS example (make.bat) compiles a large ROM.
The testbench "types" "see see" into the Forth interpreter.
ModelSIM chugs along disassembling about one line per second in real time (on a fast PC).
Using hardware UM\* and UM/MOD (options bit 0) doubles the speed.
You can run `make.bat` in the `helloworld` example to generate a smaller ROM.

Although you could use the ANS Forth's synthesizable ROM in a real system,
a 32KB ROM is a lot of memory in an FPGA.
It would be better to save the ROM image as a hex file and flash it into a SPI flash chip.
Although you could execute exclusively from flash,
it would be better to cache the bottom 2K or 4K bytes in a block RAM to run faster.

### Synthesis results for MAX 10

I synthesized the HelloWorld example into a 10M08SCE144C8G using Quartus Prime Lite.
V0 took 3506 LEs and had a Fmax of 115 MHz in the "slow 85C" model.
The synchronous-read code ROM synthesized using 1966 LEs, so the MCU without ROM would be 1540 LEs.
This MAX10 doesn't do initialized block RAM. You would initialize from user flash or SPI flash.
There was also a EBR "read during write" warning when inferring RAM in `spram32.vhd`.

MCU_V1 came out to 150 MHz on a 10CL006YE144C6G and 2693 LEs (46% of the part).

### Synthesis results for Cyclone V

The ANS example has bigger ROM and RAM. Synthesized into 5CEBA2F17C6 using Quartus Prime Lite.
There was no EBR warning this time.

- Without hardware multiply/divide: 708 ALMs, Fmax of 145 MHz in the "slow 85C" model.
- With hardware multiply/divide: 932 ALMs, Fmax of 131 MHz in the "slow 85C" model.

## Dhrystone MIPS

Why would you care? Benchmarks are completely meaningless.
Just as in the classic Forth world time-critical code is moved into assembly,
time-critical functions should be in hardware. Yup, you have to learn a little VHDL or Verilog.
Use `userfn.vhd` as an example for your own functions.


