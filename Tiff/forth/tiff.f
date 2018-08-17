\ Forth kernel for Tiff

include core.f
include numio.f
include interpret.f
include tools.f

\ future stuff
include compile.f
include tasker.f

\ a number recognizer from Fminus
\ want to make smarter.
\ : isnumber  ( a -- n )
\    dup dup c@ + c@ [char] . =  dup >r  if -1 over c+! then     \ trim trailing .
\    count over   c@ [char] - =  dup >r  if 1 /string then    \ accept leading '-'
\    0 0 2swap >number abort" ???" drop                        \ convert to number
\    r> if dnegate then
\    tstate @ if r> if swap ,lit else drop then ,lit
\    else r> 0= if drop then then ;



\ Some test words

: foo hex decimal ;


cp @ equ s1 ," 123456"  : str1 s1 count ;
cp @ equ s2 ," the quick brown fox"  : str2 s2 count ;

\ to do:
\ Recognize a string as a number if possible.

: isfloat  \ addr len -- float32
   drop
;

\ Formats:
\ Leading minus applies to entire string.
\ A decimal anywhere means it's a 32-bit floating point number.
\ If string is bracketed by '' and the inside is a valid utf-8 sequence, it's a character.
\ C prefixes are recognized: 0x, 0X are hex.


: isnumber  \ addr u -- n
    over c@ [char] - =  dup >r  if 1 /string then           \ accept leading '-'
    2dup [char] . scan nip if                               \ is there a decimal?
       isfloat  r> 0x80000000 and + exit                    \ floating point
    then
    nip
    r> if negate then
;
\    0 0 2swap >number abort" ???" drop                        \ convert to number
\    r> if dnegate then
\    tstate @ if r> if swap ,lit else drop then ,lit
\    else r> 0= if drop then then ;
