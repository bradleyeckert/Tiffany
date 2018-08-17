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
#elif _WIN32
#include <conio.h>
int tiffKEYQ (void) { return kbhit(); }
int tiffEKEY (void) { return getch(); }
#else
#error Unknown OS for console I/O
#endif

// Emit outputs a xchar in UTF8 format

void tiffEMIT(uint32_t xchar) {
    char c[5];
    if (xchar<0xC0) {
        c[0] = (char)xchar;
        c[1]=0;
    } else {
        if (xchar<0x800) {
            c[0] = (char)((xchar>>6) + 0xC0);
            c[1] = (char)((xchar&63) + 0x80);
            c[2]=0;
        } else {
            if (xchar<0x10000) {
                c[0] = (char) ((xchar >> 12) + 0xE0);
                c[1] = (char) (((xchar >> 6) & 63) + 0x80);
                c[2] = (char) ((xchar & 63) + 0x80);
                c[3] = 0;
            } else {
                c[0] = (char) ((xchar >> 18) + 0xF0);
                c[1] = (char) (((xchar >> 12) & 63) + 0x80);
                c[2] = (char) (((xchar >> 6) & 63) + 0x80);
                c[3] = (char) ((xchar & 63) + 0x80);
                c[4] = 0;
            }
        }
        printf("%s",c);
    }
}

int32_t UserFunction (int32_t T, int32_t N, int fn ) {
    switch (fn) {
        case 0: return tiffKEYQ();
        case 1: return tiffEKEY();
        case 2: tiffEMIT(T);  return 0; // `emit`
        case 3: return 1;               // `emit?`
        default: return 0;
    }
}
