\ Forth kernel for Tiff

\ CP, HP, and DP point to the first free bytes in code, header, and data space.
\ System variables are already defined in Tiff because it uses them internally.
\ See `config.h`.

.( Tiff.f: )

4 equ DumpColumns                       \ Output chars ~ Columns * 13 + 5

include core.f
: pause ; : /pause ; \ include tasker.f \ no multitasker
include timing.f
include numio.f                         \ numeric I/O
include flash.f                         \ SPI flash programming
include compile.f                       \ compile opcodes, macros, calls, etc.
include interpret.f                     \ parse, interpret, convert to number
include tools.f                         \ dump, .s

: main
   ." Hello World"
   cr 10 0 do i . loop
;

include weanexec.f                      \ replace C execution fns in existing headers
include end.f                           \ finish the app

\ ------------------------------------------------------------------------------
\ TEST STUFF: The demo doesn't use anything after this...

\ Here's where we reposition CP to run code out of SPI flash.
\ Put the less time-critical parts of your application here.
\ Omitting the SPI flash should break the interpreter but nothing else.
\ This is also necessary because the Forth can't modify internal ROM at run time.

cp ?                                    \ display bytes of internal ROM used
hp0 16384 + cp !                        \ leave 16K for headers
cp @ equ codespace                      \ put code here in aux flash

include weancomp.f                      \ replace C compilation fns in existing headers
include define.f                        \ defining words
include forth.f                         \ high level Forth

\ If you XWORDS at this point, you'll see that all XTEs and XTCs are now Forth words.


\ Some test words

: foo hex decimal ;

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

 : zz ." hello" ;

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


.( bytes in internal ROM, ) CP @ hp0 16384 + - .
.( bytes of code in flash, ) HP @ hp0 - .
.( bytes of header.) cr

: ten  20 0 do i .  i 10 = if leave then loop ;

0 [if] .( should not print )
[else] .( Hi there!) cr
[then]

: ENVIRONMENT? ( c-addr u -- false ) 2drop 0 ;

\ include test/ttester.fs
\ include test/coretest.fs

\ make ../templates/app.c ../demo/vm.c       \ C version
make ../src/vm.c ../demo/vm.c       \ C version, vm.c is usable as a template
\ make ../templates/app.A51 ../8051/vm.A51   \ 8051 version

\ make ../templates/app.c ../testbench/vm.c  \ C version for testbench
\ 100 make ../templates/test_main.c ../testbench/test.c
theme=color
