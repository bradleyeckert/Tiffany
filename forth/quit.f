\ QUIT loop

\ Once QUIT starts, file access goes away.

: refill  ( -- ior )                    \ keyboard only
    depth ?dup if
      0 .r  [char] | emit
	then ." ok>"
    tib |tib| accept tibs !  0 >in !
	cr									\ mimic stdin trailing LF echo
    0
;

: (quit)  ( -- )
    begin
        refill drop
        interpret
    again
;

: .quit  \ error# --
    cr ." Error#" .
    cr tib tibs @ type cr
;

: quit
    begin
        status up!                      \ terminal task
        rp0 @  4 - rp!                  \ clear return stack
        tib tibb !
        0  dup state !  dup c_colondef !  blk !    \ clear compiler
        ['] (quit) catch
        ?dup if .quit then
        sp0 @  sp!                      \ clear data stack
    again
;
