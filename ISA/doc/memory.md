# Memory Model

The memory model uses 32-bit byte addresses in little endian format. Physical memory consists of:

- Internal single-port RAM for code
- Internal dual-port RAM for data
- External AXI space

Code RAM is single-port synchronous RAM. Data RAM is dual-port synchronous RAM. In an ASIC, single-port is half the die area of dual-port, so a decent amount of area is available for code. In an FPGA, you typically have 18Kb blocks of DPRAM, so 512-word chunks. Both RAMs need byte lane enables.

AXI is a streaming-style system interface where data is best transferred in bursts due to long latency times. It's the standard industry interface. Rather than abstracting it away, the ISA gives direct control to the programmer. Several opcodes are multi-cycle instructions that stream RAM data to and from the AXI bus. Coming out of reset, the IR contains instructions to load the code RAM from AXI.

## Word Size

A 4KB (1K word) ROM will hold quite a bit of kernel. That would make the minimum PC width 10 bits. With 6-bit opcodes, the minimum instruction width is 16 bits. However, a much wider word is needed to address AXI space. For that, we allow 32 bits to make for convenient data storage in SPI flash. The biggest SPI NOR flash Digikey has in stock as of mid 2018 is 128M bytes, so a 27-bit address range.

## Address Ranges

In Tiff, #defines in config.h specify the sizes (in 32-bit words) of RAM and ROM as `RAMsize` and `ROMsize` respectively.

| Type | Range                        |
| -----|:----------------------------:|
| ROM  | 0 to ROMsize-1               |
| RAM  | ROMsize to ROMsize+RAMsize-1 |

AXI space starts at address 0. Tiff treats this as SPI flash. It's up to the implementation to write-protect the bottom of SPI flash so as to not be able to brick itself. Tiff simulates a blank flash and applies the rule of never writing a '0' bit twice to the same bit without erasing it first. Such activity may over-charge the floating gate (if the architecture doesn't prevent it), leading to reliability problems.

## External Code

When executing code from an external source, it's first streamed into the top of code RAM from AXI. Then, it's executed.


TBD: Handling address offsets to allow code compiled for large model to be moved and executed. Possibly apply the offset when PC is in the cache range. Then when the cache misses, what?






