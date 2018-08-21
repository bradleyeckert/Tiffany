\ SPI flash access through `5 user`.

\ In Tiff, a SPI flash chip is simulated in tiffUser.c.
\ Real hardware will have an actual SPI flash chip that uses the same commands.
\ This code must run in internal ROM:

: SPIxfer     \ command -- result
   5 user                               \ Flash, a-ah, king of the impossible
;
: SPIaddress  \ addr command final --   \ initiate a command with 24-bit address
   >r
   SPIxfer drop  hld dup >r ! r>        \ use a cell for extracting bytes
   count >r  count >r  c@               \ low med high
   SPIxfer drop                         \ assuming a little-endian model
   r> SPIxfer drop
   r> r> + SPIxfer drop
;
: SPIbyte  \ a -- a+1
   255 SPIxfer swap c!+
;
: SPI@  \ addr -- x
   11 0 SPIaddress  dup SPIxfer drop      \ command and dummy byte
   hld  SPIbyte SPIbyte SPIbyte SPIbyte drop
   511 SPIxfer drop                     \ end the command
   hld @
;
: SPISR  \ -- status
   5 SPIxfer drop                       \ read status register
   511 SPIxfer                          \ LSB is `busy`, bit1 is WEN
;
: SPIwait  \ --
   begin  SPISR 1 and 0=  until
;
: SPI!  \ n addr --
   262 SPIxfer drop                     \ WREN
   2 0 SPIaddress  hld tuck !
   count SPIxfer drop
   count SPIxfer drop
   count SPIxfer drop
   c@ 256 + SPIxfer drop
   260 SPIxfer drop                     \ WRDI
   SPIwait
;
: SPIID  \ -- mfr type capacity
   159 SPIxfer                          \ JEDEC attempt at a standard
   dup xor  SPIxfer                     \ ended up vendor defined.
   dup xor  SPIxfer                     \ Refer to datasheets.
   drop 511 SPIxfer
;
: SPIerase4K  \ addr --                 \ erase 4K sector
   262 SPIxfer drop                     \ WREN
   32 256 SPIaddress
   260 SPIxfer drop                     \ WRDI
   SPIwait
;

\ used SPI flash commands:
\ 0x05 RDSR   opcd -- n1
\ 0x0B FR     opcd A2 A1 A0 xx -- data...
\ 0x02 PP     opcd A2 A1 A0 d0 d1 d2 ...
\ 0x20 SER4K  opcd A2 A1 A0
\ 0x06 WREN   opcd
\ 0x04 WRDI   opcd
\ 0x9F RDJDID opcd -- n1 n2 n3
