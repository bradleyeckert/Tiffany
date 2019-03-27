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
: um/mod_ov  \ ud u -- -1 -1
   drop drop drop -1 dup
;
: um/mod \ ud u -- ur uq
   2dup - drop
   |ifc um/mod_ov exit |                \ check for overflow
   -32  u2/ 2*                          \ clear carry
   begin                                \ L H divisor count
      >r >r   >r 2*c r> 2*c             \ dividend' | count divisor
      ifnc
         dup r@  - drop                 \ test subtraction
         |ifc r@ - |                    \ keep it
      else
         r@ -  0 2* drop                \ clear carry
      then
      r> r> 1+                          \ L' H' divisor count
   +until
   drop drop swap 2*c
;

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
