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
