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

| Type  | Range                        |
| ------|:----------------------------:|
| Code  | 0 to ROMsize-1               |
| Data  | ROMsize to ROMsize+RAMsize-1 |
| Code  | > Data: Read-only from AXI   |     

AXI space starts at address 0. Tiff treats this as SPI flash. It's up to the implementation to write-protect the bottom of SPI flash so as to not be able to brick itself. The AXI address range of \[ROMsize to ROMsize+RAMsize-1\] is a section of SPI flash that's unreachable by normal memory opcodes. However, it can be streamed into RAM.

Tiff simulates a blank flash in AXI space and applies the rule of never writing a '0' bit twice to the same bit without erasing it first. Such activity may over-charge the floating gate (if the architecture doesn't prevent it), leading to reliability problems. 

## Streaming Operations

Read channel of AXI:

- AXI\[PC\] to the IR, one word, for extended code space fetch (not an opcode)
- AXI\[A\] to Code RAM for streaming in a code/data segment or initializing code space

Write channel of AXI:

- Code RAM to AXI\[A\] for streaming out a working buffer

Getting single words from the AXI read channel could take tens of cycles. However, since almost all time is spent in internal code space it doesn't matter. A little prefetch buffering would help things along. 

The AXI4 protocol allows for a burst size between 1 and 256 words. Bursts use 32-bit words.
