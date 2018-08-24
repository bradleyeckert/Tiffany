\ Forth kernel for Tiff

include core.f
include tasker.f
include numio.f
include flash.f
\ Here's where you'd reposition CP to run code out of SPI flash.
\ Put the time critical parts of your application here.
\ Omitting the SPI flash should break the interpreter but nothing else.

cp ?                                    \ display bytes of internal ROM used
hp0 16384 + cp !                        \ leave 16kB for headers

include compile.f
include wean.f
include interpret.f
include tools.f

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
\ Put this as the last definition.
: _:  \ <name> --
   cp @ ['] compile, header[
   224 flags!       ]header             \ flags: jumpable, anon, smudged
   1 c_colondef c!
;


\ Note: interpreter will need the compiler.


\ Some test words

: foo hex decimal ;


cp @ equ s1 ," 123456"  : str1 s1 count ;
cp @ equ s2 ," the quick brown fox"  : str2 s2 count ;


: oops
   cr ." Error#" .
   w_>in w@ >in !               \ back up to error
   parse-word type space
;

: try  ['] interpret catch ?dup if oops then ;


.( bytes in internal ROM, ) CP @ hp0 16384 + - .
.( bytes of code in flash, ) HP @ hp0 - .
.( bytes of header.) cr

