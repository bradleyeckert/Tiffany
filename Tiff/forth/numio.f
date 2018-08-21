\ Numeric conversion and text I/O

\ EMIT pauses for the task loop. User functions 2 and 3 are for text output.
: term_emit                             \ xchar --
   begin pause dup 3 user until         \ wait until emit is ready
   2 user  drop                         \ send it
;
defer type

\ `defer` means "forward reference". There is no run-time `is`.
\ `$type` outputs a counted string.
\ VT100 escape sequences are assumed to work.

: $type      count type ;               \ addr --
cp @ equ string_cr ," \r\l"             \ CRLF
cp @ equ string_pg ," \e[2J"            \ ANSI "clear screen"
: term_cr    string_cr $type ;          \ --
: term_page  string_pg $type ;          \ --

\ `personality` is an execution table for I/O.
\ The default personality is the following table in ROM:

cp @ equ term_personality               \ terminal personality
' term_emit ,
' term_cr ,
' term_page ,

: io=term  \ --                         \ use terminal for I/O
   term_personality personality !
; io=term
: personality_exec                      \ fn# --
   cells personality @ + @ >r
;

: emit    0 personality_exec ;          \ 0: EMIT
: cr      1 personality_exec ;          \ 1: newline
: page    2 personality_exec ;          \ 2: clear screen

:noname \ addr len --                   \ send chars
   negate                               \ len=0 sends 1 char
   begin  swap count emit
          swap 1+
   +until 2drop
; is type
: space   bl emit ;
: spaces  negate begin +if: drop exit | space 1+ again ;

\ Numeric conversion, from eForth mostly.
\ Output is built starting at the end of a fixed `pad` of `|pad|` bytes.
\ Many Forths use RAM ahead of HERE for PAD.
\ That is not available here because the dictionary is in flash.

: digit   9 over < 7 and + [char] 0 + ;
: <#      [ pad |pad| + ] literal hld ! ;
: hold    hld @ 1- dup hld ! c! ;
: #       0 base @ um/mod >r base @ um/mod swap digit hold r> ;
: #s      begin # 2dup or 0= until ;
: sign    0< if [char] - hold then ;
: #>      2drop hld @ [ pad |pad| + ] literal over - ;
: s.r     over - spaces type ;
: d.r     >r dup >r dabs <# #s r> sign #> r> s.r ; \ d width --
: u.r     0 swap d.r ;                  \ u width --
: .r      >r s>d r> d.r ;               \ n width --
: d.      0 d.r space ;                 \ d --
: u.      0 d. ;                        \ u --
: .       base @ 10 xor if u. exit then s>d d. ; \ signed if decimal
: ?       @ . ;                         \ a --
: <#>     <# negate begin >r # r> 1+ +until drop #s #> ;  \ ud digits-1
: h.x     base @ >r hex  0 swap <#> r> base !  type space ;

