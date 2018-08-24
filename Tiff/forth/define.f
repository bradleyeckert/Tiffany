\ Defining words

\ Create a new header. The source is either the keyboard or blocks.

\ | Cell | \[31:24\]                        | \[23:0\]                           |
\ | ---- |:---------------------------------| ----------------------------------:|
\ | -3   | Source File ID                   | List of words that reference this  |
\ | -2   | Source Line, Low byte            | xtc, Execution token for compile   |
\ | -1   | Source Line, High byte           | xte, Execution token for execute   |
\ | 0    | # of instructions in definition  | Link                               |

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
   16 + c@ 1+ aligned  16 + >r
   hp @  r@  SPImove                    \ write to flash
   hp @ 12 + current @ !                \ add to current definitions
   r> hp +!
;
: flags!  \ c --                        \ change flags (from 0)
   [ pad 16 + ] literal c!+
;

: equ  \ n --                           \ new equ
   ['] equ_ee  ['] equ_ec
   header[ ,h ]header
;

: flipxtc  \ n --                       \ flip bits in the current xtc
   invert current @ @ 8 - SPI!
;
: macro      4 flipxtc ;  \ --          \ flip xtc from compile to macro
: immediate  8 flipxtc ;  \ --          \ flip xtc from compile to immediate

: call-only  \ --
   current @ @                          \ current head
   128 invert  swap 4 + SPI!            \ clear the jumpable bit
;

: ]  1 state ! ;                        \ resume compilation mode
: [  0 state ! ; immediate

\ not quite the same as : yet
: _:  \ <name> --                        \ new :
   cp @ ['] compile, header[
   224 flags!       ]header             \ flags: jumpable, anon, smudged
   1 c_colondef c!  ]
;

: ,;  \ --
   ,exit  NewGroup
   current @ @                          \ link
   dup 4 + @ 32 and if                  \ smudge bit is set?
      dup 4 + 32 invert SPI!            \ clear it
   then
   c_colondef c@ if                     \ wid
      0 c_colondef c!
      cp @  over 4 - link>  2/ 2/       \ length
      255 min
      pad c!  pad swap 1 SPImove
   then
   0 state !
; immediate


