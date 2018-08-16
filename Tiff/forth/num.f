
\ convert char to uppercase
: toupper  \ c -- c'
   dup [char] a [char] { within \ }
   if 32 - then
;
\ convert char to digit, return `invalid` flag
: digit  \ c -- n bad?
   toupper [char] 0 -  dup 0< >r
   dup 10 17 within >r
   dup 9 > 7 and -
   r> r> or
   over base @ - 0< 0= or
;
\ convert string to number until non-digit
: >number ( ud1 c-addr1 u1 -- ud2 c-addr2 u2)
   begin
      dup 0= if exit then
      over c@ digit             \ ud a u n bad?
      if drop exit then
      swap >r swap >r >r        \ ud | u a n
      swap >r
      base @ dup >r um*         \ dH*base | u a n L base
      r> r> um* d+  r> 0 d+
      r> r>  1 /string
   again
;

