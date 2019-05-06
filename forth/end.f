\ Set up initialization

cp @  idata,
equ IdataTable        					\ compile idata table to ROM


\ hex
: initialize                            \ ? -- | R: ? a --
    [ status ] literal                  \ start of RAM
    dup negate                          \ status to FFFFFFFF
    erase                               \ is 0 by default
	IdataTable
	begin
		@+ 2dup 1+ cells + -rot         ( list' asrc length )
	dup while
		>r @+ r>  umove                 ( list' )
	repeat                              \ stack will be reset soon
	r> hld !                            \ save return address
	[ 0 up ]      literal up!           \ terminal task
	[ sp0 @ ]     literal sp!           \ empty data stack
	[ rp0 @ 4 - ] literal rp!           \ empty return stack
	hld @ >r
	[ hex ]
    10000  cp !                         \ a place to put new code
    18000  hp !                         \ a place to put new headers
; call-only
decimal

:noname                                 \ launch user application
	initialize                          \ return stack is now empty, you can't return to caller
	main
	bye
; is coldboot

:noname                                 \ enter bootloader/terminal without app
	initialize
	main                                \ not implemented yet
	bye
; is safemode
