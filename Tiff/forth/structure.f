\ Structures and words that use IMMEDIATE.

: \       tibs @ >in ! ; immediate      \ comment to EOL
: literal literal, ; immediate          \ compile a literal
: [char]  char literal, ; immediate     \ compile a char
: exit    ,exit ; immediate             \ compile exit
: chars   ; immediate
: (       [char] ) parse 2drop ; immediate

: [']   \ "name" --
   ' literal,
; immediate

\ Note (from Standard): [compile] is obsolescent and is included as a
\ concession to existing implementations.
\ : [compile] ( "name" -- )   ' compile, ;  immediate

: xtextc  \ head -- xte xtc
   8 - dup  cell+ link>  swap link>
;
: xtflag  \ ht -- xt flag
   xtextc ['] do-immediate  =  invert 2* 1+
;
: search-wordlist  \  c-addr u wid -- 0 | xt flag
   _hfind  dup if xtflag exit then  2drop
;

: postpone \ "name" --
   h'  xtextc                           \ xte xtc
   ['] do-immediate =                   \ immediate?
   if   compile, exit   then            \ compile it
   literal,  ['] compile, compile,      \ postpone it
; immediate
: recurse  \ --
   -4 last link> compile,
; immediate

\ ------------------------------------------------------------------------------
\ Control structures
\ see compiler.f

: NoExecute   \ --                      Must be compiling
   state @ 0= -14 and throw
;
: NeedSlot  \ slot -- addr
   NoExecute
   cp @ swap 64000 > if cell+ 2+ then   \ large address, leave room for >16-bit address
   c_slot c@ > if NewGroup then
   cp @
;
hex
: _branch   \ again --
   op_jmp Explicit                      \ addr slot
;
: _jump     \ addr -- then
   c_slot c@  -1 over lshift invert     \ create a blank address field
   _branch  18 lshift +                 \ pack the slot and address
   60000000 +							\ tag = 3, forward reference
;
: then      \ C: then --
   NoExecute  NewGroup  				\ then -- addr slot tag
   dup FFFFFF and
   swap 18 rshift
   dup 1F and  swap 5 rshift			\ addr slot tag
   3 <> -16 and throw					\ bogus tag
   -1 swap lshift  cp @ u2/ u2/ +       \ addr field
   swap ROM!
; immediate

: begin     \ C: -- again
   NoExecute  NewGroup  cp @
   40000000 +							\ tag = 2, backward mark
; immediate

: again     \ C: again --
   NoExecute  2/ 2/
   dup 38000000
   and 10000000 <> -16 and throw		\ expect tag of 2
   3FFFFF and _branch
; immediate

decimal

: ahead     \ C: -- then
   14 NeedSlot  _jump
; immediate
: ifnc      \ C: -- then
   20 NeedSlot
   op_ifc: Implicit  _jump
; immediate
: if        \ C: -- then | E: flag --
   20 NeedSlot
   op_ifz: Implicit  _jump
; immediate
: if+       \ C: -- then | E: n -- n
   20 NeedSlot
   op_-if: Implicit  _jump
; immediate
: else      \ C: then1 -- then2
   postpone ahead  swap
   postpone then
; immediate
: while     \ C: again -- then again | E: flag --
   NoExecute  >r  postpone if  r>
; immediate
: repeat    \ C: then again --
   postpone again
   postpone then
; immediate
: +until    \ C: again -- | E: n -- n
   20 NeedSlot drop
   op_-if: Implicit
   postpone again
; immediate
: until    \ C: again -- | E: flag --
   20 NeedSlot drop
   op_ifz: Implicit
   postpone again
; immediate
: rfor   \ C: -- again then | E: cnt -- R: -- cnt
   postpone begin   postpone (for)
   postpone ahead
; immediate
: for     \ C: -- again then | E: cnt -- R: -- cnt
   op_>r Implicit
   postpone rfor
; immediate
: next    \ C: -- again then | E: R: -- cnt
   swap  postpone again  postpone then
; immediate

\ CREATE DOES>
hex
: _create  \ "name" --
   cp @  ['] get-compile  header[
   1 [ pad 0F + ] literal c!            \ count byte = 1
   0C0 flags!                           \ flags: jumpable, anon
   ]header   NewGroup
;
: defer  \ <name> --
   _create
   3FFFFFF op_jmp Explicit
;
: is  \ xt --
   2/ 2/  4000000 -  '  ROM!            \ resolve forward jump
;
decimal

: (create)  \ -- xt | R: UA RA -- UA
   r> @+ swap @                         \ xt does>
   |-if drop exit |                     \ missing does>
   >r                                   \ do does>
;
: create  \ -- | -- n
   _create
   postpone (create)
   here  c_scope c@ 1 = 8 and +  ,c     \ skip forward if in ROM
   -1 ,c
;
: >body  \ xt -- body
   cell+ @
;
: (does)  \ RA --
   r>  -4 last link> cell+ cell+  ROM!  \ resolve the does> field
; call-only
: does>  \ patches created fields, pointed to by CURRENT
   postpone (does)
; immediate

\ Eaker CASE statement
\ Note: Number of cases is limited by the stack headspace.
\ For a depth of 64 cells, that's 32 items.

: case      \ C: -- 0
   0
; immediate
: of        \ C: -- addr slot | E: x1 x2 -- | --
   postpone (of)  postpone ahead
; immediate
: endof     \ C: -- addr1 slot1 -- addr2 slot2
   postpone else
; immediate
: endcase   \ C: 0 a1 s1 a2 s2 a3 s3 ... | E: x --
   postpone drop
   begin ?dup while
      postpone then
   repeat
; immediate

\ PAD is LEAVE stack

: do  \ -- then again
   NoExecute  NewGroup
   op_swap Implicit   op_com  Implicit
   op_1+   Implicit   op_>r   Implicit   op_>r   Implicit
   postpone begin  0 pad !
; immediate
: ?do  \ -- then again
   NoExecute
   postpone (?do)  postpone ahead  4 pad 2!
   postpone begin
; immediate

: pushLV  pad dup >r @+  dup cell+ r> ! + ! ;   \ n --
: popLV   pad dup dup @ + @ >r -4 swap +! r> ;  \ -- n

: leave  \ pad: <then> <then> <again> -- <then> <again>
   NoExecute  postpone unloop  postpone ahead  pushLV
; immediate

: _leaves  \ pad: <thens>...
   begin  pad @  while
      popLV  postpone then
   repeat
;
: loop
   NoExecute  postpone (loop)  postpone again  _leaves
; immediate
: +loop
   NoExecute  postpone (+loop)  postpone again  _leaves
; immediate
: i
   NoExecute  op_r@ Implicit
; immediate

\ Embedded strings

: ,"   \ string" --
   [char] " parse  ( addr len )
   dup >r c,c                           \ store count
   cp @ r@ ROMmove                      \ and string in code space
   r> cp +!
;
: _x"  \ string" --
   postpone (string)  ," alignc			\ compile embedded string
;
: ."   \ string" --
   _x"  op_c@+ Implicit
   postpone type
; immediate

: abort" \ string" -- | e: i * x x1 -- | i * x ) ( r: j * x -- | j * x
   postpone ."  \ "
   -1 throw
; immediate

\ Execution semantics of S" and C" need a temporary buffer.
\ As few as one buffer is allowed, but it can't be the TIB.
\ Let's just put it ahead in data space and assume there's room.

: transient"  \ string" -- a-addr
   [char] " parse
   dp @ 128 +  dup >r  over swap c!+    \ as u ad | ad
   swap cmove  r>
;

\ Solve the state-smartness of S" and C" by giving them custom xtcs.

:noname _x" op_c@+ Implicit ;           \ compilation action of S"
+: S"   transient" c@+ ;                \ -- a u
' _x" +: C"   transient" ;              \ -- a
