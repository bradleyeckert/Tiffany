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
   pad  dup
   16 + c@ 31 and  1+ aligned  16 + >r
   hp @  r@  SPImove                    \ write to flash
   hp @ 12 + current @ !                \ add to current definitions
   r> hp +!
;
: flags!  \ c --                        \ change flags (from 0)
   [ pad 16 + ] literal c+!
;
: equ  \ n --                           \ new equ
   ['] equ_ee  ['] equ_ec
   header[ ,h ]header
;

: last  current @ @ + ;       \ n -- a  \ index into last defined header
: clr-xtcbits  invert -8 last SPI! ;    \ n --   flip bits in the current xtc
: clr-flagbits  invert 4 last SPI! ;    \ n --   flip bits in the flags
: macro      4 clr-xtcbits ;  \ --      \ flip xtc from compile to macro
: immediate  8 clr-xtcbits ;  \ --      \ flip xtc from compile to immediate
: call-only  128 clr-flagbits ;  \ --   \ clear the jumpable bit

: ]  1 state ! ;              \ --      \ resume compilation
: [  0 state ! ; immediate    \ --      \ interpret

: :  \ <name> --                        \ new :
   cp @ ['] get-compile header[
   224 flags!                           \ flags: jumpable, anon, smudged
   255 [ pad 15 + ] literal c!          \ blank count byte
   ]header
   1 c_colondef c!  ]
;
: ;  \ --
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
; immediate

