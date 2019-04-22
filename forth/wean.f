\ Wean the pre-built headers off the C functions in tiffFUNC.
\ Note that replace-xt is only available in the host system due to
\ the constraints of run-time flash programming.

: get-xte  \ xte --                     \ from HEAD
   head @ invert cell+ invert link>
;

\ COMPILE is converted to other types by clearing select bits.
\ 11xx Compile     \ compile call to xte
\ 10xx CompileMacro
\ 01xx Execute

cp @ negate 4 +  12 and  cp +!          \ align to 16-byte boundary
defer do-immediate                      \ 1st cell -> immediate
defer get-macro                         \ 2nd cell -> macro (address bit2=0)
defer get-compile                       \ 3rd cell -> compile (address bit2=1)
defer do-does-e                         \ 0th cell -> does> execute
defer do-does-c                         \ 1st cell -> does> compile
defer equ_ee                            \ 2nd cell -> equ execute
defer equ_ec                            \ 3rd cell -> equ compile

\ EQU pushes or compiles W parameter
:noname head @ 16 - @ ; is equ_ee       \ -- n
:noname get-xte execute ; is do-immediate \ replaces xtc

:noname  \ does>                        \ replaces equ_ee
   head @ 20 - 2@ execute               \ 'data
; is do-does-e

:noname -14 throw ;   -5 replace-xt     \ "Interpreting a compile-only word"
:noname  equ_ee up ; -10 replace-xt     \ execute user variable
' equ_ee  -1 replace-xt                 \ execution part of EQU

\ Executing an implicit opcode is not possible without the host VM.
\ Instead, the xte is replaced with a noname definition.
\ So, there's no "-3 replace-xt"

\ Run-time actions of primitives are headerless
:noname dup    ; xte-is dup             \ 6.1.1290
:noname +      ; xte-is +               \ 6.1.0120
:noname -      ; xte-is -               \ 6.1.0160
:noname c+     ; xte-is c+
:noname r@     ; xte-is r@              \ 6.1.2070
:noname and    ; xte-is and             \ 6.1.0720
:noname over   ; xte-is over            \ 6.1.1990
:noname r>     ; xte-is r>              \ 6.1.2060
:noname xor    ; xte-is xor             \ 6.1.2490
:noname 2*     ; xte-is 2*              \ 6.1.0320
:noname 2*c    ; xte-is 2*c
:noname c!+    ; xte-is c!+
:noname c@+    ; xte-is c@+
:noname count  ; xte-is count
:noname c@     ; xte-is c@              \ 6.1.0870
:noname w!+    ; xte-is w!+
:noname w@+    ; xte-is w@+
:noname w@     ; xte-is w@
:noname !+     ; xte-is !+
:noname @+     ; xte-is @+
:noname @      ; xte-is @               \ 6.1.0650
:noname u2/    ; xte-is u2/
:noname rept   ; xte-is rept
:noname -rept  ; xte-is -rept
:noname 2/     ; xte-is 2/              \ 6.1.0330
:noname sp     ; xte-is sp
:noname invert ; xte-is invert          \ 6.1.1720
:noname rp!    ; xte-is rp!
:noname rp     ; xte-is rp
:noname port   ; xte-is port
:noname sp!    ; xte-is sp!
:noname up     ; xte-is up
:noname up!    ; xte-is up!
:noname drop   ; xte-is drop            \ 6.1.1260
:noname 1+     ; xte-is 1+              \ 6.1.0290
:noname char+  ; xte-is char+           \ 6.1.0897
:noname 2+     ; xte-is 2+
:noname cell+  ; xte-is cell+           \ 6.1.0880
:noname >r     ; xte-is >r              \ 6.1.0580
:noname swap   ; xte-is swap            \ 6.1.2260
:noname 0=     ; xte-is 0=              \ 6.1.0270
:noname 0<     ; xte-is 0<              \ 6.1.0250
:noname        ; xte-is nop

:noname  \ get-macro                    \ replaces xtc
   get-xte  @+  26                      \ addr IR slot
   begin
      dup 0< if
         drop 3 and >r  @+ 26 r>
      else
         2dup rshift 63 and
         >r 6 - r>
      then                              \ addr IR slot opcode
      dup op_exit = if
         2drop 2drop exit
      then  Implicit
   again
; is get-macro

:noname  get-xte compile,
; is get-compile

:noname  equ_ee literal,
; is equ_ec

:noname  head @ 20 - @+ literal, @ compile,
; is do-does-c

\ Compilation actions

' equ_ec  -2 replace-xt                 \ compilation part of EQU

:noname                                 \ compilation part of implicit opcode
   equ_ee Implicit
; -4 replace-xt

:noname                                 \ immediate opcode
   c_litpend dup c@ if
      0 swap c!
      equ_ee  nextlit @  swap Explicit
      exit
   then  -59 throw
; -6 replace-xt

:noname                                 \ place "skip" instruction
   FlushLit
   c_slot 8 < if NewGroup then
   equ_ee Implicit
; -7 replace-xt

:noname                                 \ place "skip" instruction in a new group
   NewGroup  equ_ee Implicit
; -8 replace-xt

:noname                                 \ skip to new group
   FlushLit NewGroup
; -9 replace-xt

:noname                                 \ compile user variable
   equ_ee literal,  op_up Implicit
; -11 replace-xt
