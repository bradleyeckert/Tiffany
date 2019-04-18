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

: 2literal  \ d --                      \ 8.6.1.0390
   swap  postpone literal
   postpone literal
; immediate

: 2variable  \ <name> --                \ 8.6.1.0440
    create 2 cells /allot
;

: 2constant  \ d <name> --              \ 8.6.1.0360
    create , , does> 2@
;

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

: m+    s>d d+ ;                        \ 8.6.1.1830

: m*                                    \ 8.6.1.1820
    2dup xor >r
    abs swap abs um*
    r> 0< if dnegate then
;

\ From Wil Baden's "FPH Popular Extensions"
\ http://www.wilbaden.com/neil_bawd/fphpop.txt

: tnegate                           ( t . . -- -t . . )
    >r  2dup or dup if drop  dnegate 1  then
    r> +  negate ;

: t*                                ( d . n -- t . . )
                                    ( d0 d1 n)
    2dup xor >r                     ( r: sign)
    >r dabs r> abs
    2>r                             ( d0)( r: sign d1 n)
    r@ um* 0                        ( t0 d1 0)
    2r> um*                         ( t0 d1 0 d1*n .)( r: sign)
    d+                              ( t0 t1 t2)
    r> 0< if tnegate then ;

: t/                                ( t . . u -- d . )
                                    ( t0 t1 t2 u)
    over >r >r                      ( t0 t1 t2)( r: t2 u)
    dup 0< if tnegate then
    r@ um/mod                       ( t0 rem d1)
    rot rot                         ( d1 t0 rem)
    r> um/mod                       ( d1 rem' d0)( r: t2)
    nip swap                        ( d0 d1)
    r> 0< if dnegate then ;

: m*/  ( d . n u -- d . )  >r t*  r> t/ ;


\ Double-Number extension words not defined:
\ 8.6.2.0435 2VALUE
