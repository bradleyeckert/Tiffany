\ testrom.txt file generation

\ Duplicate address in the data for testing
\ Data is little-endian
\ 263 is a prime number. I wanted something a little over 256.

: x8   ( c -- )  2 h.0 cr ;
: x32  ( x -- )  4 0 do  dup 255 and x8  8 rshift  loop drop ;
: go  ( hi lo )  ?do i 263 * x32 loop ;

