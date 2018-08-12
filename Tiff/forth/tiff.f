\ Forth kernel for Tiff

: !                 !+ drop ; macro     \ x addr --
: c!               c!+ drop ; macro     \ c addr --
: w!               w!+ drop ; macro     \ w addr --
: negate          invert 1+ ; macro     \ n1 n2 -- n3
: cells               2* 2* ; macro
: rot       >r swap r> swap ; macro
: -rot      swap >r swap r> ; macro

: lshift  \ x count             \ left shift
   63 and                       \ limit the count
   negate +if: drop exit |      \ 0
   1+ swap
   | 2* -rept swap drop ;
;

: rshift  \ x count             \ right shift
   63 and                       \ limit the count
   negate +if: drop exit |      \ 0
   1+ swap
   | u2/ -rept swap drop ;
;



\ Define a compiler
\ char params:  c_colondef c_litpend c_slot c_called

: ClearIR   \ --                \ Initialize the IR. slot=26, litpend=0
   0 iracc !  26 c_slot w!  0 c_called c!
;
: AppendIR  \ imm opcode        \ insert opcode at slot, adding imm
   c_slot c@ lshift  iracc @    \ shift it and get IR
   +  +              iracc !    \ merge into instruction
;

\ First, let's wean the pre-built headers off the C functions.
\ Note that replace-xt is only available in the host system due to
\ the constraints of run-time flash programming.

\ EQU pushes or compiles W parameter
: equ_e   head @ -16 + @ ;      \ -- n
' equ_e  -1 replace-xt          \ execution part of EQU


\ Some kernel words

: decimal  10 base ! ;          \ --
: hex      16 base ! ;          \ --
: foo hex decimal ;


