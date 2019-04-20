\ Forth kernel for Tiff

\ CP, HP, and DP point to the first free bytes in code, header, and data space.
\ System variables are already defined in Tiff because it uses them internally.
\ See `config.h`.

4 equ DumpColumns                       \ Output chars ~ Columns * 13 + 5

defer coldboot
defer safemode
defer errorISR
-1 , -1 ,                               \ space for CRC and LENGTH

include ../forth/core.f
: pause ; : /pause ; \ include ../forth/tasker.f \ no multitasker
include ../forth/timing.f
include ../forth/numio.f                \ numeric I/O
include ../forth/flash.f                \ SPI flash programming

.( Internal ROM minimum bytes = ) cp ? cr
.( Tiff.f: )

0 [if] \ not using QUIT yet

	: throw  \ n --  				    \ for testing, remove later
	?dup if  port drop  			    \ save n in dbg register, like error interrupt
		16 >r						    \ fake an error interrupt
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

include ../forth/compile.f              \ compile opcodes, macros, calls, etc.

include ../forth/tools.f                \ dump, .s
include ../forth/interpret.f            \ parse, interpret, convert to number
include ../forth/wean.f                 \ replace most C fns in existing headers
include ../forth/define.f               \ defining words
include ../forth/structure.f			\ control structures
include ../forth/evaluate.f		        \ evaluate
include ../forth/quit.f                 \ the quit loop

include ../forth/order.f
include ../forth/see.f
include ../forth/coreext.f		        \ oddball CORE EXT words
include ../forth/toolsext.f		        \ oddball TOOLS EXT words
include ../forth/string.f
include ../forth/double.f		        \ double math

: main
   ." May the Forth be with you!" cr
   quit
;

\ http://home.iae.nl/users/mhx/monsterbench.html
\ A blast from the past, about as fast as the PCs of the time.
\ The VM on a Core-i7 simulates 100-200 MHz PCs of the 1997 era.
\ 2019 computers have single-thread performance 20x of those.

: FIB ( x -- y )
	DUP 2 > IF DUP  1- RECURSE
		   SWAP 2 - RECURSE +  EXIT
	     THEN
	DROP 1 ;

: bench  \ --                           \ benchmark
   counter >r
   ( 40 ) 30  fib drop \ note run-time limit in Tiff. Use cold.
   ." 30 fib executes in "
   counter r> - 0 <# # [char] . hold #s #> type ."  msec "
;


include ../forth/end.f                  \ finish the app

cp @  dup    16 !                       \ resolve internal ROM length
0 swap crc32 12 !                       \ store CRC, which includes length

HP @ . .( bytes in ROM, of which ) CP @ . .( is code and ) HP @ hp0 - . .( is head. )
.( RAM = ) DP @ ROMsize - . .( of ) RAMsize .  cr

\ Make C demo files
make ../src/vm.c ../demo/vm.c           \ vm.c is usable as a template
make ../src/flash.c ../demo/flash.c     \ flash.c is usable as a template

\ make ../templates/app.A51 ../8051/vm.A51   \ 8051 version

\ make ../templates/app.c ../testbench/vm.c  \ C version for testbench
\ 100 make ../templates/test_main.c ../testbench/test.c
\ ------------------------------------------------------------------------------

\ TEST STUFF: The demo doesn't use anything after this...

romsize ramsize + cp !  \ uncomment to compile code to flash region (not internal ROM)


cp @ ," DataCodeHead" 1+
: .unused  \ --
   literal                              \ sure is nice to pull in external literal
   3 0 do                               \ even it's not ANS
      i c_scope c!
      dup i cells + 4 type
      ." : " unused . cr                \ unused is a weird, depends on memory usage
   loop  drop  ram
;

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
