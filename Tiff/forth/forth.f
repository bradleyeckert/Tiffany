\ High level Forth

\ EVALUATE: Given a string and length on the stack, evaluate it in the
\ current system context.

: source  \ -- c-addr len
   tibs 2@
;
: save-input  \ -- xn ... x1 n
   source-id  5
   dup >r  for  @+ swap
   next  drop r>
;
: restore-input  \ xn ... x1 n -- flag
   5 over <> -63 and throw              \ bogus save-input
   source-id  over cells +              \ ... x1 n address
   swap for                             \ ... x1 address
      4 - swap over !
   next  dup xor                        \ drop 0
;
: evaluate  \ i*x c-addr u -- j*x
   save-input n>r  ( c-addr u )
   tibs 2!  0 >in !  -1 source-id !
   ['] interpret   catch
   dup if
      cr source type cr  postpone [
   then
   nr> restore-input drop  throw
;
: word  \ char "<chars>ccc<char>" -- c-addr
   _parse pad  dup >r c! r@ c@+ cmove r>  \ use pad as temporary
;
: find  \ c-addr -- c-addr 0 | xt flag
   dup count hfind  over if             \ c-addr addr len
      drop dup xor exit                 \ not found
   then
   >r drop drop r>  xtextc              \ xte xtc
   ['] do-immediate xor 0<> 2* 1+
;

: cmove>  \ a1 a2 n --                   \ move bytes
   |-if 3drop exit |
   dup >r +  swap r@ +  swap  r>         \ a1' a2' n
   for
      1- swap 1- swap
      over c@ over c!
   next  2drop
;
: move  \ from to count --
   >r  2dup u< if  r> cmove>  else  r> cmove  then
;

: compare  \ c-addr1 len1 c-addr2 len2 -- n
   rot swap  2dup max >r
   - r> swap >r >r 						\ a1 a2 | sign count
   rfor
      c@+ >r swap c@+ >r swap           \ a1' a2' | sign count c2 c1
	  r> r> -                           \ a1' a2' diff
	  dup if
	     r> drop  r> drop				\ unloop
	     >r 2drop r> 0< 2* 1+ exit
      then drop
   next  2drop
   r> dup if							\ got all the way through
      0< 2* 1+ exit
   then
;

\ CP grew from 0, DP grew from ROMsize, HP grew from HP0

: unused    \ -- n
   c_scope c@
   case
   0 of  [ RAMsize ROMsize + ] literal  here -  endof
   1 of  [ hp0               ] literal  here -  endof
         hp @ hp0 -  swap
   endcase
;
cp @ ," DataCodeHead" 1+
: .unused  \ --
   literal                              \ sure is nice to pull in external literal
   3 0 do                               \ even it's not ANS
      i c_scope c!
      dup i cells + 4 type
      ." : " unused . cr
   loop  drop  ram
;

: .quit  \ error# --
   cr ." Error#" .
   cr tib tibs @ type cr
;

\ Interpret isn't hooked in yet, Tiff's version of QUIT is running.
\ Let's do some sanity checking.

: try  ['] interpret catch  ?dup if .quit postpone \ then ;

\ here's the rub: refill.
\ The VM only has access to KEY, KEY?, and EMIT. No files.
\ Once QUIT starts, file access goes away.
\ Simulated flash should be able to implement blocks, though.

\ Since KEY is raw input (rather than the cooked input served up by the terminal)
\ you would implement keyboard history (if any) here.

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
