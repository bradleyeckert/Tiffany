

\ Wean the pre-built headers off the C functions in tiffFUNC.
\ Note that replace-xt is only available in the host system due to
\ the constraints of run-time flash programming.

\ EQU pushes or compiles W parameter
: equ_ee   head @ -16 + @ ;      \ -- n
' equ_ee  -1 replace-xt          \ execution part of EQU
:noname equ_ee litral ;   -2 replace-xt          \ compilation part of EQU
:noname equ_ee Implicit ; -4 replace-xt          \ compilation part of implicit opcode

\ Executing an implicit opcode is not possible without the host VM.
\ Instead, the xte is replaced with a noname definition.

\ Run-time actions of primitives
:noname dup    ; xte-is dup
:noname +      ; xte-is +
:noname -      ; xte-is -
:noname c+     ; xte-is c+
:noname r@     ; xte-is r@
:noname and    ; xte-is and
:noname over   ; xte-is over
:noname r>     ; xte-is r>
:noname xor    ; xte-is xor
:noname 2*     ; xte-is 2*
:noname 2*c    ; xte-is 2*c
:noname c!+    ; xte-is c!+
:noname c@+    ; xte-is c@+
:noname count  ; xte-is count
:noname c@     ; xte-is c@
:noname w!+    ; xte-is w!+
:noname w@+    ; xte-is w@+
:noname w@     ; xte-is w@
:noname !+     ; xte-is !+
:noname @+     ; xte-is @+
:noname @      ; xte-is @
:noname u2/    ; xte-is u2/
:noname rept   ; xte-is rept
:noname -rept  ; xte-is -rept
:noname 2/     ; xte-is 2/
:noname sp     ; xte-is sp
:noname invert ; xte-is invert
:noname rp!    ; xte-is rp!
:noname rp     ; xte-is rp
:noname port   ; xte-is port
:noname sp!    ; xte-is sp!
:noname up     ; xte-is up
:noname up!    ; xte-is up!
:noname drop   ; xte-is drop
:noname 1+     ; xte-is 1+
:noname char+  ; xte-is char+
:noname 2+     ; xte-is 2+
:noname cell+  ; xte-is cell+
:noname >r     ; xte-is >r
:noname swap   ; xte-is swap
:noname 0=     ; xte-is 0=
:noname 0<     ; xte-is 0<

\             case 4: tiffIOR = -14;  break;
\             case 5: Immediate(w);   break;  // compile immediate opcode
\             case 6: SkipOp(w);      break;  // compile skip opcode
\             case 7: HardSkipOp(w);  break;  // compile skip opcode in slot 0
\             case 8: FlushLit();  NewGroup();   break;  // skip to new instruction group
\ 			case 9: PushNum (w);  FakeIt(opUP); break;     // execute user variable
\ 			case 10: Literal (w);  Implicit(opUP); break;  // compile user variable
\ // Compile xt must be multiple of 8 so that clearing bit2 converts to a macro
\             case 16: Compile     (FetchCell(ht-4) & 0xFFFFFF); break;  // compile call to xte
\             case 20: CompileMacro(FetchCell(ht-4) & 0xFFFFFF); break;


