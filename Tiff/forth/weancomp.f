\ Wean the pre-built headers off the C functions in tiffFUNC.
\ Note that replace-xt is only available in the host system due to
\ the constraints of run-time flash programming.

' equ_ec  -2 replace-xt                 \ compilation part of EQU

:noname                                 \ compilation part of implicit opcode
   equ_ee Implicit
; -4 replace-xt

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

:noname                                 \ compile user variable
   equ_ee literal,  op_up Implicit
; -11 replace-xt

