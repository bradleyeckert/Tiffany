\ Serial Port example

defer coldboot
defer safemode
defer errorISR

1 equ options                           \ use HW mul/div
include ../../forth/core.f
: pause ;  \ include ../../forth/tasker.f \ no multitasker
include ../../forth/timing.f
include ../../forth/numio.f             \ numeric I/O

	: throw  \ n --  				    \ for testing, remove later
	?dup if  port drop  			    \ save n in dbg register, like error interrupt
		8 >r						    \ fake an error interrupt
	then
	; call-only

	\ The error ISR has the last known good PC on the return stack and the ior in port.
	\ Usually, you would just throw an error.
	\ Since the Mforth interpreter is being used, use a test THROW.

	:noname
	cr ." Error " dup port . ." at PC=" r> .
	." Line# " w_linenum w@ .	cr
	-1 @  								\ produce an error to quit
	; is errorISR

include ../../forth/comma.f             \ smart comma

: hi
	." Hello World!" cr
	10 0 do i . loop cr
;

include ../../forth/hostfns.f           \ serial port, file access, etc.

variable port  7 port !
variable connected
variable baseblock
115200 constant baudrate

\ The com port is assumed to work - if you drop a character or something,
\ the code may hang. In that case, close the app and start again.
\ Use read-back or CRC to verify programming.

hex
: ping  \ -- char               \ returns a 60 or 61 status, 61 means busy
   [char] / com-emit com-key
;
: getbyte  \ -- c               \ get hex byte using lower nybls only
   com-key  2* 2* 2* 2* 0F0  and
   com-key  0F and +
;
: getPID  \ -- u                \ get 32-bit product ID
   [char] ? com-emit  0 4 0 do
   8 lshift com-key +     loop
;
: ..  \ --                      \ check for junk chars in the return stream
   begin com-key? while com-key . repeat
;
: getbaseblock  \ -- n         \ query base sector (64K block) of Forth
   [char] + com-emit getbyte
;
: connect  \ --
   connected @ if
      ." Already connected. Disconnect first."
      exit
   then
   baudrate port @ com-open throw
   10 com-emit  0F com-emit  12 com-emit         \ take control of flash
   1 connected !
   \ there should be a time-out error detect here
   begin ping [char] < = until          \ wait until controller is ready
   getbaseblock baseblock !
   hex ." PID = 0x" getPID .
   cr ." BaseBlock = 0x" baseblock @ .
   decimal
;
: disconnect  \ --
   connected @ 0= if
      ." Already disconnected. Connect first."
      exit
   then
   1B com-emit  32 ms  com-close
   0 connected !
;
: SPIxfer  ( in -- out )                \ same behavior as in the VM but over UART
   03FF and
   dup 5 rshift 40 + com-emit              \ first 5-bit digit
   01F and 60 + com-emit                   \ last 5-bit digit
   com-key  0F and  2* 2* 2* 2*            \ returned digits = byte
   com-key  0F and +
;
: SPIidle ;    \ --  \ wait until flash interface is idle (always ready)

decimal

\ The following is copied from flash.f
\ Note that flash addresses are an offset from (baseblock<<16).
\ Modern computers have a long turnaround time between TX and RX streams.
\ Try to avoid that limitation.

: SPIaddress  \ addr command final --   \ initiate a command with 24-bit address
   hld dup @ >r  swap >r >r             \ R: hld final 'hld
   SPIxfer drop                         \ send command
   r@ ! r>                              \ use HLD cell for extracting bytes
   count >r  count >r  c@               \ high | R: hld final low med
   baseblock @ +                        \ add 64KB-block offset
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



\ theme=color  \ looks better in color, if your terminal isn't dumb.
