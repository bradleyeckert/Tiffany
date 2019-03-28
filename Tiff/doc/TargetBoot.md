# Target Boot

Tiff builds a VM model that can be exported to run on anything. The model includes a ROM image that initializes RAM to match the contents of the RAM in Tiff at the time of export.

## RAM initialization

Tiff RAM starts off blank and gets sparsely populated with various runs of nonzero values. The target initialization code uses a data structure that's compatible with this layout. It uses a sequence of cell-based chunks terminated with -1. The chunk format is:

| Cell | Usage   | Description                                  |
|:----:|:--------|:---------------------------------------------|
| 0    | Address | Packed n and address                         |
| 1..n | Data    | 1 to 256 cells                               |

## Initial Values

The PC in the target VM starts at 0. The first cell of ROM is expected to jump to initialization code.
The SP register starts out at a usable address such that the stack can be used without clobbering anything.

## Safe Mode

Safe mode should be able to write to SPI flash without running the application.
One way to handle it is to have a time delay before launching the app.
The delay isn't a problem in battery powered devices since hard reset is rare.
Code stays within internal ROM until after the time-out.

The protocol only needs to re-flash the application.
It could test the internal ROM build number to make sure it matches the app.
One could send an Intel HEX file in raw text format:
An incoming line is delivered by ACCEPT (line-at-a-time) and then interpreted.
Addresses on a 4K boundary trigger a sector erase and 16-byte write.
The 70ms typical 4K-page erase time is the limiting factor:
The UART should support XON/XOFF flow control.
Programming a 4K page involves the 70ms erase time and 40ms data transfer at 3M BPS.
A cheap FTDI USB adapter can handle that, allowing a 1MB update in 30 seconds.
Time-out is easy: If there's no ':' within a short time after POR, the app launches.
The app should be protected by length and checksum. No match, no launch.
If the app expects a wrong internal ROM build number, the appropriate error message should appear.
