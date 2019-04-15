\ Disassembler

cp @ equ opnames
   ," ."     ," dup"  ," exit"  ," +"    ," user" ," 0<"   ," r>"  ," 2/"
   ," ifc:"  ," 1+"   ," swap"  ," -"    ," ?"    ," c!+"  ," c@+" ," u2/"
   ," _"     ," 2+"   ," ifz:"  ," jmp"  ," ?"    ," w!+"  ," w@+" ," and"
   ," ?"     ," litx" ," >r"    ," call" ," ?"    ," 0="   ," w@"  ," xor"
   ," rept"  ," 4+"   ," over"  ," c+"   ," ?"    ," !+"   ," @+"  ," 2*"
   ," -rept" ," ?"    ," rp"    ," drop" ," ?"    ," rp!"  ," @"   ," 2*c"
   ," -if:"  ," ?"    ," sp"    ," @as"  ," ?"    ," sp!"  ," c@"  ," port"
   ," +if"   ," lit"  ," up"    ," !as"  ," ?"    ," up!"  ," r@"  ," com"
   cp @ aligned cp !

: opname  \ opcode --					Type opcode name string
   opnames  begin over while
      count +  ( cnt a )  swap 1- swap
   repeat  nip count type
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

: isimm  \ c table -- flag				Is this opcode an immediate type?
   swap >r  count
   begin dup while
      swap  count r@ = if  \ len a' | c
	     r> 3drop  1 exit
	  then  swap 1-
   repeat  r> drop nip
;

: _disIR  \ IR slot opcode -- IR slot-6 | IR -1
   dup >r  opimmed isimm  if			\ display immediate field
      2dup  -1 swap lshift  invert and
	  r@ opimmaddr isimm  if			\ translate to byte address
	     cells
	  then
	  3 h.x
	  drop  5
   then
   r> opname space
   6 -
;
: disIR  \ IR --
   26 begin
   dup 0< 0= while
      2dup rshift 63 and 				\ IR slot opcode
	  _disIR
   repeat
   over 3 and							\ IR slot <op>
   over -4 = if
      _disIR dup
   then  3drop
;
: _dasm  \ addr len --
   begin dup while >r
      dup 3 h.x  @+  dup 7 h.x  disIR  cr
   r> 1- repeat  drop drop
;

: _see  \ "name"
   h' dup cell- link>  swap 3 + c@  _dasm
;

: loc  \ "name"
   h' dup >r 1- c@ 8 lshift
   r@ 5 - c@ +   ." Line " .
   r> 9 - c@     ." File "
   hp0 swap invert begin
      >r @ r> 1+
   +until drop cell+
   count type cr
;


