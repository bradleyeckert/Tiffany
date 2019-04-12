\ High level Forth

\ Words that use INTERPRET, IMMEDIATE, etc.

: \    tibs @ >in ! ; immediate         \ comment to EOL
: literal  literal, ; immediate         \ compile a literal
: [char]  char literal, ; immediate     \ compile a char
: exit    ,exit ; immediate             \ compile exit
: chars   ; immediate

\ Header space:     W len xtc xte link name
\ offset from ht: -16 -12  -8  -4    0 4

: h'  \ "name" -- ht
   parse-word  hfind  swap 0<>          \ len -1 | ht 0
   -13 and throw                        \ header not found
;
: '   \ "name" -- xte
   h' invert cell+ invert link>
;
: [']   \ "name" --
   ' literal,
; immediate

\ Note (from Standard): [compile] is obsolescent and is included as a
\ concession to existing implementations.
\ : [compile] ( "name" -- )   ' compile, ;  immediate

: xtextc  \ head -- xte xtc
   8 - dup  cell+ link>  swap link>
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
: addrslot  \ token -- addr slot
\   NoExecute
   dup 16777215 and
   swap 24 rshift
;
: _branch   \ dest --
   op_jmp Explicit                      \ addr slot
;
: _jump     \ addr -- token
   c_slot c@  -1 over lshift invert     \ create a blank address field
   _branch  24 lshift +                 \ pack the slot and address
;
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
   2/ 2/  4000000 -  '  SPI!            \ resolve forward jump
;

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
: (does)  \ RA
   r@  -4 last link> cell+ cell+  SPI!       \ resolve the does> field
;
: does>  \ patches created fields, pointed to by CURRENT
   postpone (does)
; immediate


decimal
: ahead     \ C: -- token
   14 NeedSlot  _jump
; immediate
: ifnc      \ C: -- token
   20 NeedSlot
   op_ifc: Implicit  _jump
; immediate
: if        \ C: -- token | E: flag --
   20 NeedSlot
   op_ifz: Implicit  _jump
; immediate
: if+       \ C: -- token | E: n -- n
   20 NeedSlot
   op_-if: Implicit  _jump
; immediate
: then      \ C: token --
   NewGroup  addrslot
   -1 swap lshift  cp @ u2/ u2/ +       \ addr field
   swap SPI!
; immediate
: else      \ C: token1 -- token2
   postpone ahead  swap
   postpone then
; immediate
: begin     \ C: -- addr
   NoExecute  NewGroup  cp @
; immediate
: again     \ C: addr --
   NoExecute  2/ 2/ _branch
; immediate
: while     \ C: addr1 -- addr2 token | E: flag --
   NoExecute  >r  postpone if  r>
; immediate
: repeat    \ C: addr token --
   postpone again
   postpone then
; immediate
: +until    \ C: addr -- | E: n -- n
   20 NeedSlot drop
   op_-if: Implicit
   2/ 2/ _branch
; immediate
: until    \ C: addr -- | E: flag --
   20 NeedSlot drop
   op_ifz: Implicit
   2/ 2/ _branch
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

: do  \ -- addr
   NoExecute  NewGroup
   op_swap Implicit   op_com  Implicit
   op_1+   Implicit   op_>r   Implicit   op_>r   Implicit
   postpone begin  0 pad !
; immediate
: ?do  \ -- addr
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
    dup >r c,c                          \ store count
    cp @ r@ SPImove                     \ and string
    r> cp +!
;

: ."   \ string" --
    cp @  8 +  literal,                 \ point to embedded string
    postpone ahead  ," alignc
    postpone then
    op_c@+ Implicit
    postpone type
; immediate

: find  \ c-addr -- c-addr 0 | xt flag
   dup count hfind  over if             \ c-addr addr len
      drop dup xor exit                 \ not found
   then
   >r drop drop r>  xtextc              \ xte xtc
   ['] do-immediate xor 0<> 2* 1+
;

: unused    \ -- n
   c_scope c@
   case
   0 of  [ RAMsize ROMsize + ] literal  here -  endof    \ Data
   1 of  SPIflashSize  here -  endof
         codespace hp @ -  swap
   endcase
;
cp @ ," DataCodeHead" 1+
: .unused  \ --
   literal                              \ sure is nice to pull in external literal
   3 0 do                               \ even it's not ANS
      i c_scope c!
      cr dup i cells + 4 type
      ." : " unused .
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
