\ Forth kernel for Tiff

include core.f
include io.f

\ Interpreter words

: [         0 state ! ;
: ]         -1 state ! ;
: source    tibs 2@ ;                   \ -- addr len  entire source string
: /source   source >in @ /string ;      \ -- addr len  remaining source string

: skip  \ c-addr1 u1 char -- c-addr2 u2
   >r begin
      dup ifz: r> drop exit |           \ zero length quits
      over c@ r@ xor 0=
      |ifz r> drop exit |               \ mismatch quits
      1 /string
   again
;
: scan  \ c-addr1 u1 char -- c-addr2 u2
   >r begin
      dup ifz: r> drop exit |           \ zero length quits
      over c@ r@ xor
      |ifz r> drop exit |               \ mismatch quits
      1 /string
   again
;
: parse  \ char "ccc<char>" -- c-addr u
   /source  over >r  rot scan  r>  rot over -  rot 0<>
   1 and over + >in +!
;
: parse-word  \ "<spaces>name" -- c-addr u
   /source over >r  bl skip drop r> - >in +!  bl parse
;
: .(  [char] ) parse type ;             \ parse to output

\ A version of FIND that accepts a counted string and returns a header token.
\ `tolower` converts a character to lowercase
\ `c_caseins` is the case-insensitive flag
\ `match` checks two strings for mismatch and keeps the first string
\ `_hfind` searches one wordlist for the string

: tolower  \ c1 -- c2
   dup [char] A [char] [ within  if 32 + then
;
: link>  \ a1 -- a2
   @ 16777215 and                       \ mask off upper 8 bits
;
: match  \ a1 n1 a2 n2 -- a1 n1 0 | nonzero
   third over xor 0=                    \ n2 <> n1
   |ifz drop dup xor exit |             \ a1 n1 0 | a1 n1 a2 n2
   drop over >r third >r  swap negate   \ a1 a2 -n | n1 a1
   begin >r
      c@+ >r swap c@+ r>                \ a2' a1' c1 c2 | n1 a1 -n
      c_caseins c@ if
         tolower swap tolower
      then
      xor if                            \ mismatch
         r> 3drop r> r>
         dup dup xor exit               \ 0
      then
   r> 1+ +until
   3drop r> drop r> 1+                  \ flag is n1+1
;
: _hfind  \ addr len wid -- addr len 0 | ht    Search one wordlist
   @ over 31 u> -19 and throw           \ name too long
   over ifz: dup xor exit |             \ addr 0 0   zero length string
   begin
      dup >r cell+ c@+  63 and          \ addr len 'name length | wid
      match if
         r> exit                        \ return only the ht
      then
      r> link>
   dup 0= until                         \ not found
;
: hfind  \ addr len -- addr len | 0 ht  \ search the search order
   c_wids c@ begin
      invert 1+ invert -if: drop exit | \ finished
      >r
      r@ cells context + @ _hfind       \ wid
      ?dup if
        r> dup xor swap exit
      then
      r> dup
   again
;
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


\ not working yet - >number gets bad result

: digit?       \ c base -- u f
   >r [char] 0 - 9 over <
   if 7 - dup 10 < + then dup r> u<
;
: >number      \ ud a u -- ud a u   ( 6.1.0570 )
   begin dup
   while >r  dup >r c@ base @ digit?
   while swap base @ um* drop rot base @ um* d+ r> char+ r> 1-
   repeat drop r> r> then
;



\ Some test words

: foo hex decimal ;


cp @ equ s1 ," 123456"  : str1 s1 count ;
cp @ equ s2 ," the quick brown fox"  : str2 s2 count ;



