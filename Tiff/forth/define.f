\ Defining words

: alignh    hp @ aligned hp ! ;         \ --
: alignc    cp @ aligned cp ! ;         \ --
: align     h @ aligned h ! ;           \ --

\ Create a new header. The source is the keyboard, file, or blocks.
\ `include` (in tiff.c) saves the filename and manages c_fileid and w_linenum.

\ |Cell| \[31:24\]                        | \[23:0\]                           |
\ |----|:---------------------------------| ----------------------------------:|
\ | -4 | optional W                                                            |
\ | -3 | Source File ID                   | List of words that reference this  |
\ | -2 | Source Line, Low byte            | xtc, Execution token for compile   |
\ | -1 | Source Line, High byte           | xte, Execution token for execute   |
\ | 0  | # of instructions in definition  | Link                               |
\   ^------------CURRENT @
\ | 1+ | counted name string ...

: header[  \ <name> xte xtc --          \ populate PAD with header data
   pad |pad| -1 fill                    \ default fields (starting with FID) are blank
   [ pad 4 + ] literal !+ !+
   current @ @ swap !+                  \ add xtc, xte, link
   >r parse-word dup r> c!+             \ store length
   swap cmove                           \ and string
   c_fileid c@ [ pad 3 + ] literal c!   \ file ID
   w_linenum c@+  >r c@ r>              \ hi lo
   [ pad 7 + ] literal c!
   [ pad 11 + ] literal c!              \ insert line number
;
: ]header  \ --                         \ write PAD to flash
   pad  dup
   16 + c@ 31 and  1+ aligned  16 + >r
   hp @  r@  ROMmove                    \ write to flash
   hp @ 12 + current @ !                \ add to current definitions
   r> hp +!
;
: flags!  \ c --                        \ change flags (from 0)
   [ pad 16 + ] literal c+!
;

\ | Bit | Usage       | Description                                  |
\ |:---:|:------------|:---------------------------------------------|
\ | 7   | Call Only   | '0' when tail call optimization is inhibited |
\ | 6   | Anonymous   | '0' when to be excluded from where list      |
\ | 5   | Smudged     | '0' when header is findable (used by : etc.) |
\ | 4:0 | Name Length | 0 to 31                                      |

: equ  \ n -- | -- n                    \ new equ
   ['] equ_ee  ['] equ_ec
   header[  ,h
   ]header
;

: last  current @ @ + ;       \ n -- a  \ index into last defined header
: clrlast    last dup >r @ and r> ROM! ;   \ bits offset --
\ : clrlast    last dup SPI! ;   \ bits offset --
: clr-xtcbits  invert -8 clrlast ;      \ n --   flip bits in the current xtc
: clr-xtebits  invert -4 clrlast ;      \ n --   flip bits in the current xte
: clr-flagbits  invert 4 clrlast ;      \ n --   flip bits in the flags
: macro      4 clr-xtcbits ;  \ --      \ flip xtc from compile to macro
: immediate  8 clr-xtcbits ;  \ --      \ flip xtc from compile to immediate
: call-only  128 clr-flagbits ;  \ --   \ clear the jumpable bit

: ]  1 state ! ;              \ --      \ resume compilation
: [  0 state ! ; immediate    \ --      \ interpret

: ;  \ --
   ,exit  NewGroup
   4 last c@  32 and if                 \ smudge bit is set?
      32 clr-flagbits                   \ clear it
   then
   c_colondef c@ if                     \ wid
      0 c_colondef c!
      cp @  -4 last link> - 2/ 2/       \ length
      255 min
      1+ 24 lshift 1-  0 clrlast
   then
   0 state !
; immediate

: +:  \ xtc <name> --                   \ new : with custom xtc
   >r alignc  cp @ r> header[
   255 [ pad 15 + ] literal c!          \ blank count byte = 255
   224 flags!                           \ flags: jumpable, anon, smudged
   ]header
   1 c_colondef c!  ]
;
: :  \ <name> --                        \ new :
   ['] get-compile  +:
;
: :noname  \ -- xt
   alignc  NewGroup  cp @  ]
;

\ Now that Tiff's : and ; are hidden, only Forth XTs will be used.
\ Replace Tiff XTs with equivalent Forth XTs.

' do-immediate  -25 replace-xt
' get-macro     -21 replace-xt
' get-compile   -17 replace-xt

\ If you XWORDS at this point, you will see that all words use Forth code.
\ Their xte and xtc fields are addresses in ROM.

: here      h @ ;                      \ -- a
: allot     h +! ;                     \ n --
: erase     0 fill ;                   \ --
: /allot    h @ swap dup allot erase ; \ n --
: buffer:   h @ equ  allot ;           \ length "name" --
: constant  equ ;
: variable  align h @ equ  4 /allot ;
