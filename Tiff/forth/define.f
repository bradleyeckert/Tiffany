\ Defining words

\ Create a new header. The source is the keyboard, file, or blocks.
\ `include` (in tiff.c) saves the filename and manages c_fileid and w_linenum.

\ |Cell| \[31:24\]                        | \[23:0\]                           |
\ |----|:---------------------------------| ----------------------------------:|
\ | -3 | Source File ID                   | List of words that reference this  |
\ | -2 | Source Line, Low byte            | xtc, Execution token for compile   |
\ | -1 | Source Line, High byte           | xte, Execution token for execute   |
\ | 0  | # of instructions in definition  | Link                               |

: header[  \ <name> xte xtc --          \ populate PAD with header data
   pad 28 -1 fill                       \ default fields are blank
   [ pad 4 + ] literal !+ !+
   current @ @ swap !+                  \ add xtc, xte, link
   >r parse-word dup r> c!+             \ store length
   swap cmove
   c_fileid c@ [ pad 3 + ] literal c!   \ file ID
   w_linenum c@+  >r c@ r>              \ hi lo
   [ pad 7 + ] literal c!
   [ pad 11 + ] literal c!              \ insert line number
;
: ]header  \ --                         \ write PAD to flash
   pad  16 + c@ 31 and  1+ aligned  16 + >r
   pad  hp @  r@  SPImove               \ write to flash
   hp @ 12 + current @ !                \ add to current definitions
   r> hp +!
;
: flags!  \ c --                        \ change flags (from 0)
   [ pad 16 + ] literal c+!
;

: last  current @ @ + ;       \ n -- a  \ index into last defined header
: clr-xtcbits  invert -8 last SPI! ;    \ n --   flip bits in the current xtc
: clr-flagbits  invert 4 last SPI! ;    \ n --   flip bits in the flags
                                        \ see xtc address rules in wean.f
: macro      4 clr-xtcbits ;  \ --      \ flip xtc from compile to macro
\ : immediate  8 clr-xtcbits ;  \ --      \ flip xtc from compile to immediate
: call-only  128 clr-flagbits ;  \ --   \ clear the jumpable bit

: ]  1 state ! ;              \ --      \ resume compilation

: chere  NewGroup cp @ ;                \ -- xt  of new code
: :noname  chere ] ;                    \ <name> -- xt

\ In a dual-xt flash Forth, you want to compile the header with the desired xtc
\ upon defining the execution part. `::` is like `:` with a custom compilation
\ action. It's used in lieu of state-smart words. This syntax avoids the need to
\ patch flash later, which can't be done to an arbitrary xtc.

: <:>  \ xte xtc <name> --              \ generic definition
   header[   224 flags!                 \ flags: jumpable, anon, smudged
   255 [ pad 15 + ] literal c!          \ blank count byte
   ]header
   1 c_colondef c!  ]
;

: comp_oops  -14 throw ;                \ Interpreting a compile-only word
: :    chere  ['] get-compile  <:> ;    \ <name> --   define a word
: immediate:       chere  dup  <:> ;    \ <name> --   define an immediate word
: compile:  ['] comp_oops  chere  <:> ; \ <name> --   define a compile-only word

immediate: [  0 state ! ;     \ --      \ interpret

immediate: ;  \ --
   ,exit  NewGroup
   4 last c@  32 and if                 \ smudge bit is set?
      32 clr-flagbits                   \ clear it
   then
   c_colondef c@ if                     \ wid
      0 c_colondef c!
      cp @  -4 last link> - 2/ 2/       \ length
      255 min
      pad tuck c!  3 last  1 SPImove
   then
   0 state !
;

: equ  \ n --                           \ new equ
   ,h  ['] equ_ee  ['] equ_ec
   header[  ]header
;

: h'  \ <name> -- ht                    \ get header token
   parse-word hfind
   swap 0<> -13 and throw
;
: '  \ -- xt                            \ get xt
   h' 4 - link>
;
: defer  \ <name> --                    \ new defer
   chere ['] get-compile header[
   192 flags!                           \ flags: jumpable, anon
   1 [ pad 15 + ] literal c!            \ length = 1
   ]header
   67108863  op_jmp Explicit            \ unresolved jump
;
: is  \ <name>
   2/ 2/  mask24 invert +  ' SPI!       \ resolve the jump
;
immediate: [']  ' literal, ;            \ <name> --

immediate: exit  ,exit ;

\ Postpone depends on whether a word is immediate. It determines a word is
\ immediate by comparing its xte and xtc. An immediate word gets compiled now.
\ Otherwise, its xte is compiled as a literal and compile, is compiled.

immediate: postpone  \ <name>
   h' 8 - 2@ over xor mask24 and        \ xte notsame
   if  literal,  ['] compile,  compile,  exit
   then  compile,
;

\ A classic state smart word is S". If interpreting, a string is returned on the stack. If compiling, it's compiled.
\ Multiple interpreted S" returns different parts of the TIB. However, you can't put them on multiple lines.

: _,"  \ <str>                           \ compile counted string
   [char] " parse  dup >r c,
   cp @  r@  SPImove
   r> cp +!
;

: _ahead  \ -- slot                     \ compile forward jump
   c_slot c@
   -1 over lshift invert
   op_jmp  Explicit                     \ address is all '1's
;

: _slot  \ slot --
   c_slot c@ swap < if NewGroup then
;

compile: ahead  \ -- addr slot          \ compile forward jump
   14 _slot  cp @  _ahead
;

compile: if  \ -- addr slot             \ compile conditional forward jump
   20 _slot
   cp @  op_ifz: Implicit  _ahead
;

compile: then  \ addr slot --           \ resolve if
   NewGroup
   -1 swap lshift  cp @ 2/ 2/ +  swap SPI!
;

\ compile: else  \ addr slot -- addr slot
\    NewGroup  cp @  _ahead
\ ;

\ void CompElse (void){  // ( addr slot -- addr' slot' )
\     NewGroup();  NoExecute();
\     uint32_t cp =   FetchCell(CP);
\     uint32_t slot = FetchByte(SLOT);
\     Explicit(opJUMP, 0x3FFFFFF);
\     CompThen();
\     PushNum(cp);
\     PushNum(slot);
\ }
\ void CompBegin (void){  // ( -- addr )
\     NewGroup();  NoExecute();
\     PushNum(FetchCell(CP));
\ }
\ void CompAgain (void){  // ( addr -- )
\     NoExecute();
\     uint32_t dest = PopNum();
\     Explicit(opJUMP, dest>>2);
\ }
\ void CompWhile (void){  // ( addr -- addr' slot addr )
\     uint32_t beg = PopNum();
\     CompIf();
\     PushNum(beg);
\ }
\ void CompRepeat (void){  // ( addr2 slot2 addr1 )
\     CompAgain();
\     CompThen();
\ }
\ void CompPlusUntil (void){  // ( addr -- )
\     NoExecute();
\     if (FetchByte(SLOT)<20)
\         NewGroup();
\     uint32_t dest = PopNum();
\     Implicit(opSKIPGE);
\     Explicit(opJUMP, dest>>2);
\ }
\ void CompUntil (void){  // ( addr -- )
\     NoExecute();
\     if (FetchByte(SLOT)<20)
\         NewGroup();
\     uint32_t dest = PopNum();
\     Implicit(opSKIPNZ);
\     Explicit(opJUMP, dest>>2);
\

