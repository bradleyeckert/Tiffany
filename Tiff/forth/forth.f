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

: (of)  \ x1 x2 R: RA -- x1 x2 R: RA | R: RA+4
   over over xor 0= if                  \ if match
      drop drop  r> cell+ >r exit       \ then skip branch
   then
; call-only

\ ------------------------------------------------------------------------------
\ Control structures
\ see compiler.f

: NoExecute   \ --                      Must be compiling
   exit \ for testing
   state @ 0<> -14 and throw
;
: NeedSlot  \ slot -- addr
   NoExecute
   cp @ swap 0xF000 > if cell+ 2+ then  \ large address, leave room for >16-bit address
   c_slot c@ > if NewGroup then
   cp @
;
: _jump   \ -- slot
   c_slot c@  -1 over lshift invert     \ create a blank address field
   op_jmp Explicit
;
: ahead   \ -- addr slot
   14 NeedSlot  _jump
; immediate
: ifnc   \ -- addr slot
   20 NeedSlot
   op_ifc: Implicit  _jump
; immediate
: _if   \ -- addr slot
   20 NeedSlot
   op_ifz: Implicit  _jump
; immediate
: if+   \ -- addr slot
   20 NeedSlot
   op_-if: Implicit  _jump
; immediate

: _then \ addr slot --
   NoExecute  NewGroup
   over 0= if                           \ ignore dummy
      2drop exit
   then
   -1 swap lshift  cp @ u2/ u2/ +       \ addr field
   swap SPI!
; immediate
: _else  \ fa fs -- fa' fs'
   postpone ahead  2swap
   postpone _then
; immediate

0 [if]
void CompBegin (void){  // ( -- addr )
    NewGroup();  NoExecute();
    PushNum(FetchCell(CP));
}
void CompAgain (void){  // ( addr -- )
    NoExecute();
    uint32_t dest = PopNum();
    Explicit(opJUMP, dest>>2);
}
void CompWhile (void){  // ( addr -- addr' slot addr )
    uint32_t beg = PopNum();
    CompIf();
    PushNum(beg);
}
void CompRepeat (void){  // ( addr2 slot2 addr1 )
    CompAgain();
    CompThen();
}
void CompPlusUntil (void){  // ( addr -- )
    NeedSlot(20);
    uint32_t dest = PopNum();
    Implicit(opSKIPGE);
    Explicit(opJUMP, dest>>2);
}
void CompUntil (void){  // ( addr -- )
    NeedSlot(20);
    uint32_t dest = PopNum();
    Implicit(opSKIPNZ);
    Explicit(opJUMP, dest>>2);
}
[then]

0 [if]
: OF ( -- ofsys )
   POSTPONE (OF) (BEGIN) POSTPONE 2DROP ;  IMMEDIATE

: ENDOF ( casesys ofsys -- casesys )
   POSTPONE (ELSE)  >RESOLVE  HERE 4 - !  (BEGIN) ; IMMEDIATE

: ENDCASE ( casesys -- )
   POSTPONE DROP
   BEGIN
      ?DUP WHILE
      -?>RESOLVE
   REPEAT -BAL ;  IMMEDIATE

: CASE ( -- casesys )
   +BAL 0 ; IMMEDIATE
[then]



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
