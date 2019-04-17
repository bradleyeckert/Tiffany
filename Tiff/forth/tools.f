\ Tools

\ Note: Misspellings to allow host versions to run:
\ WRDS, _DASM, _SEE

: .s  \ ? -- ?                          \ 15.6.1.0220  stack dump
   depth  ?dup if
      negate begin
         dup negate pick .
      1+ +until drop
   then ." <sp " cr
;
: _words  \ wid --                      \ display one wordlist
   @ begin
      dup >r cell+ c@+  31 and  type    \ 'name length | wid
      space r> link>
   dup 0= until  drop                   \ example: context @ @ _words
;
: wrds  \ --                            \ 15.6.1.2465
   c_wids c@ begin
      invert 1+ invert -if: drop exit | \ finished
      dup cells context + @ _words      \ wid
   again
;

\ Dump in cell and char format

: dump  \ addr bytes --                 \ 15.6.1.1280
   swap -4 and swap                     \ cell-align the address
   begin dup while
      cr over 3 h.x
      2dup  [ DumpColumns ] literal
      dup >r min  r> over - 9 * >r      \ a u addr len | padding
      begin dup while >r                \ dump cells in 32-bit hex
         @+ 7 h.x r> 1-
      repeat  2drop  r> spaces
      2dup  [ DumpColumns 2* 2* ] literal min
      begin dup while >r                \ dump chars in ASCII
         c@+
         dup bl 192 within |ifz drop [char] .
         emit  r> 1-                    \ outside of 32 to 191 is '.'
      repeat 2drop
      [ DumpColumns 2* 2* ] literal /string
      0 max
   repeat  2drop
   cr
;

\ http://lars.nocrew.org/forth2012/tools/NtoR.html
: N>R \ xn .. x1 N - ; R: -- x1 .. xn n \ 15.6.2.1908
\ Transfer N items and count to the return stack.
   DUP                        \ xn .. x1 N N --
   BEGIN
      DUP
   WHILE
      ROT R> SWAP >R >R       \ xn .. N N -- ; R: .. x1 --
      1-                      \ xn .. N 'N -- ; R: .. x1 --
   REPEAT
   DROP                       \ N -- ; R: x1 .. xn --
   R> SWAP >R >R
; call-only

: NR> \ - xn .. x1 N ; R: x1 .. xn N -- \ 15.6.2.1940
\ Pull N items and count off the return stack.
   R> R> SWAP >R DUP
   BEGIN
      DUP
   WHILE
      R> R> SWAP >R -ROT
      1-
   REPEAT
   DROP
; call-only
