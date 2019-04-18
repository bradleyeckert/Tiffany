\ Core Extensions not defined elsewhere

\ Note: DEFER is used for forward references only.
\ I'm not willing to give up RAM cells (scarce as they are) for that.
\ So, Mforth's DEFER is "non-standard". You only get DEFER and IS.

\ Your application should not use these CORE EXT words:
\ ACTION-OF, DEFER!, DEFER@.
\ Or, you can redefine and DEFER and IS.

\ UNUSED depends on the implementation
\ CP grew from 0, DP grew from ROMsize, HP grew from HP0

: unused    \ -- n                      \ 6.2.2395
   c_scope c@
   case
   0 of  [ RAMsize ROMsize + ] literal  here -  endof
   1 of  [ hp0               ] literal  here -  endof
         hp @ hp0 -  swap
   endcase
;

: roll                                  \ 6.2.2150
\ xu xu-1 ... x0 u -- xu-1 ... x0 xu
    dup ifz: drop exit |
    swap >r  1- recurse  r> swap
;

\ Note (from Standard): [compile] is obsolescent and is included as a
\ concession to existing implementations.
: [compile]  ' compile, ; immediate     \ 6.2.2530

\ http://www.forth200x.org/documents/html/core.html
: HOLDS  \ addr u --                    \ 6.2.1675
   BEGIN DUP WHILE 1- 2DUP + C@ HOLD REPEAT 2DROP
;

\ VALUE and TO - I don't use them but feel free to define them here.
\ 6.2.2405 VALUE
\ 6.2.2295 TO

\ Other undefined:
\ 6.2.1850 MARKER
\ 6.2.2125 REFILL
\ 6.2.2266 S\
