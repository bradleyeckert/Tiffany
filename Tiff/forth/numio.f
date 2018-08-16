\ Numeric conversion and text I/O

defer type
: $type      count type ;               \ addr --
cp @ equ string_cr ," \r\l"             \ CRLF
cp @ equ string_pg ," \e[2J"            \ ANSI "clear screen"
: term_emit  2 user drop ;              \ c --
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

: >char   127 and dup 127 bl within if drop 95 then ;
: digit   9 over < 7 and + [char] 0 + ;
: <#      pad hld ! ;
: hold    hld @ 1 - dup hld ! c! ;
: #       0 base @ um/mod >r base @ um/mod swap digit hold r> ;
: #s      begin # 2dup or 0= until ;
: sign    0< if [char] - hold then ;
: #>      2drop hld @ pad over - ;
: s.r     over - spaces type ;
: d.r     >r dup >r dabs <# #s r> sign #> r> s.r ;
: u.r     0 swap d.r ;
: .r      >r s>d r> d.r ;
: d.      0 d.r space ;
: u.      0 d. ;
: .       base @ 10 xor if u. exit then s>d d. ; \ signed if decimal
: ?       @ . ;
: du.r    >r <# #s #> r> s.r ;
: du.     du.r space ;

\ convert char to uppercase, also used by `hfind`
: toupper  \ c -- c'
   dup [char] a [char] { within \ }
   32 and +
;
\ convert char to digit, return `ok` flag
: digit?  \ c base -- n ok?
   >r  toupper  [char] 0 -
   dup 10 17 within 0= >r               \ check your blind spot
   dup 9 > 7 and -
   r> over r> u< and                    \ check the base
;
\ convert string to number, stopping at first non-digit
: >number       \ ud a u -- ud a u   ( 6.1.0570 )
   begin dup
   while >r  dup >r c@ base @ digit?
   while swap base @ um* drop rot base @ um* d+ r> char+ r> 1-
   repeat drop r> r> then
;
\ stack dump
: .s
   depth  ?dup if
      negate begin
         dup negate pick .
      1+ +until drop
   then ." <sp " cr
;

