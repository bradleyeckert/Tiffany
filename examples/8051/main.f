\ Hello World example

defer coldboot
defer safemode
defer errorISR

0 equ options
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

make ../../templates/app.A51 C:\SiliconLabs\SimplicityStudio\M2\src\vm.A51

\ theme=color

bye
