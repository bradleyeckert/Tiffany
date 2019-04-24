\ Disassembler

\ Note: Misspellings to allow host versions to run:
\ LOC instead of LOCATE

cp @ equ opnames
   ," ."     ," dup"  ," exit"  ," +"    ," user" ," 0<"   ," r>"  ," 2/"
   ," ifc:"  ," 1+"   ," swap"  ," ?"    ," ?"    ," c!+"  ," c@+" ," u2/"
   ," _"     ," ?"    ," ifz:"  ," jmp"  ," ?"    ," w!+"  ," w@+" ," and"
   ," ?"     ," litx" ," >r"    ," call" ," ?"    ," 0="   ," w@"  ," xor"
   ," reptc" ," 4+"   ," over"  ," c+"   ," ?"    ," !+"   ," @+"  ," 2*"
   ," -rept" ," ?"    ," rp"    ," drop" ," ?"    ," rp!"  ," @"   ," 2*c"
   ," -if:"  ," ?"    ," sp"    ," @as"  ," ?"    ," sp!"  ," c@"  ," port"
   ," ?"     ," lit"  ," up"    ," !as"  ," ?"    ," up!"  ," r@"  ," com"
   cp @ aligned cp !

: opname  \ opcode -- c-addr u			\ type opcode name string
   opnames  begin over while
      count +  ( cnt a )  swap 1- swap
   repeat  nip count
;

rom
cp @ equ opimmed
   7 c,
   op_call c,  op_jmp c,  op_user c,  op_lit c,  op_litx c,  op_@as c,  op_!as c,
cp @ equ opimmaddr
   2 c,
   op_call c,  op_jmp c,
   cp @ aligned cp !
ram

: isimm  \ c table -- flag				\ is this opcode an immediate type?
   swap >r  count
   begin dup while
      swap  count r@ = if  \ len a' | c
	     r> 3drop  1 exit
	  then  swap 1-
   repeat  r> drop nip
;

: _xtname  \ wid xt -- a a | xt 0       \ search for xt name in a wordlist
   begin  over cell- link>
      over = if
         drop cell+ dup exit
      then
      swap link> swap
   over 0= until swap
;
: xtname  \ xt -- addr u | xt 0         \ search in all wordlists
   c_wids c@
   begin dup while 1- dup >r
      cells context + @ link> ( wid )
      over _xtname  if
         r> drop swap drop
         count 31 and  exit
      then  drop
   r> repeat
;
: _disIR  \ IR slot opcode -- IR slot-6 | IR -1
   dup >r  opimmed isimm  if			\ display immediate field
      2dup  -1 swap lshift  invert and
	  r@ opimmaddr isimm  if			\ translate to byte address
	     cells  xtname dup if
            ColorComp type space        \ label can print
         else  drop  ColorImmA 3 h.x    \ unknown address
         then
      else
	     ColorImm .                     \ unlabeled IMM
	  then
	  drop  5
   then
   r> opname  ColorOpcod type space
   6 -
;
: disIR  \ IR --                        \ disassemble instruction group
   26 begin
   dup 0< 0= while
      2dup rshift 63 and 				\ IR slot opcode
	  _disIR
   repeat
   over 3 and							\ IR slot <op>
   over -4 = if
      _disIR dup
   then  3drop
   ColorNone
;
: _dasm  \ a -- a+4
   dup >r  ColorHi     dup 3 h.x
   @+      ColorOpcod  dup 7 h.x  disIR
   r> xtname  dup if
     ." \ "  ColorDef type
   else 2drop
   then  ColorNone  cr
;
hex
: defer?  \ addr 1 -- addr 1 false | addr addr' true
   over @ dup  1A rshift op_jmp = if    ( a 1 a IR )
      swap drop  3FFFFFF and cells  dup exit
   then dup xor \ drop 0
;
decimal

: dasm  \ addr len --                   \ disassemble groups
   dup 1 = if
      defer? if
         swap _dasm drop  10
         14 spaces ." refers to:" cr
      then
   then
   begin dup while >r _dasm  r> 1- repeat
   drop drop
;

hex
: see  \ "name" --                      \ 15.6.1.2194
   h' dup >r  cell- link>  dup
   r@ 0C - w@
   200 min  dasm
   r> 0A - c@
   2 = if                               \ CREATEd word
      @+ swap @                         ( <literal> <exit> )
      FFFFF over invert over and  if    ( <literal> <exit> mask )
         and swap  3FFFFFF and          ( xt addr )
         ."      CREATE value is "  ColorImm  @ .  ColorNone cr
         ."      DOES> action is:" cr
         cells 0A dasm exit
      else  2drop
      then
   then  drop
;
decimal

: loc  \ "name"                 LOCATE
   h' dup >r 1- c@ 8 lshift
   r@ 5 - c@ +   ." Line " .
   r> 9 - c@     ." File "
   hp0 swap invert begin
      >r @ r> 1+
   +until drop cell+
   count type cr
;
