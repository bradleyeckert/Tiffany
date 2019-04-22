\ EVALUATE: Given a string and length on the stack, evaluate it in the
\ current system context.

: source  \ -- c-addr len               \ 6.1.2216
   tibs 2@
;
: save-input  \ -- xn ... x1 n
   source-id  5
   dup >r  for  @+ swap
   next  drop r>
;
: restore-input  \ xn ... x1 n -- flag
   5 over <> -63 and throw              \ bogus save-input
   source-id  over cells +              \ ... x1 n address
   swap for                             \ ... x1 address
      4 - swap over !
   next  dup xor                        \ drop 0
;
: evaluate  \ i*x c-addr u -- j*x       \ 6.1.1360
   save-input n>r  ( c-addr u )
   tibs 2!  0 >in !  -1 source-id !
   ['] interpret   catch
   dup if
      cr source type cr  postpone [
   then
   nr> restore-input drop  throw
;
