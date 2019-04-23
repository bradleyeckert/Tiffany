\ Test vector generation

\ CP, HP, and DP point to the first free bytes in code, header, and data space.
\ System variables are already defined in Tiff because it uses them internally.
\ See `config.h`.

.( Testbench generation ) cr

defer coldboot                          theme=color
10000 ,
defer exception

\ Include core words that test a lot of primitives

include ../../forth/core.f

: test0 ;                               \ call and return

: test1    ( -- )                       \ basic opcodes
    nop         |                       \ nop, no:
    1234                                \ 4D2 <sp
    dup         |                       \ 4D2 4D2 <sp
    drop        |                       \ 4D2 <sp
    dup         |                       \ 4D2 4D2 <sp
    cell+       |                       \ 4D2 4D6 <sp
    +           |                       \ 9A8 <sp
    0<          |                       \ 0 <sp
    2+          |                       \ 2 <sp
    invert      |                       \ FFFFFFFD <sp
    dup >r      |                       \ FFFFFFFD <sp
    2/          |                       \ FFFFFFFE <sp
    1+          |                       \ FFFFFFFF <sp
    r@          |                       \ FFFFFFFF FFFFFFFD <sp
    -           |                       \ 2 <sp
    r>          |                       \ 2 FFFFFFFD <sp
    xor         |                       \ FFFFFFFF <sp
    0=          |                       \ 0 <sp
    0= 0<       |                       \ FFFFFFFF <sp
    12345678                            \ FFFFFFFF BC614E <sp
    hld                                 \ FFFFFFFF BC614E 8288 <sp
    dup >r      |                       \ FFFFFFFF BC614E 8288 <sp
    !+          |                       \ FFFFFFFF 828C <sp
    drop        |                       \ FFFFFFFF <sp
    r@ c@+      |                       \ FFFFFFFF 8289 4C <sp
    swap        |                       \ FFFFFFFF 4C 8289 <sp
    c@          |                       \ FFFFFFFF 4C 83 <sp
    drop drop   |                       \ FFFFFFFF <sp
    r@ w@+      |                       \ FFFFFFFF 828A 834C <sp
    swap        |                       \ FFFFFFFF 834C 828A <sp
    w@          |                       \ FFFFFFFF 834C 0 <sp
    drop drop   |                       \ FFFFFFFF <sp
    r@ @+       |                       \ FFFFFFFF 828C 834C <sp
    swap        |                       \ FFFFFFFF 834C 828C <sp
    @           |                       \ FFFFFFFF 834C 20786568 <sp
    and         |                       \ FFFFFFFF 148 <sp
    1000 r@ w!+ drop |                  \ FFFFFFFF 148 <sp
    r@ w@       |                       \ FFFFFFFF 148 8351 <sp
    drop 99     |                       \ FFFFFFFF 148 63 <sp
    r@ c!+ drop |                       \ FFFFFFFF 148 <sp
    r> c@       |                       \ FFFFFFFF 148 51 <sp
    c+          |                       \ FFFFFFFF 199 <sp
    over        |                       \ FFFFFFFF 199 FFFFFFFF <sp
    u2/         |                       \ FFFFFFFF 199 7FFFFFFF <sp
    2/          |                       \ FFFFFFFF 199 3FFFFFFF <sp
    c+          |                       \ FFFFFFFF 40000198 <sp
    swap        |                       \ 40000198 FFFFFFFF <sp
    2/ u2/      |                       \ 40000198 7FFFFFFF <sp
    2*c         |                       \ 40000198 FFFFFFFE <sp
    2*          |                       \ 40000198 FFFFFFFC <sp
    invert +    |                       \ 4000019B <sp
    85 litx                             \ 9B000055 <sp
    drop
;
: test2  \ c --                         \ emit
    2 user                              \ user
    drop                                \ drop
;

: test3                                 \ cause an exception
   [ pad 1+ ] literal
   @        |
   drop     |
;
:noname  \ --                           \ ignore the error
; is exception

: test4
   0 pad 4 umove                        \ copy ROM to RAM, 4 cells
   [ pad 16 + ] literal  dup 16 +  16 cmove  \ copy RAM to RAM
   -1 -1 um* drop                       \ test multiply
   -1 u2/ dup -3 um/mod  2drop
;

: test5
   1000000 355 113 */ throw
;

:noname                                 \ launch user application
    test0
    test1
    [char] [ test2
    test3
    [char] - test2
    test4
    ['] test5 catch  drop
    [char] - test2
    begin again
; is coldboot

make ../../src/vm.c vm.c                \ Using the know-good one
200 make ../../templates/test_main.c main.c
\ stack is changed after testbench due to SP register moved, but harmless.
bye
