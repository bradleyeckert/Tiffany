\ Forth kernel for Tiff

\ CP, HP, and DP point to the first free bytes in code, header, and data space.
\ System variables are already defined in Tiff because it uses them internally.
\ See `config.h`.

.( Tiff.f: )

4 equ DumpColumns                       \ Output chars ~ Columns * 13 + 5

defer coldboot
defer safemode
defer errorISR

include ../forth/core.f
: pause ; : /pause ; \ include ../forth/tasker.f \ no multitasker
include ../forth/timing.f
include ../forth/numio.f                \ numeric I/O

0 [if] \ not using QUIT yet

    : throw  \ n --  						\ for testing, remove later
    ?dup if  port drop  					\ save n in dbg register, like error interrupt
        8 >r								\ fake an error interrupt
    then
    ; call-only

    \ The error ISR has the last known good PC on the return stack and the ior in port.
    \ Usually, you would just throw an error.
    \ Since the Tiff interpreter is being used, use a test THROW.

    :noname
    cr ." Error " dup port . ." at PC=" r> .
    ." Line# " w_linenum w@ .
    cr
    -1 @  								\ produce an error to quit
    ; is errorISR
    [else]
    :noname
    dup port throw
    ; is errorISR

[then]

include ../forth/flash.f                \ SPI flash programming
include ../forth/compile.f              \ compile opcodes, macros, calls, etc.
include ../forth/tools.f                \ dump, .s
include ../forth/interpret.f            \ parse, interpret, convert to number
include ../forth/see.f
include ../forth/wean.f                 \ replace most C fns in existing headers
include ../forth/define.f               \ defining words
include ../forth/structure.f			\ control structures
include ../forth/string.f
include ../forth/order.f
include ../forth/evaluate.f		        \ evaluate
include ../forth/coreext.f		        \ oddball CORE EXT words
include ../forth/toolsext.f		        \ oddball TOOLS EXT words
include ../forth/double.f		        \ double math
include ../forth/forth.f                \ high level Forth

: main
   ." May the Forth be with you!" cr
   quit
;

include ../forth/end.f                  \ finish the app

HP @ . .( bytes in ROM, of which ) CP @ . .( is code and ) HP @ hp0 - . .( is head. )
.( RAM = ) DP @ ROMsize - . .( of ) RAMsize .  cr

make ../src/vm.c ../demo/vm.c           \ C version, vm.c is usable as a template
make ../templates/app.A51 ../8051/vm.A51   \ 8051 version

\ make ../templates/app.c ../testbench/vm.c  \ C version for testbench
\ 100 make ../templates/test_main.c ../testbench/test.c

\ Include the rest of Forth for testing, etc.

\ 65536 cp !  \ uncomment to compile code to flash region (not internal ROM)

\ ------------------------------------------------------------------------------
\ TEST STUFF: The demo doesn't use anything after this...

\ Here's where we reposition CP to run code out of SPI flash.
\ Put the less time-critical parts of your application here.
\ Omitting the SPI flash should break the interpreter but nothing else.
\ This is also necessary because the Forth can't modify internal ROM at run time.


\ If you XWORDS at this point, you'll see that all XTEs and XTCs are now Forth words.


\ Some test words

\ If using ConEmu, set it up to handle UTF8 output. UTF8 input can't paste onto
\ console input, so you can't directly test 平方.
\ However, Linux GNOME has proper UTF8 handling. But not good cooked mode.
: chin
   ." 你好，世界"                         \ Chinese Hello World in UTF8 format
;
: 平方  dup * ;                          \ use a UTF8 word name
: dist  \ x y -- dist^2
   平方 swap 平方 +
;

cp @ equ s1
   ," 123456"

: str1 s1 count ;

cp @ equ s2
    ," +你~好~，~世~界+"
: str2 s2 count ;

: spell  \ n --
   case
   0 of ." zero"  endof
   1 of ." one"   endof
   2 of ." two"   endof
   3 of ." three" endof
   dup .
   endcase
;

: mynum  ( n "name" -- )
   rom  create ,
   ram  does> @ 2*
;
 111 mynum z1
 222 mynum z2

0 [if]
: ENVIRONMENT? ( c-addr u -- false ) 2drop 0 ;

cr .( Test suite: ) cr
include ../forth/test/ttester.fs
include ../forth/test/coretest.fs
include ../forth/test/dbltest.fs
bye
[then]

theme=color
