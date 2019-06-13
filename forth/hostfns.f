\ Host functions

: COM-OPEN    ( port baud -- ior )  0 host ;
: COM-CLOSE   ( -- )                1 host ;
: COM-EMIT    ( c -- )              2 host ;
: COM-KEY?    ( -- flag )           3 host ;
: COM-KEY     ( c -- )              4 host ;
: testout     ( addr len -- )       5 host ; \ string to console, may remove

1 constant R/O  \ 11.6.1.2054 ( -- fam )
2 constant W/O  \ 11.6.1.2425 ( -- fam )
3 constant R/W  \ 11.6.1.2056 ( -- fam )

: CLOSE-FILE      ( fileid -- ior )                     6 host ;
: CREATE-FILE     ( c-addr u fam -- fileid ior )        7 host ;
: OPEN-FILE       ( c-addr u fam -- fileid ior )        8 host ;
: READ-FILE       ( c-addr u1 fileid -- u2 ior )        9 host ;
: READ-LINE       ( c-addr u1 fileid -- u2 flag ior )  10 host ;
: FILE-POSITION   ( fileid -- ud ior )                 11 host ;
: REPOSITION-FILE ( ud fileid -- ior )                 12 host ;
: WRITE-FILE      ( c-addr u fileid -- ior )           13 host ;
: WRITE-LINE      ( c-addr u fileid -- ior )           14 host ;
: FILE-SIZE       ( fileid -- ud ior )                 15 host ;
