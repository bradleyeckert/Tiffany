\ Core words for 32-bit Mforth

 0 equ false
-1 equ true
32 equ bl

: !                 !+ drop ; macro     \ x addr --
: c!               c!+ drop ; macro     \ c addr --
: w!               w!+ drop ; macro     \ w addr --
: +!        dup >r @ + r> ! ;           \ x addr --
: c+!     dup >r c@ + r> c! ;           \ c addr --
: negate          invert 1+ ; macro     \ n -- -n
: 1-       invert 1+ invert ; macro     \ n -- n-1
: cells               2* 2* ; macro     \ n -- n*4
: rot       >r swap r> swap ; macro     \ n m x -- m x n
: -rot      swap >r swap r> ; macro     \ n m x -- x n m
: tuck            swap over ; macro     \ ab -- bab
: nip             swap drop ; macro     \ ab -- b
: 2>r        swap r> swap >r swap >r >r ;  call-only
: 2r>        r> r> swap r> swap >r swap ;  call-only
: 2r@  r> r> r@ swap >r swap r@ swap >r ;  call-only
: 2drop           drop drop ; macro     \ d --
: 2dup            over over ; macro     \ d1 d2 -- d1 d2 d1
: 2swap       rot >r rot r> ;
: 2over    >r >r 2dup r> r> 2swap ;
: 2rot  2>r 2swap 2r> 2swap ;
: -2rot           2rot 2rot ;
: 2nip          2swap 2drop ;
: 2tuck         2swap 2over ;
: 3drop      drop drop drop ; macro     \ abc --
: ?dup  dup ifz: exit | dup ;           \ n -- n n | 0
: s>d   dup +if: dup xor exit |
        dup xor invert ;                \ n -- d
: =                  xor 0= ; macro     \ x y -- f
: <>              xor 0= 0= ; macro     \ x y -- f
: 0<>                 0= 0= ; macro     \ x y -- f
: 0>              negate 0< ; macro     \ n -- f
: aligned      2+ 1+ -4 and ;           \ n -- n'
: d2*          >r 2* r> 2*c ;           \ d -- d'
: 2@         @+ swap @ swap ;           \ a -- n2 n1
: 2!             !+ !+ drop ; macro     \ n2 n1 a --
: abs         |-if negate | ;           \ n -- u
: d+     >r >r swap r> +  swap r> c+ ;  \ d1 d2 -- d3
: or    invert swap invert and invert ; \ n m -- n&m
: execute                >r ;           \ xt --
: dnegate                               \ d -- -d
   invert >r invert 1+ r> |
   over 0= -if: drop 1+ exit |
   drop
;
: dabs    |-if dnegate exit | ;         \ n -- u
: third   4 sp @ ;
: fourth  8 sp @ ;
: depth   sp0 @ 8 sp - 2/ 2/ ;
: pick    dup cells sp @ swap drop ;
: decimal 10 base ! ;                   \ --
: hex     16 base ! ;                   \ --
: link>   @ 16777215 and ;              \ a1 -- a2, mask off upper 8 bits

: (?do)  \ limit i -- | R: RA -- -limit i RA+4 | RA
   over over invert + 0<
   |-if drop exit |
   drop  swap invert 1+                 \ i -limit
   r> swap >r
   swap cell+ >r
; call-only
: (loop)  \ R: -limit i RA -- -limit i+1 RA | RA+4
   R> R> 1+ R>  over over +             \ RA i+1 ~limit sum
   |-if drop >R >R >R exit |            \ keep going
   drop drop drop cell+ >R              \ quit loop
; call-only
: (+loop)  \ x -- | R: -limit i RA -- -limit i+x RA / RA+4
   r> swap r> over +                    \ RA x i' | -limit
   r@ over +  swap >r                   \ RA x sign | -limit i'
   swap 0< xor                          \ RA sign | -limit i'
   |-if  drop >r  exit |                \ keep going
   drop  r> drop  r> drop  cell+ >r     \ quit loop
; call-only

: j       12 rp @ ; call-only           \ R: ~limit j ~limit i RA
: unloop  R> R> drop R> drop >R ; call-only

        \ match?         no         yes
: (of)  \ x1 x2 R: RA -- x1 R: RA | R: RA+4
   over over xor 0= |ifz drop exit |
   drop drop  r> cell+ >r               \ eat x and skip branch
; call-only
         \ done?        no           yes
: (for)  \ R: cnt RA -- cnt-1 RA+4 | RA
   r> r> 1-                             \ faster than DO LOOP because all of the
   |-if drop >r exit |                  \ indexing is done here. NEXT just jumps.
   >r cell+ >r                          \ FOR NEXT does something 0 or more times.
; call-only                             \ It also skips if negative count.


