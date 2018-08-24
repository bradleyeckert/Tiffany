\ Wean the pre-built headers off the C functions in tiffFUNC.
\ Note that replace-xt is only available in the host system due to
\ the constraints of run-time flash programming.

\ EQU pushes or compiles W parameter
: equ_ee   head @ 16 - @ ;              \ -- n
' equ_ee  -1 replace-xt                 \ execution part of EQU
: equ_ec   equ_ee literal, ;
' equ_ec  -2 replace-xt                 \ compilation part of EQU

\ Executing an implicit opcode is not possible without the host VM.
\ Instead, the xte is replaced with a noname definition.
\ So, there's no "-3 replace-xt"

\ Run-time actions of primitives
:noname dup    ; xte-is dup
:noname +      ; xte-is +
:noname -      ; xte-is -
:noname c+     ; xte-is c+
:noname r@     ; xte-is r@
:noname and    ; xte-is and
:noname over   ; xte-is over
:noname r>     ; xte-is r>
:noname xor    ; xte-is xor
:noname 2*     ; xte-is 2*
:noname 2*c    ; xte-is 2*c
:noname c!+    ; xte-is c!+
:noname c@+    ; xte-is c@+
:noname count  ; xte-is count
:noname c@     ; xte-is c@
:noname w!+    ; xte-is w!+
:noname w@+    ; xte-is w@+
:noname w@     ; xte-is w@
:noname !+     ; xte-is !+
:noname @+     ; xte-is @+
:noname @      ; xte-is @
:noname u2/    ; xte-is u2/
:noname rept   ; xte-is rept
:noname -rept  ; xte-is -rept
:noname 2/     ; xte-is 2/
:noname sp     ; xte-is sp
:noname invert ; xte-is invert
:noname rp!    ; xte-is rp!
:noname rp     ; xte-is rp
:noname port   ; xte-is port
:noname sp!    ; xte-is sp!
:noname up     ; xte-is up
:noname up!    ; xte-is up!
:noname drop   ; xte-is drop
:noname 1+     ; xte-is 1+
:noname char+  ; xte-is char+
:noname 2+     ; xte-is 2+
:noname cell+  ; xte-is cell+
:noname >r     ; xte-is >r
:noname swap   ; xte-is swap
:noname 0=     ; xte-is 0=
:noname 0<     ; xte-is 0<
:noname        ; xte-is nop

:noname equ_ee Implicit ; -4 replace-xt \ compilation part of implicit opcode
:noname -14 throw ;       -5 replace-xt \ "Interpreting a compile-only word"

:noname                                 \ immediate opcode
   c_litpend dup c@ if
      0 swap c!
      equ_ee  nextlit @  Explicit
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

:noname  equ_ee up ; -10 replace-xt     \ execute user variable
:noname                                 \ compile user variable
   equ_ee literal,  op_up Implicit
; -11 replace-xt

: get-xte  \ xte --                     \ from HEAD
   head @ invert cell+ invert link>
;

\ Lower 4 bits of CP must be 4.
cp @ 4 - 12 and  cp +!                  \ align to 4 bytes past 16-byte boundary
defer do-immediate                      \ 1st cell -> immediate
defer get-macro                         \ 2nd cell -> macro
: get-compile  get-xte compile, ;       \ 3rd cell -> compile

:noname  \ get-macro
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

' get-compile -17 replace-xt
' get-macro   -21 replace-xt

:noname  \ do-immediate
   get-xte execute
; is do-immediate

