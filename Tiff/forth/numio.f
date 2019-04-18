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
cp @ aligned cp !
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

: emit    0 personality_exec ;          \ 6.1.1320  xchar --
: cr      1 personality_exec ;          \ 6.1.0990  --
: page    2 personality_exec ;          \  clear screen
: key?    3 personality_exec ;          \  check for key
: key     4 personality_exec ;          \ 6.1.1750  -- c

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

:noname \ addr len --                   \ 6.1.2310  send chars
   begin dup 1- 0< invert while
      utf8@ emit
   repeat 2drop
; is type

: space   bl emit ;                     \ 6.1.2220  --
: spaces  begin |-if drop exit |        \ 6.1.2230  n --
          space 1- again ;

\ Numeric conversion, from eForth mostly.
\ Output is built starting at the end of a fixed `pad` of `|pad|` bytes.
\ Many Forths use RAM ahead of HERE (RAM scope) for PAD.

: digit   9 over < 7 and + [char] 0 + ;
: <#      [ pad |pad| + ] literal       \ 6.1.0490
          hld ! ;
: hold    hld @ 1- dup hld ! c! ;       \ 6.1.1670
: #       0 base @ um/mod >r            \ 6.1.0030
            base @ um/mod swap digit hold r> ;
: #s      begin # 2dup or 0= until ;    \ 6.1.0050
: sign    0< if [char] - hold then ;    \ 6.1.2210
: #>      2drop hld @ [ pad |pad| + ] literal over - ; \ 6.1.0040
: s.r     over - spaces type ;
: d.r     >r dup >r dabs                \ 8.6.1.1070  d width --
          <# #s r> sign #> r> s.r ;
: u.r     0 swap d.r ;                  \ 6.2.2330  u width --
: .r      >r s>d r> d.r ;               \ 6.2.0210  n width --
: d.      0 d.r space ;                 \ 8.6.1.1060  d --
: u.      0 d. ;                        \ 6.1.2320  u --
: .       base @ 10 xor if              \ 6.1.0180  n|u
             u. exit                    \           unsigned if hex
          then  s>d d. ;                \           signed if decimal
: ?       @ . ;                         \ 15.6.1.0220  a --
: <#>     <# negate begin >r # r> 1+ +until drop #s #> ;  \ ud digits-1
: h.x     base @ >r hex  0 swap <#> r> base !  type space ;

\ ACCEPT is needed by safemode flash programming, must be in internal ROM.
\ BACKSPACE (or ^H) is a concession to inevitable fat-fingering.

: accept  \ c-addr +n1 -- +n2           \ 6.1.0695
   >r dup dup r> + >r                   \ a a | limit
   begin  dup r@ xor  while
      begin pause key? until  key       \ get char
      dup 13 = if  drop		            \ terminator
         r> drop swap - exit
      then
      dup 8 = if  drop 1-               \ backspace
         ." \e[D"                       \ VT52 left one place
      else
         dup bl - +if  drop             \ valid char
            c_noecho c@ 0= if
               dup emit 				\ echo if enabled
            then
            swap c!+
         else  2drop
         then
      then
   repeat  r> drop swap -				\ filled
;
