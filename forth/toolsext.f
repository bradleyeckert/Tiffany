\ Tools Extensions

: [defined]    parse-name hfind drop 0= \ 15.6.2.2534
; immediate
: [undefined]  postpone [defined] 0=    \ 15.6.2.2530.30
; immediate

\ Tools Extensions undefined:

\ 15.6.2.0470 ;CODE
\ 15.6.2.0740 ASSEMBLER
\ 15.6.2.0930 CODE
\ 15.6.2.1015 CS-PICK
\ 15.6.2.1020 CS-ROLL
\ 15.6.2.1300 EDITOR
\ 15.6.2.1580 FORGET
\ 15.6.2.1909.10 NAME>COMPILE
\ 15.6.2.1909.20 NAME>INTERPRET
\ 15.6.2.1909.40 NAME>STRING
\ 15.6.2.2264 SYNONYM
\ 15.6.2.2297 TRAVERSE-WORDLIST

\ Note: Tiff implements these:
\ 15.6.2.2531 [ELSE]
\ 15.6.2.2532 [IF]
\ 15.6.2.2533 [THEN]
