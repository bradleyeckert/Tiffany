\ Serial Port example

defer coldboot
defer safemode
defer errorISR

1 equ options                           \ use HW mul/div
include ../../forth/core.f
: pause ;  \ include ../../forth/tasker.f \ no multitasker
include ../../forth/timing.f
include ../../forth/numio.f             \ numeric I/O

	: throw  \ n --  				    \ for testing, remove later
	?dup if  port drop  			    \ save n in dbg register, like error interrupt
		8 >r						    \ fake an error interrupt
	then
	; call-only

	\ The error ISR has the last known good PC on the return stack and the ior in port.
	\ Usually, you would just throw an error.
	\ Since the Mforth interpreter is being used, use a test THROW.

	:noname
	cr ." Error " dup port . ." at PC=" r> .
	." Line# " w_linenum w@ .	cr
	-1 @  								\ produce an error to quit
	; is errorISR

include ../../forth/comma.f             \ smart comma

: hi
	." Hello World!" cr
	10 0 do i . loop cr
;

: open  ( port baud -- ior )
   10 user nip
;
: close  ( -- ior )
   dup  11 user
;
: cemit  ( c -- )
   12 user  drop
;
: ckey?  ( -- flag )
   dup  13 user
;
: ckey  ( c -- )
   dup  14 user
;

\ theme=color  \ looks better in color, if your terminal isn't dumb.
