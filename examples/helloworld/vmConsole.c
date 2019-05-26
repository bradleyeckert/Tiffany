#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>

/*
    Console I/O.

    Exports: vmEmit, vmKey, vmQkey, vmKeyFormat
        If Linux: RawMode, CookedMode

    vmKeyFormat = 0 for Windows, 1 for Linux. The escape sequences are different.
*/

#ifdef defined(__linux__) || defined(__APPLE__)
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <termios.h>
#elif _WIN32
#include <windows.h>
#include <conio.h>
#endif // __linux__

#ifdef defined(__linux__) || defined(__APPLE__)
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

uint32_t vmQkey(uint32_t dummy) {
    Sleep(1);   // don't hog the CPU
    return (_kbhit() != 0); // 0 or 1
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

uint32_t vmEmit(uint32_t c) {
    putchar(c);
#ifdef defined(__linux__) || defined(__APPLE__)
    fflush(stdout);
    usleep(1000);
#endif
    return 0;
}
