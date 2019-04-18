\ Tools

: ;.  \ n --                            \ output number in decimal
   base dup >r @  [char] ; emit
   swap  decimal  0 .r  r> !            \ ";nn"
;
: colorize  \ --
    c_theme c@ ifz: r> drop exit |      \ abort if monochrome
; call-only
: theme=mono   0 c_theme c! ;           \ --
: theme=color  1 c_theme c! ;           \ --

\ VT220 / XTERM colors
: esc[   27 emit  [char] [ emit  emit ; \ c --   "<esc>[c"
: esc[0  [char] 0 esc[ ;                \ --     "<esc>[0"
: esc[1  [char] 1 esc[ ;                \ --     "<esc>[1"
: ]m     [char] m emit ;

: ColorNone  colorize  esc[0 ]m ;       \ reset colors  "\033[0m"
: color_x    esc[1 ;. ]m ;   \ n --     \ bright color
: color_xd   esc[0 ;. ]m ;   \ n --     \ dim color
: ColorHi    colorize  35 color_x ;     \ hilighted = magenta
: ColorDef   colorize  31 color_x ;     \ definition = red
: ColorComp  colorize  32 color_x ;     \ compiled = green
: ColorImm   colorize  33 color_x ;     \ immediate = yellow
: ColorImmA  colorize  33 color_xd ;    \ immediate address = dim yellow
: ColorOpcod ColorNone ;

rom cp @                                \ FG  BG
   30 c, 44 c,                          \ 30, 40 = Black
   31 c, 44 c, \ bit 0 = smudged        \ 31, 41 = Red
   30 c, 40 c,                          \ 32, 42 = Green
   31 c, 40 c,                          \ 33, 43 = Yellow
   33 c, 44 c, \ bit 2 = not call-only  \ 34, 44 = Blue
   31 c, 44 c,                          \ 35, 45 = Magenta
   33 c, 40 c, \ bit 1 = public         \ 36, 46 = Cyan
   31 c, 40 c,                          \ 37, 47 = White
equ wordcolors
ram

: ColorWord  \ n --                     \ color type 0 to 7
   c_theme c@ ifz: drop exit |
   2* wordcolors + c@+ swap c@ swap     ( bg fg )
   esc[1 ;. ;. ]m
;

{ colorForth colors, for reference:
_Color_			_Time_		_Purpose_
White			Ignored		Comment (ignored by assembler)
Gray			Compile		Literal Instruction (packed by assembler)
Red				Compile		Add name/address pair to dictionary.
Yellow number	Compile		Immediately push number (assembly-time)
Yellow word		Compile		Immediately call (i.e. assembly-time macro)
Green number	Execute		Compile literal (pack @p and value)
Green word		Execute		Compile call/jump (to defined red word)
Blue			Display		Format word (editor display-time)
}

: .s  \ ? -- ?                          \ 15.6.1.0220  stack dump
   depth  ?dup if
      negate begin
         dup negate pick .
      1+ +until drop
   then ." <sp " cr
;

\ Dump in cell and char format

: dump  \ addr bytes --                 \ 15.6.1.1280
   swap -4 and swap                     \ cell-align the address
   begin dup while
      cr over  ColorHi 3 h.x  ColorImm
      2dup  [ DumpColumns ] literal
      dup >r min  r> over - 9 * >r      \ a u addr len | padding
      begin dup while >r                \ dump cells in 32-bit hex
         @+ 7 h.x r> 1-
      repeat  2drop  r> spaces
      2dup  [ DumpColumns 2* 2* ] literal min
      ColorComp
      begin dup while >r                \ dump chars in ASCII
         c@+
         dup bl 192 within |ifz drop [char] .
         emit  r> 1-                    \ outside of 32 to 191 is '.'
      repeat 2drop
      [ DumpColumns 2* 2* ] literal /string
      0 max
   repeat  2drop
   ColorNone  cr
;

\ http://lars.nocrew.org/forth2012/tools/NtoR.html
: N>R \ xn .. x1 N - ; R: -- x1 .. xn n \ 15.6.2.1908
\ Transfer N items and count to the return stack.
   DUP                        \ xn .. x1 N N --
   BEGIN
      DUP
   WHILE
      ROT R> SWAP >R >R       \ xn .. N N -- ; R: .. x1 --
      1-                      \ xn .. N 'N -- ; R: .. x1 --
   REPEAT
   DROP                       \ N -- ; R: x1 .. xn --
   R> SWAP >R >R
; call-only

: NR> \ - xn .. x1 N ; R: x1 .. xn N -- \ 15.6.2.1940
\ Pull N items and count off the return stack.
   R> R> SWAP >R DUP
   BEGIN
      DUP
   WHILE
      R> R> SWAP >R -ROT
      1-
   REPEAT
   DROP
; call-only

