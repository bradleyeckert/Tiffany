
| 5:3\2:0 | 0         | 1          | 2         | 3         | 4         | 5         | 6         | 7         |
|:-------:|:---------:|:----------:|:---------:|:---------:|:---------:|:---------:|:---------:|:---------:|
| **0**   | nop       | dup        | exit      | +         | 2\*       | port      |           |           |
| **1**   | *no:*     | 1+         | r>        | c+        | 2\*c      | **user**  |           |           |
| **2**   | @         | rp         | r@        | and       | 2/        | **jmp**   |           |           |
| **3**   | @+        | sp         | >r        | xor       | u2/       | **call**  |           |           |
| **4**   | *reptc*   | 4+         | !+        | drop      | 0=        | **litx**  |           |           |
| **5**   | *-rept*   | up         | lane      | rp!       | 0<        | **@as**   |           |           |
| **6**   | *-if:*    | rot8       |           | sp!       | invert    | **!as**   |           |           |
| **7**   | *ifc:*    | over       | *ifz:*    | up!       | swap      | **lit**   |           |           |

Byte fetches can be done by shift-and-mask:
: c@  ( a -- c )
   dup 2/ 2/ swap 3
   and negate  swap @  ( -position u )  \ 0 -1 -2 -3
   | rot8 -rept 255 and
;

: @+  dup cell+ swap @ ;

: c!
;

