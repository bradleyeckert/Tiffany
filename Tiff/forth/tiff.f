\ Forth kernel for Tiff

include core.f
include tasker.f
include numio.f
include flash.f
\ Here's where you'd reposition CP to run code out of SPI flash
\ in order to keep the internal ROM small(er). Your app would go there too.
\ Unplugging the SPI flash should break the interpreter but nothing else.

\ At this point, 1.4K bytes are in the internal ROM image.
include tools.f
include compile.f    \ work in progress
include interpret.f

\ Create a new header. The source is either the keyboard or blocks.

\ | Cell | \[31:24\]                        | \[23:0\]                           |
\ | ---- |:---------------------------------| ----------------------------------:|
\ | -3   | Source File ID                   | List of words that reference this  |
\ | -2   | Source Line, Low byte            | xtc, Execution token for compile   |
\ | -1   | Source Line, High byte           | xte, Execution token for execute   |
\ | 0    | # of instructions in definition  | Link                               |

: Header[  \ <name> xte xtc --          \ populate PAD with header data
   pad 28 -1 fill                       \ default fields are blank
   [ pad 4 + ] literal !+ !+
   current @ swap !+                    \ add xtc, xte, link
   >r parse-word dup r> c!+             \ store length
   swap cmove
   source-id c@ [ pad 3 + ] literal c!  \ file ID
   w_linenum c@+ >r c@ r>               \ hi lo
   [ pad 7 + ] literal c!
   [ pad 11 + ] literal c!              \ insert line number
;
: ]header  \ --                         \ write PAD to flash
   pad  dup
   16 + c@ 1+ aligned  16 + >r
   hp @  r@  SPImove                    \ write to flash
   r> hp +!
;
: _:  \ <name> --
   cp @ ['] compile, Header[
\ {
\     FollowingToken(name, 32);
\     NewGroup();
\     CommaHeader(name, FetchCell(CP), ~16, -1, 0xE0);
\     // xtc must be multiple of 8 ----^
\     StoreByte(1, COLONDEF);
\     StoreCell(1, STATE);
\ }
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

.( `tiff.f` compiled ) CP ? .( bytes of code, ) HP @ 32768 - . .( bytes of header.)
cr
