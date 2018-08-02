\ Define opcodes for Tiff

 0 imp_op noop
 1 imp_op dup
 2 imp_op ;
 3 imp_op +
\ 4 no|
 5 imp_op r@
 6 imp_op ;|
 7 imp_op and
\ 8 nif|
 9 imp_op over
10 imp_op r>
11 imp_op xor
\ 12 if|
13 imp_op a
14 imp_op rdrop
\ 16 +if|
17 imp_op !as
18 imp_op @a
\ 20 -if|
21 imp_op 2*
22 imp_op @a+
\ 24 next
25 imp_op u2/
26 imp_op w@a
27 imp_op a!
28 imp_op rept
29 imp_op 2/
30 imp_op c@a
31 imp_op b!

32 imm_op sp
33 imp_op com
34 imp_op !a
35 imp_op rp!
36 imm_op rp
37 imp_op port
38 imp_op !b+
38 imp_op sp!
40 imm_op up
42 imp_op w!a
43 imp_op up!
44 imm_op sh24
46 imp_op c!a
48 imm_op user
51 imp_op nip
52 imm_op jump
54 imp_op @as
56 imm_op lit
58 imp_op drop
59 imp_op rot
60 imm_op call
61 imp_op 1+
62 imp_op >r
63 imp_op swap

+cpu
