\ Mforth compiler

\ Compiling to known-blank flash memory.

\ The VM's !AS instruction is used to program erased cells.
\ Non-blank cells are assumed safe to write as long as the new data doesn't
\ write '0's to existing '0' bits.

: ROM!  \ n addr --
   swap  hld dup >r @ r> swap >r >r 	\ save HLD ( R: hld 'hld )
   r@ !   r@ swap                  		\ hld=n ( as ad )
   0 !as  drop drop						\ store 1 cell to AXI space
   r> r> swap !							\ restore HLD
;
: ROMC!  \ c addr --
   >r  invert 255 and   				\ ~c addr
   r@ 3 and 2* 2* 2* lshift       		\ c' | addr
   invert  r> -4 and ROM!				\ write aligned c into blank slot
;

\ Code and headers are both in ROM.

: ram  0 c_scope c! ;                   \ use RAM scope
: rom  1 c_scope c! ;                   \ use flash ROM scope
: h   c_scope c@ if CP exit then DP ;   \ scoped pointers
: ,x  dup >r @ ROM!  4 r> +! ;          \ n addr --
: ,c  cp ,x ;                           \ n --
: ,h  hp ,x ;                           \ n --
: ,d  dp dup >r @ !  4 r> +! ;          \ n --
: ,   c_scope c@ if ,c exit then ,d ;   \ n --

: c,x                                   \ c addr --
   dup >r @ ROMC!  r@ @ 1+ r> !
;
: c,c  cp c,x ;                         \ c --
: c,h  hp c,x ;                         \ c --
: c,d  dp dup >r @ c!  1 r> +! ;        \ c --
: c,   c_scope c@ if c,c exit then c,d ;

: ROMmove  ( as ad n -- )
   negate begin >r
      over c@ over ROMC!
	  1+ swap 1+ swap
   r> 1+ +until
   drop drop drop
;

\ Equates for terminal variables puts copies in header space. See tiff.c/tiff.h.
\ Some are omitted to save header space.

handler        equ handler
base           equ base
hp             equ hp
cp             equ cp
dp             equ dp
state          equ state
current        equ current
source-id      equ source-id
personality    equ personality
tibs           equ tibs
tibb           equ tibb
>in            equ >in
c_wids         equ c_wids
c_casesens     equ c_casesens
\ head           equ head
context        equ context
forth-wordlist equ forth-wordlist
hld            equ hld
blk            equ blk
tib            equ tib
\ maxtibsize   equ maxtibsize
|pad|          equ |pad|
pad            equ pad

\ Define a compiler
\ char params:  c_colondef c_litpend c_slot c_called

defer FlushLit
defer NewGroup

: ClearIR   \ --                        \ Initialize the IR. slot=26, litpend=0
   0 iracc !  26 c_slot w!  0 c_called c!
;
: AppendIR  \ opcode imm --             \ insert opcode at slot, adding imm
   swap  c_slot c@ lshift  iracc @      \ shift it and get IR
   +  +                    iracc !      \ merge into instruction
;

: Implicit  \ opcode --                 \ add implicit opcode to group
   FlushLit
   c_slot c@ 0=  over 60 and  and if    \ doesn't fit in the last slot
      NewGroup
   then
   0 AppendIR
   c_slot c@  dup if
      6 -  dup 0< invert and            \ sequence: 26, 20, 14, 8, 2, 0
      c_slot c! exit
   then
   drop NewGroup                        \ slot 0 -> slot 26
;

: Explicit  \ imm opcode --
   swap FlushLit
   1  c_slot c@  dup >r  lshift         \ opcode imm maximum | slot
   over <  r> 0= or                     \ imm doesn't fit of slot=0
   if  NewGroup  then
   AppendIR
   c_slot c@  24 lshift
   cp @ +  calladdr !                   \ remember call address in case of ;
   0 c_slot c!  NewGroup
;

hex
: HardLit  \ n --                       \ compile a hard literal
   dup >r -if: negate |                 \ u
   1FFFFFF invert 2* over and if        \ too wide
      r> drop   dup 18 rshift  op_lit Explicit   \ upper part
      FFFFFF and  op_litx Explicit      \ lower part
      exit
   then
   r> 0< if                             \ compile negative
      1- op_lit Explicit
      op_com Implicit  exit
   then
   op_lit Explicit                      \ compile positive
;

:noname \ FlushLit  \ --                \ compile a literal if it's pending
   c_litpend c@ if
      0 c_litpend c!
      nextlit @ HardLit
   then
   0 c_called c!                        \ everything clears the call tail
; is FlushLit

:noname \ NewGroup  \ --                \ finish the group and start a new one
   FlushLit                             \ if a literal is pending, compile it
   c_slot c@
   dup 15 - +if: 2drop exit |           \ already at first slot
   drop if
      op_no: Implicit                   \ skip unused slots
   then
   iracc @ ,c
   ClearIR
; is NewGroup

: literal,  \ n --                      \ compile a literal
   FlushLit
   1FFFFFF invert 2*  over and
   if HardLit exit then                 \ oversized literal, compile it now
   nextlit !  1 c_litpend c!            \ literal is pending
;
: compile,  \ xt --                     \ compile a call
   2/ 2/ op_call Explicit
   head @ 4 + c@  80 and  c_called c!   \ 0 = call-only
;
: ,exit  \ --
   c_called c@ if
      calladdr link>
      8  [ calladdr 3 + ] literal c@  lshift
      invert swap ROM!                  \ convert CALL to JUMP
      0 c_called c!
      0 calladdr ! exit
   then op_exit Implicit
;
decimal
