: counter       \ -- timer
   dup 4 user
;
: ms            \ ms --
   10 * counter +
   begin pause
      dup counter <
   until drop
;
