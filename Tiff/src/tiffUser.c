#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// Console I/O needs a little help here. stdin is cooked input on Windows.
// We need raw input from the keyboard. The console is expected to be
// VT100, VT220, or XTERM without line buffering. It might support utf-8.
// There are two function for terminal input and and one for output,
// corresponding to {KEY?, EKEY, EMIT}.

#ifdef __linux__
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <termios.h>

struct termios orig_termios;
int isRawMode=0;

void CookedMode() {
    if (isRawMode) {
        tcsetattr(0, TCSANOW, &orig_termios);
        isRawMode = 0;
    }
}

void RawMode() {
    struct termios new_termios;

    if (!isRawMode) {
        /* take two copies - one for now, one for later */
        tcgetattr(0, &orig_termios);
        memcpy(&new_termios, &orig_termios, sizeof(new_termios));

        /* register cleanup handler, and set the new terminal mode */
        atexit(reset_terminal_mode);
        cfmakeraw(&new_termios);
        tcsetattr(0, TCSANOW, &new_termios);
        isRawMode = 1;
    }
}

int tiffKEYQ()
{
    RawMode();
    struct timeval tv = { 0L, 0L };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv);
}

int tiffEKEY()
{
    RawMode();
    int r;
    unsigned char c;
    if ((r = read(0, &c, sizeof(c))) < 0) {
        return r;
    } else {
        return c;
    }
}

#elif _WIN32
#include <sys/time.h>
#include <conio.h>
static int tiffKEYQ (void) { return kbhit(); }
static int tiffEKEY (void) { return getch(); }
#else
#error Unknown OS for console I/O
#endif

uint32_t SPIflashXfer (uint32_t n);     // import from flash.c

/**
* Returns the current time in microseconds.
*/
static long getMicrotime(){
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);
    return currentTime.tv_sec * (int)1e6 + currentTime.tv_usec;
}

/**
* Counter is time in milliseconds/10
*/
static uint32_t Counter (void) {
    return (uint32_t) getMicrotime() / 100;
}


// Emit outputs a xchar in UTF8 format
// putwchar would have been nice, if it worked for Unicode.

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
    }
    char* s = c;  char b;
    while ((b = *s++)) {
        putchar(b);     // avoid printf dependency
    }                   // else use printf("%s",c);
}

uint32_t UserFunction (uint32_t T, uint32_t N, int fn ) {
    switch (fn) {
        case 0: return (uint32_t)tiffKEYQ();    // `key?`
        case 1: return (uint32_t)tiffEKEY();    // `key`
        case 2: tiffEMIT(T);  return 0;         // `emit`
        case 3: return 1;                       // `emit?`
        case 4: return Counter();               // counter
        case 5: return SPIflashXfer(T);         // flashctrl
        case 6: exit(10);                       // bye
        default: return 0;
    }
}
