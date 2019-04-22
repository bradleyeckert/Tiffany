\ Cooperative Multitasker for 2-register TOS.

\ Only pause has been tested: doesn't crash when just the terminal is in the loop.
\ Not much of a test.

\ PAUSE's TCB (task control block) structure
\ Cells| _Name___ | __Description____________________ |
\   1  | status   | xt of word that resumes this task | <-- UP
\   1  | follower | address of the next task's status |
\   1  | RP0      | initial return stack pointer      |
\   1  | SP0      | initial data stack pointer        |
\   1  | TOS      | -> top of stack                   |
\   u  | user     | user data                         |
\   s  | data     | data stack, grows downward        |
\   r  | return   | return stack, grows downward      |

:noname  cell+ @ dup @ >r               \ rp rp status -- rp rp status'
; equ pass
:noname  up! tos @ sp!  drop rp!
; equ wake

status follower !                    	\ put terminal in task list
wake status !                        	\ let it run by itself

\ pause takes 30 cycles with only the terminal task active

: pause                                 \ --
   0 rp  dup  4 sp tos !                \       N  T
   follower @ dup @ >r                  \ -- rp rp follower
;                                       \     ^---------tos

: local   status - + ;   \ tid a -- a'  \ index another task's local variable
: stop    pass status ! pause ;         \ --       sleep current task
: sleep   pass swap ! ;                 \ tid --   sleep another task
: awake   wake swap ! ;                 \ tid --   wake another task

\ u+s+r = total size of task RAM in bytes
\ u includes user variables, so it should be at least 20.

: onlytask      \ u s r tid --          \ put first task in the queue
   dup up!  dup follower !              \ task points to itself
   dup >r swap >r
   + + r> over +  follower cell+ 2!     \ rp0 ! sp0 ! \ clear the stacks
   r> sleep
;
: alsotask      \ u s r tid --          \ add task to queue
   follower @ over follower !           \ link new task
   >r onlytask r> follower !            \ link old task
;
: activate      \ tid -- | ra 'start -- ra
\ tos=sp0 -> dstack=rp0 -> rstack='start
   dup cell+ cell+                      \ point to rp0 sp0
   @+ 4 - swap @ 4 -                    \ ( tid rp sp ) each stack will contain 1 item
   swap r> over !                       \ save entry at rp, skip all after activate
   over !                               \ save rp at sp, save stack context for wake
   over tos local !                     \ save sp in tos
   awake
;