: umove  \ a1 a2 n --                   \ move cells, assume cell aligned
   negate |+if 3drop exit |
   1+ swap >r swap
   | @+ r> !+ >r -rept                  \ n a1' | a2'
   r> 3drop
;
: cmove  \ a1 a2 n --                   \ move bytes
   negate |+if 3drop exit |
   1+ swap >r swap
   | c@+ r> c!+ >r -rept                \ n a1' | a2'
   r> 3drop
;
: fill  \ a1 n c --                     \ fill with bytes
   swap negate |+if 3drop exit |        \ a c n
   1+ swap >r swap                      \ n a | c
   | r@ swap c!+ -rept                  \ n a' | c
   r> 3drop
;

\ Software versions of math functions
\ May be replaced by user functions.

: lshift  \ x count                     \ left shift
   63 and                               \ limit the count
   negate +if: drop exit |              \ 0
   1+ swap
   | 2* -rept swap drop ;
;
: rshift  \ x count                     \ right shift
   63 and                               \ limit the count
   negate +if: drop exit |              \ 0
   1+ swap
   | u2/ -rept swap drop ;
;
: um*  \ u1 u2 -- ud                    \ 450-550 beats
   0 -32  begin                         \ u1 u2 0 count
      >r 2* >r 2*c                      \ u1 u2' | count x'
      |ifc over r> + >r |               \ add u1 to x
      |ifc 1+ |                         \ carry into u2
      r> r> 1+                          \ u1 u2' x' count'
   +until
   drop >r >r drop r> r> swap
;
: um/mod \ ud u -- ur uq
   2dup - drop
   ifnc
      drop drop drop  -1 dup  exit      \ overflow
   then
   -32  u2/ 2*                          \ clear carry
   begin                                \ L H divisor count
      >r >r  >r 2*c r> 2*c              \ dividend64 | count divisor32
      ifnc  \ either way, subtract
         dup r@  - drop                 \ test subtraction
         ifnc r@ - then                 \ keep it
      else                              \ carry out of dividend, so subtract
         r@ -  0 2* drop                \ clear carry
      then
      r> r> 1+                          \ L' H' divisor count
   +until
   drop drop swap 2*c invert            \ finish quotient
;
: sm/rem  \ d n -- rem quot
   2dup xor >r  over >r  abs >r dabs r> um/mod
   swap r> 0< if  negate  then
   swap r> 0< if  negate  then ;

: fm/mod  \ d n -- rem quot
   dup >r  2dup xor >r  dup >r  abs >r dabs r> um/mod
   swap r> 0< if  negate  then
   swap r> 0< if  negate  over if  r@ rot -  swap 1-  then then
   r> drop ;

\ eForth model
: m/mod
    dup 0< dup >r
    if negate  >r
       dnegate r>
    then >r dup 0<
    if r@ +
    then r> um/mod
    r> if
       swap negate swap
    then
;
: /mod   over 0< swap m/mod ;
: mod    /mod drop ;
: /      /mod nip ;

: u<  2dup xor 0<
    |ifz - 0< exit |
    swap drop 0<
;
: u> swap u< ;
: < 2dup xor 0<
    |ifz - 0< exit |
    drop 0<
;
: >  swap < ;

: min     2dup < ifz: swap | drop ;
: max     2dup swap < |ifz swap | drop ;
: umin    2dup u< ifz: swap | drop ;
: umax    2dup swap u< |ifz swap | drop ;

: /string >r swap r@ + swap r> - ;      \ a u -- a+1 u-1
: within  over - >r - r> u< ;           \ u ulo uhi -- flag
: u>      swap u< ;                     \ u1 u2 -- f
: >       swap < ;                      \ n n -- f
: *       um* drop ;
: m*
    2dup xor 0< >r
    abs swap abs um*
    r> if dnegate then
;
: */mod  >r m* r> m/mod ;
: */     */mod swap drop ;
: bye    6 user ;                       \ exit to OS if there is one

\ This version expects two registers for the top of the data stack

: catch  \ xt -- exception# | 0         \ return addr on stack
    over >r                             \ save N
    4 sp >r          \ xt               \ save data stack pointer
    handler @ >r     \ xt               \ and previous handler
    0 rp handler !   \ xt               \ set current handler = ret N sp handler
    execute                             \ execute returns if no throw
    r> handler !                        \ restore previous handler
    r> drop
    r> dup xor       \ 0                \ discard saved stack ptr
; call-only

: throw  \ ??? exception# -- ??? exception#
    dup ifz: drop exit |                \ Don't throw 0
    handler @ rp!   \ exc#              \ restore prev return stack
    r> handler !    \ exc#              \ restore prev handler
    r> swap >r      \ saved-sp          \ exc# is on return stack
    sp! drop nip
    r> r> swap      \ exc#              \ Change stack pointer
;
