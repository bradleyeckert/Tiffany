\ High level Forth

\ Words that use INTERPRET, IMMEDIATE, etc.

: \    tibs @ >in ! ; immediate         \ comment to EOL
: literal  literal, ; immediate         \ compile a literal
: [char]  char literal, ; immediate     \ compile a char
: exit    ,exit ; immediate             \ compile exit

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

: postpone ( "name" -- )
   h'  8 - dup  cell+ link>  swap link> \ xte xtc
   ['] do-immediate =                   \ immediate?
   if   compile, exit   then            \ compile it
   literal,  ['] compile, compile,      \ postpone it
; immediate

\ ------------------------------------------------------------------------------
\ Control structures
\ see compiler.f

: NoExecute   \ --                      Must be compiling
   state @ 0= -14 and throw
;
: NeedSlot  \ slot -- addr
   NoExecute
   cp @ swap 0xF000 > if cell+ 2+ then  \ large address, leave room for >16-bit address
   c_slot c@ > if NewGroup then
   cp @
;
: _jump     \ -- slot
   c_slot c@  -1 over lshift invert     \ create a blank address field
   op_jmp Explicit
;
: defer  \ <name> --
   cp @  ['] get-compile  header[
   1 [ pad 15 + ] literal c!            \ count byte = 1
   192 flags!                           \ flags: jumpable, anon
   ]header   NewGroup
   0x3FFFFFF op_jmp Explicit
;
: is  \ xt --
   2/ 2/  67108864 -  '  SPI!           \ resolve forward jump
;
: ahead     \ C: -- addr slot
   14 NeedSlot  _jump
; immediate
: ifnc      \ C: -- addr slot
   20 NeedSlot
   op_ifc: Implicit  _jump
; immediate
: if        \ C: -- addr slot | E: flag --
   20 NeedSlot
   op_ifz: Implicit  _jump
; immediate
: if+       \ C: -- addr slot | E: n -- n
   20 NeedSlot
   op_-if: Implicit  _jump
; immediate
: then      \ C: addr slot --
   NoExecute  NewGroup
   over 0= if                           \ ignore dummy
      2drop exit
   then
   -1 swap lshift  cp @ u2/ u2/ +       \ addr field
   swap SPI!
; immediate
: else      \ C: fa fs -- fa' fs'
   postpone ahead  2swap
   postpone then
; immediate
: begin     \ C: -- addr
   NoExecute cp @
; immediate
: again     \ C: addr --
   NoExecute  2/ 2/ op_jmp Explicit
; immediate
: while     \ C: addr1 -- addr2 slot2 addr1 | E: flag --
   >r postpone if r>
; immediate
: repeat    \ C: addr2 slot2 addr1 --
   postpone again
   postpone then
; immediate
: +until    \ C: addr -- | E: n -- n
   20 NeedSlot
   op_-if: Implicit  _jump
; immediate
: until    \ C: addr -- | E: flag --
   20 NeedSlot
   op_ifz: Implicit  _jump
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

: does>  \ patches created fields, pointed to by CURRENT
   8 clr-xtcbits                        \ see wean.f
   8 clr-xtebits
   0 c_colondef c!                      \ no header to tweak
   r>  -20 last  SPI!                   \ resolve does> address
; call-only                             \ and terminate CREATE clause

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
