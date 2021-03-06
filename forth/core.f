\ Core words for 32-bit Mforth

 0 equ false                            \ 6.2.1485
-1 equ true                             \ 6.2.2298
32 equ bl                               \ 6.1.0770

: -             invert 1+ + ; macro     \ replace "-" opcode
: !                 !+ drop ; macro     \ 6.1.0010  x addr --
: c!               c!+ drop ; macro     \ 6.1.0850  c addr --
: w!               w!+ drop ; macro     \           w addr --
: +!        dup >r @ + r> ! ;           \ 6.1.0130  x addr --
: c+!     dup >r c@ + r> c! ;           \           c addr --
: negate          invert 1+ ; macro     \ 6.1.1910  n -- -n
: 1-       invert 1+ invert ; macro     \ 6.1.0300  n -- n-1
: cell- invert cell+ invert ; macro     \           n -- n-4
: cells               2* 2* ; macro     \ 6.1.0890  n -- n*4
: rot       >r swap r> swap ; macro     \ 6.1.2160  n m x -- m x n
: -rot      swap >r swap r> ; macro     \           n m x -- x n m
: tuck            swap over ; macro     \ 6.2.2300  ab -- bab
: nip             swap drop ; macro     \ 6.2.1930  ab -- b
: 2>r        swap r> swap >r swap >r >r \ 6.2.0340
; call-only
: 2r>        r> r> swap r> swap >r swap \ 6.2.0410
; call-only
: 2r@  r> r> r@ swap >r swap r@ swap >r \ 6.2.0415
; call-only
: 2drop           drop drop ; macro     \ 6.1.0370  d --
: 2dup            over over ; macro     \ 6.1.0380  d -- d d
: 2swap       rot >r rot r> ;           \ 6.1.0430  d1 d2 -- d2 d1
: 2over    >r >r 2dup r> r> 2swap ;     \ 6.1.0400  d1 d2 -- d1 d2 d1
: 3drop      drop drop drop ; macro     \ abc --
: ?dup  dup ifz: exit | dup ;           \ 6.1.0630  n -- n n | 0
: s>d   dup -if: dup xor invert exit |  \ 6.1.2170  n -- d
        dup xor ;
: not                    0= ; macro     \           x -- f
: =                  xor 0= ; macro     \ 6.1.0530  x y -- f
: <>              xor 0= 0= ; macro     \ 6.2.0500  x y -- f
: 0<>                 0= 0= ; macro     \ 6.2.0260  x y -- f
: 0>              negate 0< ; macro     \ 6.2.0280  n -- f
: aligned   1+ 1+ 1+ -4 and ;           \ 6.1.0706  n -- n'
: 2@         @+ swap @ swap ;           \ 6.1.0350  a -- n2 n1
: 2!             !+ !+ drop ; macro     \ 6.1.0310  n2 n1 a --
: abs         |-if negate | ;           \ 6.1.0690  n -- u
: or    invert swap invert and invert ; \ 6.1.1980  n m -- n|m
: execute                >r ;           \ 6.1.1370  xt --
: d+     >r >r swap r> +  swap r> c+ ;  \ 8.6.1.1040  d1 d2 -- d3
: dnegate                               \ 8.6.1.1230  d -- -d
   invert >r invert 1+ r> |
   over 0= -if: drop 1+ exit |
   drop
;
: dabs    |-if dnegate exit | ;         \ 8.6.1.1160  n -- u
: count   c@+ ; macro					\ 6.1.0980  a -- a+1 c
: third   4 sp @ ;
: fourth  8 sp @ ;
: depth   sp0 @ 8 sp - 2/ 2/ ;          \ 6.1.1200  -- n
: pick    dup cells sp @ swap drop ;    \ 6.2.2030  ? n -- m
: decimal 10 base ! ;                   \ 6.1.1170  --
: hex     16 base ! ;                   \ 6.2.1660  --
: link>   @ 16777215 and ;              \ a1 -- a2, mask off upper 8 bits
         \ do is  "swap negate >r >r"  macro
         \ run loop?             no    yes
