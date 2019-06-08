\ SPI flash access through user function.

\ In Tiff, a SPI flash chip is simulated in flash.c.
\ Real hardware will have an actual SPI flash chip that uses the same commands.
\ This code must run in internal ROM.
\ It trashes HLD, so you don't want to compile or write to flash in the
\ middle of numeric output. But that would be weird.

: SPIidle     \ --
   begin fn#sfbusy  0 user  0= until    \ wait until flash interface is idle
;

: SPIxfer     \ command -- result
   fn#spixfer io11                      \ 10-bit in, 8-bit out
;

: SPIaddress  \ addr command final --   \ initiate a command with 24-bit address
   hld dup @ >r  swap >r >r             \ R: hld final 'hld
   SPIxfer drop                         \ send command
   r@ ! r>                              \ use HLD cell for extracting bytes
   count >r  count >r  c@               \ high | R: hld final low med
   fn#baseblock  0 user  +              \ add 64KB-block offset
   SPIxfer drop                         \ assuming a little-endian model
   r> SPIxfer drop
   r> r> + SPIxfer drop
   r> hld !                             \ untrash hld
;
hex
: SPISR  \ -- status
   5 SPIxfer drop                       \ read status register
   1FF SPIxfer                          \ LSB is `busy`, bit1 is WEN
;
: SPIwait  \ --
   begin  SPISR 1 and 0=  until         \ spin loop does not pause
;
: SPIID  \ -- mfr type capacity
   SPIidle
   09F SPIxfer drop                     \ JEDEC attempt at a standard
   0FF SPIxfer                          \ ended up vendor defined.
   0FF SPIxfer                          \ Refer to datasheets.
   1FF SPIxfer
;
: SPIerase4K  \ addr --                 \ erase 4K sector
   SPIidle
   106 SPIxfer drop                     \ WREN
   20 100 SPIaddress
   104 SPIxfer drop                     \ WRDI
   SPIwait                              \ wait for erase to complete
;
: SPIbyte  \ a -- a+1                   \ read next byte from flash into RAM
   0FF SPIxfer swap c!+
;

\ _SPImove is the workhorse of flash programming.
: _SPImove  \ asrc adest len -- asrc'   \ write byte array to flash
   106 SPIxfer drop                     \ WREN
   swap 2 0 SPIaddress                  \ start page write command
   begin                                \ as len
      1- >r count                       \ as c | len
      r@ 0= if                          \ finished
         100 + SPIxfer drop
         104 SPIxfer drop               \ WRDI
         SPIwait                        \ wait for write to complete
         r> drop exit
      then
      SPIxfer drop r>
   again
;

\ Test the range to be programmed to catch non-blank bits.
\ Real SPI flash won't throw an error for you. SPItest will.

: SPItest  \ asrc adest len -- asrc adest len
   hld @ >r                             \ don't trash hld
   over 0B 0 dup >r  SPIaddress         \ start SPI read from adest
   r> SPIxfer drop                      \ dummy transfer
   third over negate begin  >r          ( ... asrc | -len )
      count                             \ src byte
      hld dup >r SPIbyte drop r> c@     \ dest byte
      or 0FF xor if                     \ trying to write '0' to existing '0'
         -3C throw
      then
   r> 1+ +until  2drop
   1FF SPIxfer drop                     \ end the read sequence
   r> hld !
;

\ Break up page writes, if necessary, to avoid crossing 256-byte page boundaries
: SPImove  \ asrc adest len --          \ byte array to flash
   SPIidle
   SPItest
   begin
      dup ifz: 3drop exit |             \ nothing left to move
      over invert 0FF and 1+            \ bytes to end of destination page
      over >r  min >r                   \ as ad | len sublen
      tuck r@ _SPImove                  \ ad as' | len sublen
      swap r@ +  r> r> swap -
   again
;
: SPI!  \ n addr --                     \ store one cell
   swap hld !
   hld swap 4 SPImove
;

\ @ should already do this, but SPI@ operates the SPI to test the interface.
: SPI@  \ addr -- x                     \ 32-bit fetch
   SPIidle
   0B 0 SPIaddress  dup SPIxfer drop    \ command and dummy byte
   hld  SPIbyte SPIbyte SPIbyte SPIbyte drop
   1FF SPIxfer drop                     \ end the read sequence
   hld @
;
decimal

\ used SPI flash commands:
\ 0x05 RDSR   opcd -- n1
\ 0x0B FR     opcd A2 A1 A0 xx -- data...
\ 0x02 PP     opcd A2 A1 A0 d0 d1 d2 ...
\ 0x20 SER4K  opcd A2 A1 A0
\ 0x06 WREN   opcd
\ 0x04 WRDI   opcd
\ 0x9F RDJDID opcd -- n1 n2 n3
