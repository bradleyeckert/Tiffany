\ Forth kernel for Tiff

include core.f
include tasker.f
include numio.f
include flash.f
\ Here's where you'd reposition CP to run code out of SPI flash.
\ Put the time critical parts of your application here.
\ Omitting the SPI flash should break the interpreter but nothing else.

cp ?                                    \ display bytes of internal ROM used
hp0 16384 + cp !                        \ leave 16kB for headers

include compile.f                       \ compile opcodes, macros, calls, etc.
include wean.f                          \ replace C functions in existing headers
include interpret.f                     \ parse, interpret, convert to number
include tools.f

\ include define.f    \ not quite working.


\ Some test words

: foo hex decimal ;

cp @ equ s1
   ," 123456"
   : str1 s1 count ;

cp @ equ s2
    ," the quick brown fox"
    : str2 s2 count ;

\ causes: Attempt to write to non-blank flash memory
\ tiff.f[26]: cp @ equ s1 ," 123456"
\ if placed earlier...

: oops
   cr ." Error#" .
   w_>in w@ >in !               \ back up to error
   parse-word type space
;

: try  ['] interpret catch ?dup if oops then ;

.( bytes in internal ROM, ) CP @ hp0 16384 + - .
.( bytes of code in flash, ) HP @ hp0 - .
.( bytes of header.) cr


