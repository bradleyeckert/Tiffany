#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// Console I/O needs a little help here. stdin is cooked input on Windows.
// We need raw input from the keyboard. The console is expected to be
// VT100, VT220, or XTERM without line buffering. It might support utf-8.
// There are two function for terminal input and and one for output,
// corresponding to {KEY?, EKEY, EMIT}.

#ifdef __linux__
#include <ncurses.h>
  #include <sched.h>
  static int lastchar = ERR;
  int tiffKEYQ (void) {
      lastchar = getc(stdin);
      if (lastchar = ERR) return 0;
      else return -1;
  }
  int tiffEKEY (void) {
      while (lastchar == ERR) {
          sched_yield();
          lastchar = getc(stdin);
      }
      return lastchar;
  }
  void tiffEMIT (uint8_t c) { putc(c,stdout); }
#elif _WIN32
#include <conio.h>
int tiffKEYQ (void) { return kbhit(); }
int tiffEKEY (void) { return getch(); }
void tiffEMIT (uint8_t c) { putch(c); }
#else
#error Unknown OS for console I/O
#endif

int32_t UserFunction (int32_t T, int32_t N, int fn ) {
    switch (fn) {
        case 0: return tiffKEYQ();
        case 1: return tiffEKEY();
        case 2: tiffEMIT((uint8_t) T);
        default: return 0;
    }
}
