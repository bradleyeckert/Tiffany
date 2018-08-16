\ Forth kernel for Tiff

include core.f
include numio.f
include interpret.f
include compile.f



\ Some test words

: foo hex decimal ;


cp @ equ s1 ," 123456"  : str1 s1 count ;
cp @ equ s2 ," the quick brown fox"  : str2 s2 count ;



