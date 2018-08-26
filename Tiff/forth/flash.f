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
: SPISR  \ -- status
   5 SPIxfer drop                       \ read status register
   511 SPIxfer                          \ LSB is `busy`, bit1 is WEN
;
: SPIwait  \ --
   begin  SPISR 1 and 0=  until
;
: SPIID  \ -- mfr type capacity
   159 SPIxfer                          \ JEDEC attempt at a standard
   dup xor  SPIxfer                     \ ended up vendor defined.
   dup xor  SPIxfer                     \ Refer to datasheets.
   dup xor  SPIxfer
;
: SPIerase4K  \ addr --                 \ erase 4K sector
   262 SPIxfer drop                     \ WREN
   32 256 SPIaddress
   260 SPIxfer drop                     \ WRDI
   SPIwait
;
: SPI![  \ addr --                      \ start a write command
   262 SPIxfer drop                     \ WREN
   2 0 SPIaddress
;
: ]SPI!  \ c --                         \ send last byte to SPI flash
   256 + SPIxfer drop
   260 SPIxfer drop                     \ WRDI
   SPIwait
;
: SPI!  \ n addr --                     \ store one cell
   SPI![  hld tuck !
   count SPIxfer drop
   count SPIxfer drop
   count SPIxfer drop
   c@ ]SPI!
;
: SPIC!  \ c addr --                    \ store one byte
   SPI![  hld tuck !
   c@ ]SPI!
;
: _SPImove  \ as ad len -- as' ad'      \ byte array to flash
   dup ifz: drop exit |
   over SPI![  tuck + >r                \ as len | ad'
   begin
      1- >r count                       \ a c | ad' len
      r@ 0= if
        ]SPI! r> drop r> exit
      then
      SPIxfer drop r>
   again
;
: SPImove  \ asrc adest len --          \ don't cross page boundary
   begin
      over invert 255 and 1+            \ max bytes on page
      2dup - >r  min                    \ limit for this page
      _SPImove  r>                      \ remainder
   dup 0< until  3drop
;
: ReGild  \ --                          \ reset wids and pointers
   [ hp0 cell+ ] literal @              \ -> wid_list
   begin
      @+ >r @+ >r @  r> swap !  r>
   dup 0< until  drop
   [ hp0 2 cells + ] literal            \ -> hp0 cp0 dp0
   hp 3 umove                           \ 3-cell move
;

\ : SPIbyte  \ a -- a+1
\    255 SPIxfer swap c!+
\ ;
\ \ @ should already do this, but SPI@ operates the SPI to test the interface.
\ : SPI@  \ addr -- x                     \ 32-bit fetch
\    11 0 SPIaddress  dup SPIxfer drop    \ command and dummy byte
\    hld  SPIbyte SPIbyte SPIbyte SPIbyte drop
\    511 SPIxfer drop                     \ end the command
\    hld @
\ ;

\ used SPI flash commands:
\ 0x05 RDSR   opcd -- n1
\ 0x0B FR     opcd A2 A1 A0 xx -- data...
\ 0x02 PP     opcd A2 A1 A0 d0 d1 d2 ...
\ 0x20 SER4K  opcd A2 A1 A0
\ 0x06 WREN   opcd
\ 0x04 WRDI   opcd
\ 0x9F RDJDID opcd -- n1 n2 n3
