\ High level Forth

\ Words that use INTERPRET, IMMEDIATE, etc.

: \    tibs @ >in ! ; immediate         \ comment to EOL

\ Header space:     W xtc xte link name
\ offset from ht: -12  -8  -4    0 4

: h'  \ "name" -- ht
   parse-word  hfind  swap 0<>          \ len -1 | ht 0
   -13 and throw                        \ header not found
;
: '   \ "name" -- xte
   h' invert cell+ invert link>
;

\ Note (from Standard): [compile] is obsolescent and is included as a
\ concession to existing implementations.
\ : [compile] ( "name" -- )   ' compile, ;  immediate

: postpone ( "name" -- )
   h'  8 - dup  cell+ link>  swap link> \ xte xtc
   ['] do-immediate =                   \ immediate?
   if   compile, exit   then            \ compile it
   literal,  ['] compile, compile,      \ postpone it
; immediate

: .quit  \ error# --
   cr ." Error#" .
   cr tib tibs @ type cr
;

\ Interpret isn't hooked in yet, Tiff's version of QUIT is running.
\ Let's do some sanity checking.

: try  ['] interpret catch ?dup if .quit then ;

\ here's the rub: refill.
\ The VM only has access to KEY, KEY?, and EMIT. No files.
\ Once QUIT starts, file access goes away.
\ Simulated flash should be able to implement blocks, though.

\ Since KEY is raw input (rather than the cooked input served up by the terminal)
\ you would implement keyboard history (if any) here.

: keyt  begin key dup 32 <> while . repeat drop ;

: refill  ( -- ior )  0 ;

: (quit)  ( -- )
   begin
      refill drop  interpret  ." >ok"
   again
;

: quit
   begin
      status up!                        \ terminal task
      rp0 @  4 - rp!                    \ clear return stack
      tib tibb !
      0  dup state !  dup c_colondef !  blk !    \ clear compiler
      ['] (quit) catch
      ?dup if .quit then
      sp0 @  sp!                        \ clear data stack
   again
;
