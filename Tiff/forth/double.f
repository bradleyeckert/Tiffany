\ Doubles

: d>s                  drop ; macro     \ 8.6.1.1140
: 2rot  2>r 2swap 2r> 2swap ;           \ 8.6.2.0420
: -2rot           2rot 2rot ;
: 2nip          2swap 2drop ;
: 2tuck         2swap 2over ;
: d-             dnegate d+ ;           \ 8.6.1.1050
: d<                  d- 0< ;
: d=                  d- 0= ;           \ 8.6.1.1120
: d0=                 or 0= ; macro     \ 8.6.1.1080  d -- flag
: d0<          swap drop 0< ; macro     \ 8.6.1.1075  d -- flag
: d2*      swap 2* swap 2*c ;           \ 8.6.1.1090  d -- d'
hex
: d2/
   2/ swap ifnc  2/ swap exit then      \ 8.6.1.1100  d -- d'
   80  0 litx  or  swap
;
decimal

\ 8.6.1.0360 2CONSTANT
\ 8.6.1.0390 2LITERAL
\ 8.6.1.0440 2VARIABLE

\ 8.6.1.1820 M*/
\ 8.6.1.1830 M+

: d< ( d1 d2 -- flag )                  \ 8.6.1.1110
   rot  2dup = if  2drop u<  exit then  2nip >
;
: du<  \ ud1 ud2 -- flag                \ 8.6.2.1270
   rot  2dup = if  2drop u<  exit then  2nip swap u<
;
: dmax  \ d1 d2 -- d3                   \ 8.6.1.1210
   2over 2over d< if  2swap  then  2drop
;
: dmin ( d d2 -- d3 )                   \ 8.6.1.1220
   2over 2over d< 0= if  2swap  then  2drop
;

\ Double-Number extension words undefined:
\ 8.6.2.0435 2VALUE
