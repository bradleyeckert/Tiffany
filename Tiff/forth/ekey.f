\ EKEY conversion from VT220 cursor and function keys

cp @ equ ekey_table             \ counted strings are cell-aligned
   ," D"     ," A"     ," B"     ," C"      \ left up down right
   ," H"     ," F"     ," 5~"    ," 6~"     \ home end PgUp PgDn
   ," 1;5D"  ," 1;5C"                       \ ^left ^right
   ," P"     ," Q"     ," R"     ," S"      \ F1 F2 F3 F4
   ," 15"    ," 16"    ," 17"    ," 18"     \ F5 F6 F7 F8
   ," 19"    ," 1:"                         \ ctrl:
   ," P"     ," Q"     ," R"     ," S"      \ F1 F2 F3 F4
   ," 1;5P"  ," 1;5Q"  ," 1;5R"  ," 1;5S"   \ F5 F6 F7 F8
   ," 1;5T"  ," 1;5U"                       \ shift:
   ," 1;2P"  ," 1;2Q"  ," 1;2R"  ," 1;2S"   \ F1 F2 F3 F4
   ," 15;2~" ," 16;2~" ," 17;2~" ," 18;2~"  \ F5 F6 F7 F8
   ," 20;2~" ," 21;2~"                      \ F9 F10
   0 ,

: ekmatch  \ -- xchar           Compare table to incoming stream

: ekey   \ -- xchar             Convert arrow keys to a single number
   key  dup 27 = if                     \ escape sequence detected
      key [char] [ = if
         ekey_table dup  256 >r  key >r ( $ $ | R: xchar c )
         begin  c@+  dup while          ( $ c-addr length )
            rot over aligned + -rot     \ skip $ to next aligned string
            0 do c@+            ( $ c-addr c2 | R: xchar c1 )
            loop
            r> r> 1+ >r >r
         repeat
         drop drop r> drop r> exit
      then
      0                                 \ expected '[', got something else
   then
;

: keyt
   begin  key
      dup 32 = if drop exit then
      dup 27 = if
         drop key emit key emit
      else .
      then
   again ;
