: counter       \ -- timer
   dup 2 user
;
: ms            \ ms --
   33 * counter +
   begin pause
      dup counter <
   until drop
;