: (?do)  \ limit i -- | R: RA -- RA | -limit i RA+4
   over over invert +   |-if 3drop exit |
   drop   swap negate                   \ i -limit
   r> cell+  swap >r swap >r  >r
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

: j       12 rp @ ; call-only           \ 6.1.1730  R: ~limit j ~limit i RA
: unloop  R> R> drop R> drop >R         \ 6.1.2380  R: x1 x2 RA -- RA
; call-only
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

: (string)  \ -- c-addr | R: RA -- RA'	\ skip over counted string
   r> dup c@+ + 						\ ra ra'
   3  dup >r  + 						\ aligned
   r> invert and >r
; call-only

: u<  - 2*c 1 and 1- ;                	\ 6.1.2340  u1 u2 -- flag
: u>  swap u< ;                         \ 6.2.2350  u1 u2 -- flag

\ Here's where I disagree with the ANS standard.

\ You should be able to use "- 0<" for "<", but ANS "<" assumes the
\ existence of overflow and other flags such as those in typical CPUs.
\ This means the simple definition will fail only in a special case
\ that never happens in practice. That case is included in the Hayes test suite.

options 8 and [if]                      \ loose ANS compliance
: < - 0< ; macro
[else]
: < 2dup xor 0<                         \ 6.1.0480  n1 n2 -- flag
    |ifz - 0< exit |
    drop 0<
;
[then]
: >  swap < ;                           \ 6.1.0540  n1 n2 -- flag

: umove  \ a1 a2 n --                   \ move cells, assume cell aligned
   1- +if
      negate  swap >r swap
      | @+ r> !+ >r -rept nop           \ n a1' | a2'
      r> 3drop exit
   then  3drop
;
: cmove  \ a1 a2 n --                   \ 17.6.1.0910
   1- +if
      negate  swap >r swap
      | c@+ r> c!+ >r -rept nop         \ n a1' | a2'
      r> 3drop exit
   then  3drop
;

: cmove>  \ a1 a2 n --                  \ 17.6.1.0920
   1- +if  1+
      dup >r +  swap r@ +  swap  r>     \ a1 a2 n
      negate begin >r
         1- swap 1- swap
         over c@ over c!
      r> 1+ +until
   then  3drop
;

: move  \ from to count --              \ 6.1.1900
   >r  2dup u< if  r> cmove>  else  r> cmove  then
;

: fill  \ a1 n c --                     \ 6.1.1540
   over if
      swap negate                       \ a c -n
      1+ swap >r swap                   \ -n a | c
      | r@ swap c!+ -rept nop           \ -n' a' | c
      r> 3drop exit
   then  3drop
;

\ Is a 4x speedup worth it? Maybe if large RAM needs erased.
\ A 100 MHz CPU erases 16 cells (64 bytes) per microsecond.
\ A large 64KB RAM would erase in 1 ms.
\ I made it big and ugly to speed up hardware simulation.
\ In real life, just use "0 fill".

: erase  \ a n --                       \ 6.2.1350  --
   dup ifz: 2drop exit |                \ no length
   2dup or 3 and if \ cell address and cell length
     0 fill  exit   \ no, do it a byte at a time
   then
   dup dup xor >r   \ more compact "0 >r"
   2/ 2/ negate 1+  swap                ( -n a )
   | r@ swap !+ -rept nop
   r> 3drop
;

\ Software versions of math functions
\ May be replaced by user functions.

: lshift  \ x count -- x'               \ 6.1.1805
   1- -if: drop exit |
   63 and   negate  swap
   | 2* -rept nop swap drop ;
;

: rshift  \ x count                     \ 6.1.2162
   1- -if: drop exit |
   63 and   negate  swap
   | u2/ -rept nop swap drop ;
;

options 1 and [if]
: um*  \ u1 u2 -- ud                    \ 6.1.2360
   5 user ( u1 low )  swap  3 user
;
: *  \ n1 n2 -- n3                      \ 6.1.0090
   5 user ( u1 low )  swap drop
;
: um/mod \ ud u -- ur uq                \ 6.1.2370
   3 user drop                          \ set divisor
   4 user  ( x quot ) swap
   3 user  swap
