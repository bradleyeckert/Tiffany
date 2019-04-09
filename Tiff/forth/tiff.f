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

: coldboot
   initialize                           \ return stack is now empty, you can't return to caller
   ." Hello World"
   bye
;

cp @  0 cp !  :noname coldboot ; drop   \ resolve the forward jump to coldboot
cp !

\ The demo doesn't use anything after this...

\ Here's where we reposition CP to run code out of SPI flash.
\ Put the less time-critical parts of your application here.
\ Omitting the SPI flash should break the interpreter but nothing else.
\ This is also necessary because the Forth can't modify internal ROM at run time.

cp ?                                    \ display bytes of internal ROM used
hp0 0x8000 + cp !                       \ leave 32K for headers

include compile.f                       \ compile opcodes, macros, calls, etc.
include wean.f                          \ replace C functions in existing headers
include interpret.f                     \ parse, interpret, convert to number
include tools.f                         \ dump, .s
include define.f                        \ defining words
include forth.f                         \ high level Forth

\ If you XWORDS at this point, you'll see that all XTEs and XTCs are now Forth words.


\ Some test words

: foo hex decimal ;

\ If using ConEmu, set it up to handle UTF8 output. UTF8 input can't paste onto
\ console input, so you can't directly test 平方.
\ However, Linux GNOME terminal has proper UTF8 handling. Command buffering kind of sucks.
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


.( bytes in internal ROM, ) CP @ hp0 0x8000 + - .
.( bytes of code in flash, ) HP @ hp0 - .
.( bytes of header.) cr

: ten  20 0 do i .  i 10 = if leave then  loop ;

0 [if] .( should not print )
[else] .( Hi there!) cr
[then]

\ : ENVIRONMENT? ( c-addr u -- false ) 2drop 0 ;
\ ram \ , is to RAM

\ include test/ttester.fs
\ include test/coretest.fs

\ make ../templates/app.c ../demo/vm.c       \ C version
\ make ../templates/app.A51 ../8051/vm.A51   \ 8051 version

\ make ../templates/app.c ../testbench/vm.c  \ C version for testbench
\ 100 make ../templates/test_main.c ../testbench/test.c
theme=color
