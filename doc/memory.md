# Memory Model

The memory model uses 32-bit byte addresses in little endian format. Physical memory consists of:

- Internal ROM for code
- Internal dual-port or single-port RAM for data
- External flash memory
- External AXI space

Code ROM is synchronous-read ROM. Data RAM is dual-port synchronous RAM. In an ASIC, masked ROM is 1/10th to 1/20th the die area of dual-port RAM (per bit), so a decent amount of area is available for code. In an FPGA, you typically have 18Kb blocks of DPRAM, so 512-word chunks. The RAM needs byte lane enables. It can also be single-port, with a performance hit. The VM should alter clock cycle counts depending on the RAM type.

AXI is a streaming-style system interface where data is best transferred in bursts due to long latency times. It's the standard industry interface. Rather than abstracting it away, the ISA gives direct control to the programmer. Two opcodes, `!as` and `@as` are multi-cycle instructions that stream RAM data to and from the AXI bus.

The rationale behind the AXI bus is to put all of the user's custom memory space there so as not to slow down the main memory. Peripherals live in AXI space.
The AXI's address range is 32-bit, starting at 0. You can't access it with `@` and `!`. Instead, you would use `@as` and `!as`. DRAM would be on the AXI bus. An app that uses DRAM would be written to page data in and out of DRAM instead of using random access without forethought.

## Word Size

Cells should have enough bits to address a SPI flash using byte addressing. The biggest SPI NOR flash Digikey has in stock as of mid 2018 is 128M bytes, so a 27-bit address range. 32-bit memory words are then a no-brainer. That's compatible with commercial Forth systems, which are also 32-bit. Byte order is little-endian.

## Address Ranges

In `mf`, #defines in config.h specify the default and maximum sizes (in 32-bit words) of RAM and ROM. They can be changed from the command line.

| Type  | Range                        |
|:-----:|:----------------------------:|
| ROM   | 0 to ROMsize-1               |
| RAM   | ROMsize to ROMsize+RAMsize-1 |
| Flash | Other                        |

`mf` simulates a blank flash in AXI space and applies the rule of never writing a '0' bit twice to the same bit without erasing it first. Such activity may over-charge the floating gate (if the architecture doesn't prevent it), leading to reliability problems. `mf` writes ROM data to both AXI space and internal ROM when simulating the "Load code RAM from SPI flash" boot method.

## Writing to SPI flash

While the SPI flash can be read with `@`, writing it is another matter. One of the user functions is `flashxfer`, which performs a byte transfer over the flash's SPI bus. Programming code runs from internal ROM. This works because most flash chips have a common set of commands. The SPI format may be single, dual, or quad. It takes 12 bits to control a `flashxfer` byte transfer:

| Bits   | Range                        |
|:------:|:----------------------------:|
| 11:10  | Serial bus width: 1, 2, 4, - |
| 9      | Pre-transfer /CS state       |
| 8      | Post-transfer /CS state      |
| 7:0    | Data to flash                |

Data returned by `flashxfer` is 8-bit.

In an MCU implementation where `flashxfer` operates on internal flash, it should implement a bare bones protocol simulating a SPI flash chip. `mf` uses this method to write to the flash memory model: see `flash.c`. The bare bones protocol uses the minimum command set:

| Name   | Hex | Command                           |
|:-------|-----|----------------------------------:|
| FR     | 0Bh | Read Data Bytes from Memory       |
| PP     | 02h | Page Program Data Bytes to Memory |
| SER    | 20h | Sector Erase 4KB                  |
| WREN   | 06h | Write Enable                      |
| WRDI   | 04h | Write Disable                     |
| RDSR   | 05h | Read Status Register              |
| RDJDID | 9Fh | Read 3-byte JEDEC ID              |

The RDJDID result is MFR, Type, Capacity. These have vendor-specific meanings, so only a lookup table can determine device specifics or capacity. Some popular devices in 2018 are:

| Name       | Mfr     | Mfr | Type | Cap |
|------------|---------|-----|------|-----|
| AT25SF041  | Adesto  | 1Fh | 84h  | 01h |
| AT25SF081  | Adesto  | 1Fh | 85h  | 01h |
| AT25SF161  | Adesto  | 1Fh | 86h  | 01h |
| W25X10CL   | Winbond | EFh | 30h  | 11h |
| W25Q80DV   | Winbond | EFh | 40h  | 14h |
| W25Q32JV   | Winbond | EFh | 40h  | 16h |
| IS25LP080D | ISSI    | 9Dh | 60h  | 14h |
| IS25WP080D | ISSI    | 9Dh | 70h  | 14h |
| IS25WP040D | ISSI    | 9Dh | 70h  | 13h |
| IS25WP020D | ISSI    | 9Dh | 70h  | 12h |
| *simulated* | *none* | AAh | n    | n   |

The simulated part has a capacity byte of `n` 64KB sectors. This scheme allows for a 4GB range.

Adesto and Winbond status register:
|bit|name|usage                      |
|---|----|---------------------------|
| 7 |SRP0|Status Register Protection |
| 6 |SEC |Block Protection           |
| 5 |TB  |Top or Bottom Protection   |
| 4 |BP2 |Block Protection bit-2     |
| 3 |BP1 |Block Protection bit-1     |
| 2 |BP0 |Block Protection bit-0     |
| 1 |WEL |Write Enable Latch Status  |
| 0 |BSY |0 = Device is ready, 1=busy|

ISSI status register:
|bit|name|usage                      |
|---|----|---------------------------|
| 7 |SRP0|Status Register Protection |
| 6 |*QE*|Quad rate enable           |
| 5 |*BP3*|Block Protection bit-3    |
| 4 |BP2 |Block Protection bit-2     |
| 3 |BP1 |Block Protection bit-1     |
| 2 |BP0 |Block Protection bit-0     |
| 1 |WEL |Write Enable Latch Status  |
| 0 |BSY |0 = Device is ready, 1=busy|
