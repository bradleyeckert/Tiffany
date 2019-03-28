\ Forth kernel for Tiff

\ CP, HP, and DP point to the first free bytes in code, header, and data space.
\ System variables are already defined in Tiff because it uses them internally.
\ See `config.h`.

4 equ DumpColumns                       \ Output chars ~ Columns * 13 + 5

include core.f
include tasker.f
include numio.f
include flash.f

\ Here's where we reposition CP to run code out of SPI flash.
\ Put the less time-critical parts of your application here.
\ Omitting the SPI flash should break the interpreter but nothing else.
\ This is also necessary because the Forth can't modify internal ROM at run time.

cp ?                                    \ display bytes of internal ROM used
hp0 0x8000 + cp !                       \ leave 32K for headers

include compile.f                       \ compile opcodes, macros, calls, etc.
include wean.f                          \ replace C functions in existing headers
include interpret.f                     \ parse, interpret, convert to number
include tools.f                         \ dump, .s
include define.f                        \ defining words

\ If you XWORDS at this point, you'll see that all XTEs and XTCs are now Forth words.


\ Some test words

: foo hex decimal ;

cp @ equ s1
   ," 123456"
   : str1 s1 count ;

cp @ equ s2
    ," the quick brown fox"
    : str2 s2 count ;

: oops
   cr ." Error#" .
   w_>in w@ >in !               \ back up to error
   parse-word type space
;

\ Interpret isn't hooked in yet, Tiff's version of QUIT is running.
\ Let's do some sanity checking.

: try  ['] interpret catch ?dup if oops then ;

.( bytes in internal ROM, ) CP @ hp0 0x8000 + - .
.( bytes of code in flash, ) HP @ hp0 - .
.( bytes of header.) cr


save-rom IntRom.c
save-flash ExtRom.c
