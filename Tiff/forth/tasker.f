\ Proposed multitasker from 2007's Fminus
\ not tested

\ ------------------------------------------------------------------------------
\ Cooperative Multitasker
\ PAUSE's TCB (task control block) structure
\ Cells| _Name___ | __Description__________________________ |
\   1  | status   | xt of word that resumes this task       | <-- UP
\   1  | follower | address of the next task's status       |
\   1  | RP0      | initial return stack pointer            |
\   1  | SP0      | initial data stack pointer              |
\   1  | TOS      | -> top of stack                         |
\   u  | user     | user data                               |
\   s  | data     | data stack, grows downward              |
\   r  | return   | return stack, grows downward            |

:noname  cell+ @ dup @ >r ;   equ pass  \ 'status1 -- 'status2
:noname  up! tos @ sp! rp! ;  equ wake  \ 'status1 --

: pause  \ --
   0 rp  4 sp tos ! follower @ dup @ >r \ let next task execute
;
: local   status - + ;   \ tid a -- a'  \ index another task's local variable
: stop    pass status ! pause ;         \ --       sleep current task
: sleep   pass swap ! ;                 \ tid --   sleep another task
: awake   wake swap ! ;                 \ tid --   wake another task

\ u+s+r = total size of task RAM in bytes
\ u includes user variables, so it should be at least 20.

: onlytask      \ u s r tid --                  \ put first task in the queue
   dup up!  dup follower !  dup >r swap >r      \ task points to itself
   + + r> over +  follower cell+ 2! \ rp0 ! sp0 ! \ clear the stacks
   0 handler !                                  \ init other user variables
   r> sleep ;

: alsotask      \ u s r tid --                  \ add task to queue
   follower @ over follower !                   \ link new task
   >r onlytask r> follower ! ;                  \ link old task

: activate      \ tid -- | ra 'start -- ra
\ tos=sp0 -> dstack=rp0 -> rstack='start
   dup cell+ cell+                              \ point to rp0 sp0
   @+ 4 - swap @ 4 -        \ ( tid rp sp ) each stack will contain 1 item
   swap r> over !           \ save entry at rp, skip all after activate
   over !                   \ save rp at sp, save stack context for wake
   over tos local !         \ save sp in tos
   awake ;

