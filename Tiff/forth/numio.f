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
: term_key?  dup 0 user ;               \ -- flag
: term_key   dup 1 user ;               \ -- char

\ `personality` is an execution table for I/O.
\ The default personality is the following table in ROM:

cp @ equ term_personality               \ terminal personality
' term_emit ,
' term_cr ,
' term_page ,
' term_key? ,
' term_key ,

: io=term  \ --                         \ use terminal for I/O
   term_personality personality !
; io=term
: personality_exec                      \ fn# --
   cells personality @ + @ >r
;

: emit    0 personality_exec ;          \ 0: EMIT
: cr      1 personality_exec ;          \ 1: newline
: page    2 personality_exec ;          \ 2: clear screen
: key?    3 personality_exec ;          \ 3: check for key
: key     4 personality_exec ;          \ 4: get key char

\ TYPE accepts a UTF8 string where len is the length in bytes.
\ There could be fewer than len glyphs. EMIT takes a UTF code point.

\ UTF-8 encoding:
\ 0zzzzzzz                                0000-007F
\ 110yyyyy 10zzzzzz                       0080-07FF
\ 1110xxxx 10yyyyyy 10zzzzzz              0800-FFFF
\ 11110www 10xxxxxx 10yyyyyy 10zzzzzz     over FFFF

: char@u  \ addr len -- addr' len' c    \ get the next byte
   over >r 1- swap 1+ swap r> c@
;
: char@6  \ addr len n -- addr' len' n'
   6 lshift >r  char@u 63 and  r> +
;
: utf8@   \ addr len -- addr' len' utf  \ get next UTF8 character
   begin char@u dup 192 and 128 = while drop repeat \ 10xxxxxx not supported
   dup 128 and if
   dup 32 and if
   dup 16 and if
    7 and char@6 char@6 char@6 exit then \ 11110xxx
   15 and char@6 char@6        exit then \ 1110xxxx
   31 and char@6               exit then \ 110xxxxx
                                         \ 0xxxxxxx
;

:noname \ addr len --                   \ send chars
   begin dup 1- 0< invert while
      utf8@ emit
   repeat 2drop
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

\ Should add code to load RAM from table in ROM pointed to by ROM[4].

: initialize                            \ ? -- | R: ? a --
   r> base !                            \ save return address
   [ 0 up ] literal             up!     \ terminal task
   [ 4 sp ] literal  dup sp0 !  sp!     \ empty data stack
   [ 0 rp ] literal  dup rp0 !  4 - rp! \ empty return stack
   /pause io=term
   base @ >r  decimal                   \ init base
; call-only
