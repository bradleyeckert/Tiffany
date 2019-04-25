#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>

/*
    Console I/O.

    Exports: vmEmit, vmQemit, vmKey, vmQkey, vmKeyFormat
        If Linux: RawMode, CookedMode

    vmKeyFormat = 0 for Windows, 1 for Linux. The escape sequences are different.
*/

#ifdef __linux__
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <termios.h>

// Linux uses cooked mode to input a command line (see Tiff.c's QUIT loop).
// Any keyboard input uses raw mode.
// Apparently, Windows getch does this switchover for us.
// Thanks to ncurses for providing a way to switch modes.

static struct termios orig_termios;
static int isRawMode=0;

void CookedMode() {
    if (isRawMode) {
        tcsetattr(0, TCSANOW, &orig_termios);
        isRawMode = 0;
    }
}

void RawMode() {
    struct termios new_termios;

    if (!isRawMode) {
        isRawMode = 1;
        /* take two copies - one for now, one for later */
        tcgetattr(0, &orig_termios);
        memcpy(&new_termios, &orig_termios, sizeof(new_termios));

        /* register cleanup handler, and set the new terminal mode */
        atexit(CookedMode); // in case BYE executes while in raw mode
        cfmakeraw(&new_termios);
        tcsetattr(0, TCSANOW, &new_termios);
    }
}

uint32_t vmKeyFormat(uint32_t dummy) {
    return 1;
}

uint32_t vmQkey(uint32_t dummy)
{
    RawMode();
    struct timeval tv = { 0L, 0L };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv);
}

uint32_t vmKey(uint32_t dummy)
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
#include <windows.h>
#include <conio.h>

uint32_t vmQkey(uint32_t dummy) {
    Sleep(1);   // don't hog the CPU
    return _kbhit();
}

uint32_t vmKey(uint32_t dummy) {
    return _getch();
}
uint32_t vmKeyFormat(uint32_t dummy) {
    return 0;
}

#else
#error Unknown OS for console I/O
#endif

uint32_t vmEmit(uint32_t xchar) {
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
    puts(c);
#ifdef __linux__
    fflush(stdout);
    usleep(1000);
#endif
    return 0;
}
uint32_t vmQemit(uint32_t dummy) {
    return 1;           // always ready to emit
}
