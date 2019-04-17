\ Search Order Words

hex
: _wordlist  \ addr u -- wid            \ optional string for vocabularies
   dp dup >r @ dup cell+ r> !  >r       \ get a wid ( addr u wid )
   0 r@ !                               \ clear the wid
   [ hp0 cell+ ] literal  dup
   begin  swap @ dup -1 = until  drop
   hp @ swap ROM!                       \ resolve previous wordlist's forward link
   -1 ,h                                \ next forward link
   12345678 ,h                          \ name tag
   dup if  r@ 1000000 - ,h              \ set upper byte to FFh
      dup c,h  negate                   \ compile name: count
      begin  >r  count c,h  r> 1+
      +until 2drop
      r>  alignh  exit
   then 2drop r> dup ,h
;
: wordlist  \ -- wid                    \ 16.6.1.2460  create a wordlist
   0 dup  _wordlist
;
: (.wid)  \ addr wid --
      dup 0< if
           drop count 1F and type space
      else 3 h.x drop
      then
;
: .wid   \ wid --                       \ display WID identifier
   dup begin swap link> dup 0= until drop
   10 -                                 \ point to first possible marker
   begin  dup @ 12345678 <>  while  cell-  repeat  \ back up to marker
   cell+ @+  (.wid)
;
: .wids  \ --                           \ list wids and/or vocs in the system
   [ hp0 cell+ ] literal
   begin @ dup -1 <> while
      dup cell+ cell+ @+ (.wid)
   repeat  drop
;
decimal

: find  \ c-addr -- c-addr 0 | xt flag  \ 6.1.1550
   dup count hfind  over if             \ c-addr addr len
      drop dup xor exit                 \ not found
   then
   >r drop drop r>  xtflag
;
: get-order  \ -- widn ... wid1 n       \ 16.6.1.1647  get search order
   c_wids c@ for
      r@ cells context + @
   next   c_wids c@
;
: set-order  \ widn .. wid1 n --        \ 16.6.1.2197  set search order
   dup 0< if  drop forth-wordlist 1  then
   dup c_wids c!  0 ?do  i cells context + !  loop ;

: set-current  current ! ;   \ wid --   \ 16.6.1.2195
: get-current  current @ ;   \ -- wid   \ 16.6.1.1643
: only        -1 set-order ;            \ --
: also         get-order over swap 1+ set-order ;
: previous     get-order nip 1-       set-order ;
: definitions  \ --                     \ 16.6.1.1180
   get-order  over set-current  set-order
;
: /forth       forth-wordlist dup 1 set-order  set-current ;

: order  \ --                           \ 16.6.2.1985
   cr ."  Context: "
   context  c_wids c@ 0 ?do  @+ .wid  loop  drop
   cr ."  Current: " current @ .wid
;

\ Search Order Extensions not implemented:

\ 16.6.2.0715 ALSO
\ 16.6.2.1590 FORTH
\ 16.6.2.1965 ONLY
\ 16.6.2.2037 PREVIOUS

