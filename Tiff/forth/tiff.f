\ Forth kernel for Tiff

: !                 !+ drop ; macro     \ x addr --
: +!        dup >r @ + r> ! ;           \ x addr --
: c!               c!+ drop ; macro     \ c addr --
: w!               w!+ drop ; macro     \ w addr --
: negate          invert 1+ ; macro     \ n -- -n
: cells               2* 2* ; macro     \ n -- n*4
: rot       >r swap r> swap ; macro     \ n m x -- m x n
: -rot      swap >r swap r> ; macro     \ n m x -- x n m
: 2drop           drop drop ; macro     \ d --
: 2dup            over over ; macro     \ d1 d2 -- d1 d2 d1
: tuck            swap over ; macro     \ ab -- bab
: nip             swap drop ; macro     \ ab -- b
: 3drop      drop drop drop ; macro     \ abc --
: ?dup        dup 0= -if: drop exit | ; \ n -- nn|0
: s>d  dup +if: dup dup xor exit | -1 ; \ n -- d
: =                  xor 0= ; macro     \ x y -- f
: <>              xor 0= 0= ; macro     \ x y -- f
: 0<>                 0= 0= ; macro     \ x y -- f
: aligned      2+ 1+ -4 and ;           \ n -- n'
: d2*          >r 2* r> 2*c ;           \ d -- d'
: abs    |-if negate | ;                \ n -- u
: 0<     |-if 0= 0= exit | 0= ;         \ n -- flag
: d+     >r swap >r + r> r> c+ ;        \ d1 d2 -- d3
: dnegate                               \ d -- -d
   invert >r invert 1+ r> |
   over 0= -if: drop 1+ exit | drop
;
: dabs   |-if dnegate exit | ;          \ n -- u

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
   dup +until
   drop >r >r drop r> r> swap
;

: um/mod_ov  \ ud u -- -1 -1
   drop drop drop -1 dup
;
: um/mod \ ud u -- ur uq               \ (untested)
   2dup swap - drop
   |ifc um/mod_ov exit |                \ check for overflow
   -64  dup 2* drop                     \ set carry
   begin                                \ L H divisor count
      >r >r   >r 2*c r> 2*c             \ L' H' | count divisor
      ifnc
         dup r@ - drop                  \ test subtraction
         |ifc r@ - |                    \ keep it
      else
         r@ -  0 2* drop                \ clear carry
      then
      r> r> 1+                          \ L' H' divisor count
   dup +until
   drop drop
;

: u<
   2dup xor
   |+if drop - 0< exit |
   drop >r drop r> 0<
;
: <                                     \ n n -- f
   2dup xor                             \ ANS style
   |+if drop - 0< exit |
   drop drop 0<
;

: u>      swap u< ;                     \ u1 u2 -- f
: >       swap < ;                      \ n n -- f
: *       um* drop ;















: here      cp @ ;                      \ -- CodeAddr
: allot     cp +! ;                     \ bytes --
: ,         cp dup >r @ 4 r> +!   ! ;   \ x --
: w,        cp dup >r @ 2 r> +!  w! ;   \ w --
: c,        cp dup >r @ 1 r> +!  c! ;   \ c --



\ Define a compiler
\ char params:  c_colondef c_litpend c_slot c_called

defer FlushLit
defer NewGroup

: ClearIR   \ --                        \ Initialize the IR. slot=26, litpend=0
   0 iracc !  26 c_slot w!  0 c_called c!
;
: AppendIR  \ opcode imm                \ insert opcode at slot, adding imm
   swap  c_slot c@ lshift  iracc @      \ shift it and get IR
   +  +                    iracc !      \ merge into instruction
;

: Implicit  \ opcode                    \ add implicit opcode to group
   FlushLit
   c_slot c@ 0=  over 60 and  and if    \ doesn't fit in the last slot
      NewGroup
   then
   0 AppendIR
   c_slot c@  dup if
      6 - -if: dup xor |                \ sequence: 26, 20, 14, 8, 2, 0
      c_slot c! exit
   then
   drop NewGroup                        \ slot 0 -> slot 26
;

: Explicit  \ opcode imm
   FlushLit
   1  c_slot c@  dup >r  lshift         \ opcode imm maximum
   over - r> 0= and |+if NewGroup |     \ it doesn't fit
   drop AppendIR
   c_slot c@ 24 lshift
   cp @ +  calladdr !                   \ remember call address in case of ;
   0 c_slot c!  NewGroup
;


: HardLit  \ n --
   drop
;

:noname \ FlushLit  \ --                \ compile a literal if it's pending
   c_litpend c@ if
      0 c_litpend c!
      nextlit @ HardLit
   then
   0 c_called c!                        \ everything clears the call tail
; is FlushLit

:noname \ NewGroup  \ --                \ finish the group and start a new one
    FlushLit                            \ if a literal is pending, compile it
    c_slot c@
    dup 21 - +if: 2drop exit |          \ already at first slot
    drop if
       op_no: Implicit                  \ skip unused slots
    then
    iracc @ ,  ClearIR
; is NewGroup

\
\
\  \ Append a zero-operand opcode
\  \ Slot positions are 26, 20, 14, 8, 2, and 0
\
\  static void Implicit (int opcode) {
\      FlushLit();
\      if ((FetchByte(SLOT) == 0) && (opcode > 3))
\          NewGroup();                     // doesn't fit in the last slot
\      AppendIR(opcode, 0);
\      int8_t slot = FetchByte(SLOT);
\      if (slot) {
\          slot -= 6;
\          if (slot < 0) slot = 0;
\          StoreByte(slot, SLOT);
\      } else {
\          NewGroup();
\      }
\  }
\
\ First, let's wean the pre-built headers off the C functions.
\ Note that replace-xt is only available in the host system due to
\ the constraints of run-time flash programming.

\ EQU pushes or compiles W parameter
: equ_e   head @ -16 + @ ;      \ -- n
' equ_e  -1 replace-xt          \ execution part of EQU


\ Some kernel words

: decimal  10 base ! ;          \ --
: hex      16 base ! ;          \ --
: foo hex decimal ;


