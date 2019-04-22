\ EKEY conversion from arrow keys

\ Depends on terminal type since Linux and Windows treat cursor and function keys differently.
\ Linux (and embedded with PuTTY) use VT220 cursor and function keys

\ For testing, get keys until SPACE. ESC prints on newline so you see the sequences you get.
\ Then, you can figure out how to translate them to single EKEY tokens.

: keytest  \ --
   begin  key
      dup 32 = if drop exit then
      dup 27 = if drop cr  else
	    dup bl 128 within  if emit else ." <" 0 .r ." >" then
	  then
   again
;
