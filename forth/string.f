\ String wordset

\ CMOVE, CMOVE>, /STRING are in core.f

: compare  \ c-addr1 u1 c-addr2 u2 -- n \ 17.6.1.0935
   rot swap  2dup max >r
   - r> swap >r >r 						\ a1 a2 | sign count
   rfor
      c@+ >r swap c@+ >r swap           \ a1' a2' | sign count c2 c1
	  r> r> -                           \ a1' a2' diff
	  dup if
	     r> drop  r> drop				\ unloop
	     >r 2drop r> 0< 2* 1+ exit
      then drop
   next  2drop
   r> dup if							\ got all the way through
      0< 2* 1+ exit
   then
;

: blank  bl fill ;  \ c-addr len        \ 17.6.1.0780

: -trailing  \ a u -- a u'              \ 17.6.1.0170
   begin  2dup + 1- c@ bl =  over and   while
   1- repeat
;

\ Search the string specified by c-addr1 u1 for the string
\ specified by c-addr2 u2. If flag is true, a match was found
\ at c-addr3 with u3 characters remaining. If flag is false
\ there was no match and c-addr3 is c-addr1 and u3 is u1.

: search  \ a1 u1 a2 u2 -- a3 u3 flag   \ 17.6.1.2191
   dup ifz: drop exit |   \ special-case zero-length search
   2>r 2dup
   begin
      dup
   while
      2dup 2r@            ( c-addr1 u1 c-addr2 u2 )
      rot over min -rot   ( c-addr1 min_u1_u2 c-addr2 u2 )
      compare 0= if
         2swap 2drop 2r> 2drop true exit
      then
      1 /string
   repeat
   2drop 2r> 2drop
   false
;

\ 17.6.1.2212 SLITERAL

\ String extension words 17.6.2
\ 17.6.2.2141 REPLACES
\ 17.6.2.2255 SUBSTITUTE
\ 17.6.2.2375 UNESCAPE
