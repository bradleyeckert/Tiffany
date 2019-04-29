\ Structures and words that use IMMEDIATE.

: \       tibs @ >in ! ; immediate      \ 6.2.2535  --
: literal literal, ; immediate          \ 6.1.1780  x --
: [char]  char literal, ; immediate     \ 6.1.2520  <xchar> --
: exit    ,exit ; immediate             \ 6.1.1380
: chars   ; immediate                   \ 6.1.0898
: (       [char] ) parse 2drop
; immediate

: [']   \ "name" --                     \ 6.1.2510
   ' literal,
; immediate

: xtextc  \ head -- xte xtc
   8 - dup  cell+ link>  swap link>
;
: xtflag  \ ht -- xt flag
   xtextc ['] do-immediate  =  invert 2* 1+
;
: search-wordlist                       \ 16.6.1.2192
\ c-addr u wid -- 0 | xt flag
   _hfind  dup if xtflag exit then  2drop
;

: postpone \ "name" --                  \ 6.1.2033
   h'  xtextc                           \ xte xtc
   ['] do-immediate =                   \ immediate?
   if   compile, exit   then            \ compile it
   literal,  ['] compile, compile,      \ postpone it
; immediate

: recurse  \ --                         \ 6.1.2120
   -4 last link> compile,
; immediate

\ ------------------------------------------------------------------------------
\ Control structures
\ see compiler.f

: NoExecute   \ --                      \ Must be compiling
   state @ 0= -14 and throw
;
: NeedSlot  \ slot -- addr
   NoExecute
   cp @ swap 64000 > if cell+ 1+ 1+ then \ large address, leave room for >16-bit address
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
: then      \ C: then --                \ 6.1.2270
   NoExecute  NewGroup  				\ then -- addr slot tag
   dup FFFFFF and
   swap 18 rshift
   dup 1F and  swap 5 rshift			\ addr slot tag
   3 <> -16 and throw					\ bogus tag
   -1 swap lshift  cp @ u2/ u2/ +       \ addr field
   swap ROM!
; immediate

: begin     \ C: -- again               \ 6.1.0760
   NoExecute  NewGroup  cp @
   40000000 +							\ tag = 2, backward mark
; immediate

: again     \ C: again --               \ 6.2.0700
   NoExecute  2/ 2/
   dup 38000000
   and 10000000 <> -16 and throw		\ expect tag of 2
   3FFFFF and _branch
; immediate

decimal

: ahead     \ C: -- then                \ 15.6.2.0702
   14 NeedSlot  _jump
; immediate
: ifnc      \ C: -- then
   20 NeedSlot
   op_ifc: Implicit  _jump
; immediate
: if        \ C: -- then | E: flag --   \ 6.1.1700
   20 NeedSlot
   op_ifz: Implicit  _jump
; immediate
: +if       \ C: -- then | E: n -- n
   20 NeedSlot
   op_-if: Implicit  _jump
; immediate
: else      \ C: then1 -- then2         \ 6.1.1310
   postpone ahead  swap
   postpone then
; immediate
: while     \ C: again -- then again    \ 6.1.2430
   NoExecute  >r  postpone if  r>
; immediate \ E: flag --
: +while    \ C: again -- then again
   NoExecute  >r  postpone +if  r>
; immediate \ E: flag --
: repeat    \ C: then again --          \ 6.1.2140
   postpone again
   postpone then
; immediate
: +until    \ C: again -- | E: n -- n
   20 NeedSlot drop
   op_-if: Implicit
   postpone again
; immediate
: until    \ C: again -- | E: flag --   \ 6.1.2390
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
: _create  \ n "name" --
   cp @  ['] get-compile  header[
   dup  pad w!+  c!                 \ group count = n, tag byte = n
   0C0 flags!                           \ flags: jumpable, anon
   ]header   NewGroup
;
: defer  \ <name> --
   1 _create
   3FFFFFF op_jmp Explicit
;
: is  \ xt --
   2/ 2/  4000000 -  '  ROM!            \ resolve forward jump
;
: create  \ -- | -- addr                \ 6.1.1000
   2 _create  align
   here  c_scope c@ 1 = 8 and +  literal,  \ skip forward if in ROM
   op_exit  Implicit
   iracc @
   -40 c_slot c@ lshift invert +		\ last group is "exit" or "com exit"
   ,c  ClearIR
;

: iscom?  \ 'group -- flag				\ does the group have a COM in slot 0?
   @ [ -1   1A lshift ] literal  and
   [ op_com 1A lshift ] literal  xor 0=
;

: _>body  \ xt -- body link flag		\ flag = T if there is no DOES> field
   @+ FFFFF and  swap          			( literal 'group )
   dup @ >r  iscom? if
      invert  r> 3FFF
   else
      r> FFFFF							( body group mask )
   then
   dup >r and  dup r> =
;

: >body  \ xt -- body                   \ 6.1.0550
   _>body  2drop
;

decimal

\ DOES> ends the current thread and causes the last CREATE to pick it up.
\ The COM EXIT or EXIT are cleared to COM NOP or NOP so that a JMP is executed.

: does>  \ RA --                        \ 6.1.1250
   r> 2/ 2/                             \ jump here, jdest
   -4 last link> cell+  dup >r
   iscom? if
	  [ op_jmp 14 lshift  op_exit invert 20 lshift  + ] literal
   else \ slot0 = com, must be "com exit"
	  [ op_jmp 20 lshift  op_exit invert 26 lshift  + ] literal
   then
   +  r> ROM!           				\ resolve the does> field
;

\ Eaker CASE statement
\ Note: Number of cases is limited by the stack headspace.
\ For a depth of 64 cells, that's 32 items.

: case      \ C: -- 0                   \ 6.2.0873
   0
; immediate
: of        \ C: -- then                \ 6.2.1950
   postpone (of)  postpone ahead
; immediate \ E: x1 x2 -- | --
: endof     \ C: then -- again          \ 6.2.1343
   postpone else
; immediate
: endcase   \ C: 0 a1 a2 ... | E: x --  \ 6.2.1342
   postpone drop
   begin ?dup while
      postpone then
   repeat
; immediate

\ PAD is LEAVE stack

: do  \ -- then again                   / 6.1.1240
   NoExecute  NewGroup
   op_swap Implicit   op_com  Implicit
   op_1+   Implicit   op_>r   Implicit   op_>r   Implicit
   postpone begin  0 pad !
; immediate
: ?do  \ -- then again                  / 6.2.0620
   NoExecute
   postpone (?do)  postpone ahead  4 pad 2!
   postpone begin
; immediate

: pushLV  pad dup >r @+  dup cell+ r> ! + ! ;   \ n --
: popLV   pad dup dup @ + @ >r -4 swap +! r> ;  \ -- n

: leave  \ pad: ... -- ... <then>       \ 6.1.1760
   NoExecute  postpone unloop  postpone ahead  pushLV
; immediate

: _leaves  \ pad: <thens>...
   begin  pad @  while
      popLV  postpone then
   repeat
;
: loop  \ again --                      \ 6.1.1800
   NoExecute  postpone (loop)  postpone again  _leaves
; immediate
: +loop  \ again --                     \ 6.1.0140
   NoExecute  postpone (+loop)  postpone again  _leaves
; immediate
: i  \ index                            \ 6.1.1680
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
: ."   \ string" --                     \ 6.1.0190
   _x"  op_c@+ Implicit
   postpone type
; immediate

: abort  -1 throw ;  \ --               \ 6.1.0670

: abort" \ ( string" -- | e: i * x x1 -- | i * x ) ( r: j * x -- | j * x )
   postpone if                          \ 6.1.0680
   postpone ."  \ "
   postpone abort
   postpone then
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
+: S"   transient" c@+ ;                \ 6.1.2165  -- a u
' _x" +: C"   transient" ;              \ 6.2.0855  -- a