;
[else]
: um*  \ u1 u2 -- ud                    \ 6.1.2360
   dup dup xor -32
   begin >r
   2* >r 2*c                            \ u1 u2' | count x'
   |ifc over r> + >r |                  \ add u1 to x
   |ifc 1+ |                            \ carry into u2
   r> r> 1+ +until drop
   >r >r drop r> r> swap
;
: *       um* drop ;                    \ 6.1.0090  n1 n2 -- n1*n2
: um/mod \ ud u -- ur uq                \ 6.1.2370
    2dup - drop
    ifnc
        -32  u2/ 2*                     \ clear carry
        begin
            >r >r  swap 2*c swap 2*c    \ dividend64 | count divisor32
            ifnc
                dup r@  - drop          \ test subtraction
                |ifc r@ - |             \ keep it  (carry is inverted)
                dup 2*c invert 2/ drop
            else                        \ carry out of dividend, so subtract
                r@ -   0 2* drop        \ clear carry
            then
            r> r> 1+                    \ L' H' divisor count
        +until
        drop drop swap 2*c invert       \ finish quotient
        exit
    then
    drop drop dup xor  dup 1-           \ overflow = 0 -1
;
[then]

: sm/rem  \ d n -- rem quot             \ 6.1.2214
   2dup xor >r  over >r  abs >r dabs r> um/mod
   swap r> 0< if  negate  then
   swap r> 0< if  negate  then ;

: fm/mod  \ d n -- rem quot             \ 6.1.1561
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
: /mod   over 0< swap m/mod ;           \ 6.1.0240
: mod    /mod drop ;                    \ 6.1.1890
: /      /mod nip ;                     \ 6.1.0230

: min    2dup < ifz: swap | drop ;      \ 6.1.1870
: max    2dup swap < |ifz swap | drop ; \ 6.1.1880
: umin   2dup u< ifz: swap | drop ;
: umax   2dup swap u< |ifz swap | drop ;

: /string >r swap r@ + swap r> - ;      \ 17.6.1.0245  a u -- a+1 u-1
: within  over - >r - r> u< ;           \ 6.2.2440  u ulo uhi -- flag
: m*                                    \ 6.1.1810  n1 n2 -- d
    2dup xor 0< >r
    abs swap abs um*
    r> if dnegate then
;
: */mod  >r m* r> m/mod ;               \ 6.1.0110  n1 n2 n3 -- remainder n1*n2/n3
: */     */mod swap drop ;              \ 6.1.0100  n1 n2 n3 -- n1*n2/n3
: bye    1 user begin again ;           \ 15.6.2.0830  exit to OS if there is one

hex
: crc32  \ c-addr u -- crc              \ CRC32 of string, about 160 cycles per byte
   swap -1 >r                           ( u addr | crc )
   begin  over  while
      swap 1- swap
      count r> xor                      ( u addr' crc' )
      -8 begin  1+ >r                   \ do 8 times
         dup  1 and negate              ( u addr crc mask )
         EDB88320 and
         swap u2/ xor
      r> +until  drop  >r
   repeat
   2drop r> invert
;
decimal

\ This version expects two registers for the top of the data stack

: catch  \ xt -- exception# | 0         \ 9.6.1.0875
    over >r                             \ save N
    4 sp >r          \ xt               \ save data stack pointer
    handler @ >r     \ xt               \ and previous handler
    0 rp handler !   \ xt               \ set current handler = ret N sp handler
    execute                             \ execute returns if no throw
    r> handler !                        \ restore previous handler
    r> drop
    r> dup xor       \ 0                \ discard saved stack ptr
; call-only

: throw  \ ??? exc# -- ??? exc#         \ 9.6.1.2275
    dup ifz: drop exit |                \ Don't throw 0
    handler @ rp!   \ exc#              \ restore prev return stack
    r> handler !    \ exc#              \ restore prev handler
    r> swap >r      \ saved-sp          \ exc# is on return stack
    sp! drop nip
    r> r> swap      \ exc#              \ Change stack pointer
;
