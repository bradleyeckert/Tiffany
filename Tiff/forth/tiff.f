\ Forth kernel for Tiff

include core.f
include tasker.f
include numio.f
include interpret.f
include tools.f

\ work in progress
include compile.f

\ Note: interpreter will need the compiler.


\ Some test words

: foo hex decimal ;


cp @ equ s1 ," 123456"  : str1 s1 count ;
cp @ equ s2 ," the quick brown fox"  : str2 s2 count ;


: oops  cr ." Error#" . ;  \ n --

: try  ['] interpret catch ?dup if oops then ;

