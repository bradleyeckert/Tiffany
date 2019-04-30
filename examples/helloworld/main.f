\ Hello World example

defer coldboot
defer safemode
defer errorISR

include ../../forth/core.f
: pause ;  \ include ../../forth/tasker.f \ no multitasker
include ../../forth/timing.f
include ../../forth/numio.f             \ numeric I/O
include ../../forth/comma.f             \ smart comma

: main
	." Hello World!" cr
	10 0 do i . loop
;

include ../../forth/end.f               \ finish the app

\ Make C files
make ../../src/vm.c vm.c                \ vm.c is used as a template

\ save a hex file you could `coldboot` from
1 save-hex boot.hex                    \ only internal ROM

\ theme=color

\ bye
