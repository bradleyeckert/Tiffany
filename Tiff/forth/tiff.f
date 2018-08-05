\ Forth kernel for Tiff
\ Put this in the working directory with tiff.exe

: @  a! @a ; macro              \ addr -- x
: !  a! !a ; macro              \ x addr --

\ First, let's wean the pre-built headers off the C functions.
\ Note that replace-xt is only available in the host system due to
\ the constraints of run-time flash programming.

\ EQU pushes or compiles W parameter
: equ_e   head @ -12 + @ ;      \ -- n
' equ_e  -1 replace-xt          \ execution part of EQU



\ Some kernel words

: -rot rot rot ; macro

: decimal  10 base ! ;          \ --
: hex      16 base ! ;          \ --

