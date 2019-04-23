\ Compiling to known-blank flash memory.

\ Non-blank cells are assumed safe to write as long as the new data doesn't
\ write '0's to existing '0' bits. The host will detect this and throw an error.
\ Physically, programming '0' to a flash memory bit twice may over-charge its
\ gate. That can cause reliability problems such as incomplete erasure and bad
\ reads of adjacent bits. Programming the same memory word twice, with '0's not
\ in the same location, should be okay. The flash manufacturers are rightly
\ paranoid about over-programming so they may recommend overly cautious
\ programming procedures rather than go into the physics of over-programming.

[undefined] SPI! [if]                   \ There is no flash memory defined

: ROM!  ! ;                             \ n addr --
: ROMC!                                 \ c addr --
	>r  invert 255 and   				\ ~c addr
	r@ 3 and 2* 2* 2* lshift       		\ c' | addr
	invert  r> -4 and !				    \ write aligned c into blank slot
;
: ROMmove  ( as ad n -- )
	negate begin >r                     \ internal ROM
		over c@ over ROMC!
		1+ swap 1+ swap
	r> 1+ +until
	drop drop drop
;

[else]

: ROM!  \ n addr --
	dup ROMsize - |-if drop ! exit |    \ internal ROM: host stores, target throws -20
	drop SPI!                           \ external ROM
;
: ROMC!  \ c addr --
	dup ROMsize - +if  drop             \ external ROM
		swap hld dup >r c!              \ ad | as
		r> swap  1 SPImove  exit        \ store 1 byte via SPI
	then  drop                          \ internal ROM
	>r  invert 255 and   				\ ~c addr
	r@ 3 and 2* 2* 2* lshift       		\ c' | addr
	invert  r> -4 and ROM!				\ write aligned c into blank slot
;
: ROMmove  ( as ad n -- )
	dup ROMsize - +if  drop             \ external ROM
		SPImove exit
	then  drop
	negate begin >r                     \ internal ROM
		over c@ over ROMC!
		1+ swap 1+ swap
	r> 1+ +until
	drop drop drop
;

[then]

\ Code and headers are both in ROM.

: ram  0 c_scope c! ;                   \ use RAM scope
: rom  1 c_scope c! ;                   \ use flash ROM scope
: h   c_scope c@ if CP exit then DP ;   \ scoped pointers
: ,x  dup >r @ ROM!  4 r> +! ;          \ n addr --
: ,c  cp ,x ;                           \ n --
: ,h  hp ,x ;                           \ n --
: ,d  dp dup >r @ !  4 r> +! ;          \ n --
: ,   c_scope c@ if ,c exit then ,d ;   \ 6.1.0150  n --

: c,x                                   \ c addr --
    dup >r @ ROMC!  r@ @ 1+ r> !
;
: c,c  cp c,x ;                         \ c --
: c,h  hp c,x ;                         \ c --
: c,d  dp dup >r @ c!  1 r> +! ;        \ c --
: c,  c_scope c@ if c,c exit then c,d ; \ 6.1.0860  c --

\ Compile the IDATA initialization table as runs of nonzero data.
\ Start up with non-essential data (TIB and PAD) blank.

: cont  \ a flag -- flag'				\ stop scan if addr is at the end
	over dp @ xor and					\ "dp @" is first free byte in data space
;
: _idata,  \ a -- mark a0 a'			\ compile a nonzero run
	cp @ swap -1 ,c						\ length to be resolved
	begin dup @ 0=  cont  while  cell+  repeat  \ skip zeros
	dup dup ,c							\ start address of non-zero run
	begin dup @ 0<>  cont  while  @+ ,c  repeat  \ compile run
;
: idata,	\ <ignored...> -- 			\ compile a new IDATA table
	tibs @ >in !
	hld [ |tib| cell+  |pad| + ] literal erase	\ wipe unneeded data
	\ HLD, TIB, PAD
	status begin
		_idata,  ( mark a0 a' -- a' )
		>r negate r@ +  2/ 2/ 			\ run length  ( mark len | 'a )
		swap over swap ROM!  r> 		\ resolve length  ( length a' )
		swap 0=
	until  drop
;
