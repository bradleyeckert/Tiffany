\ Tools

: .s  \ ? -- ?                          \ stack dump
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
: wrds  \ --                            \ list all
   c_wids c@ begin
      invert 1+ invert -if: drop exit | \ finished
      dup cells context + @ _words      \ wid
   again
;

\ Dump in cell and char format

: dump  \ addr bytes --
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
