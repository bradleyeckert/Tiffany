\ Forth kernel for Tiff

include core.f
include tasker.f
include numio.f
\ Here's where you'd reposition CP to run code out of SPI flash
\ in order to keep the internal ROM small(er). Your app would go there too.
\ Unplugging the SPI flash should break the interpreter but nothing else.

\ At this point, 1.4K bytes are in the internal ROM image.
include tools.f
include compile.f    \ work in progress
include interpret.f

\ Note: interpreter will need the compiler.


\ Some test words

: foo hex decimal ;


cp @ equ s1 ," 123456"  : str1 s1 count ;
cp @ equ s2 ," the quick brown fox"  : str2 s2 count ;


: oops
   cr ." Error#" .
   w_>in w@ >in !               \ back up to error
   parse-word type space
;

: try  ['] interpret catch ?dup if oops then ;

.( `tiff.f` compiled ) CP ? .( bytes of code, ) HP @ 32768 - . .( bytes of header.)
cr
