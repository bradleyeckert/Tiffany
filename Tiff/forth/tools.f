\ Tools

: _words  \ wid --                      \ display one wordlist
   @ begin
      dup >r cell+ c@+  31 and  type    \ 'name length | wid
      space r> link>
   dup 0= until  drop                   \ example: context @ @ _words
;
: wrds  \ --                            \ list all
   c_wids c@ begin
      invert 1+ invert -if: drop exit | \ finished
      dup cells context + @ _words      \ wid
   again
;

