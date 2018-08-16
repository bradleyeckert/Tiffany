\ compiler (not tested yet)
\ incomplete

\ Data space is in initialized RAM.
\ Code and header space is in SPI flash, accessed by @AS and !AS.

: here      dp @ ;                      \ -- DataAddr
: allot     dp +! ;                     \ bytes --
: (aligned) over and 0<> -23 and throw ; \ a mask -- a
: (,x)      3 (aligned)  dup >r @  !+  r>  !+ drop ; \ x a --
: ,         dp (,x) ;                   \ x --
: (w,x)     1 (aligned)  dup >r @  w!+  r>  !+ drop ; \ w a --
: w,        dp (w,x) ;                  \ w --
: (c,x)     dup >r @  c!+  r>  !+ drop ; \ w a --
: c,        dp (c,x) ;                  \ c --

: w,     cp dup >r @ w!+  r> w!+ drop ; \ w --
: c,     cp dup >r @ c!+  r> c!+ drop ; \ c --
: h,     hp dup >r @  !+  r>  !+ drop ; \ x --
: hw,    hp dup >r @ w!+  r> w!+ drop ; \ w --
: hc,    hp dup >r @ c!+  r> c!+ drop ; \ c --



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
   c_slot c@  24 lshift
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

