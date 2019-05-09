\ Hello World example

defer coldboot
defer safemode
:noname coldboot ; drop                 \ error reboots

0 equ options                           \ use SW mul/div
include ../../forth/core.f
: pause ;  \ include ../../forth/tasker.f \ no multitasker
include ../../forth/timing.f
include ../../forth/numio.f             \ numeric I/O
include ../../forth/comma.f             \ smart comma

: main
	." Hello World!" cr
	10 0 do i . loop cr
;

include ../../forth/end.f               \ finish the app


\ Make C file
make ../../src/vm.c vm.c                \ vm.c is used as a template

\ Make VHDL ROM file
 make ../../templates/rom.vhd  ../../VHDL/rom32.vhd

\ save a hex file you can `coldboot` from
1 save-hex boot.hex                     \ only internal ROM

\ theme=color  \ looks better in color, if your terminal isn't dumb.

\ bye
