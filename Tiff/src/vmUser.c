#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>

#ifdef __linux__
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <termios.h>
#elif _WIN32
#include <conio.h>
#endif // __linux__

uint32_t parm1; // global N, optional second parameter

/**
User Functions may be added to the function table at the end.
All functions have the format uint32_t function (uint32_t parm0);
*/

/**
Built-in functions:
*/

// Console I/O needs a little help here. stdin is cooked input on Windows.
// We need raw input from the keyboard. The console is expected to be
// VT100, VT220, or XTERM without line buffering. It might support utf-8.
// There are two function for terminal input and and one for output,
// corresponding to {KEY?, EKEY, EMIT}.

#ifdef __linux__
// Linux uses cooked mode to input a command line (see Tiff.c's QUIT loop).
// Any keyboard input uses raw mode.
// Apparently, Windows getch does this switchover for us.
// Thanks to ncurses for providing a way to switch modes.

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

uint32_t tiffKEYQ(uint32_t dummy)
{
    RawMode();
    struct timeval tv = { 0L, 0L };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv);
}

uint32_t tiffEKEY(uint32_t dummy)
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

// Arrow keys in Linux are VT220 escape sequences.
// We are now in Windows, so need to re-map them to escape sequences.

char KbBuf[256];                        // circular input buffer
uint8_t head = 0;
uint8_t tail = 0;

static void push (uint8_t c) {          // push byte into buffer
    KbBuf[head++] = c;
}
static int size (void) {
    return 0xFF & (head-tail);
}

static void translate (const char table[][8], int len, char c) {
    for (int i=0; i<len; i++) {
        const char *s = table[i];
        char n;
        if (c == *s++) {                // translate byte to
            push ('\e');                // escape sequence:
            while((n = *s++)) {push(n);}
            return;
        }
    }
    push(c);                            // no translation
}

static const char cursor_table[10][8] = {
    "K[D", "H[A",  "P[B",  "M[C",       // left up down right
    "G[H", "I[5~", "Q[6~", "O[F",       // home PgUp PgDn end
    "s[1;5D", "t[1;5C"                  //^left ^right
};

static const char function_table[30][8] = {
    ";[P",    "<[Q",    "=[R",    ">[S",    // F1 F2 F3 F4
    "?[15",   "@[16",   "A[17",   "B[18",   // F5 F6 F7 F8
    "C[19",   "D[1:",                       // ctrl:
    "^[P",    "_[Q",    "`[R",    "a[S",    // F1 F2 F3 F4
    "b[1;5P", "c[1;5Q", "d[1;5R", "e[1;5S", // F5 F6 F7 F8
    "e[1;5T", "e[1;5U",                     // shift:
    "T[1;2P", "U[1;2Q", "V[1;2R", "W[1;2S", // F1 F2 F3 F4
    "X[15;2~","Y[16;2~","Z[17;2~","[[18;2~",// F5 F6 F7 F8
    "\[20;2~","][21;2~"                     // F9 F10
};

static int winFill (void) {
    uint8_t c;
    while (kbhit()) {                   // got data?
        if (size() > 248) break;        // FIFO is full
        c = _getch();
        switch (c) {
            case 0:                     // re-map function keys
                translate(function_table, 30, _getch());
                break;
            case 0x0E0:                 // re-map arrow keys
                translate(cursor_table, 10, _getch());
                break;
            default:
                push (c);
                break;
        }
    }
    return size();
}

static uint32_t tiffKEYQ (uint32_t dummy) {
    return winFill();
}

static uint32_t tiffEKEY (uint32_t dummy) {
    while (!winFill()) {};
    uint8_t c = KbBuf[tail++];
    return c;
}

#else
#error Unknown OS for console I/O
#endif

////////////////////////////////////////////////////////////////////////////////
// Non-keyboard stuff...

uint32_t SPIflashXfer (uint32_t n); // import from flash.c

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
static uint32_t Counter (uint32_t dummy) {
    return (uint32_t) getMicrotime() / 100;
}

// Emit outputs a xchar in UTF8 format

uint32_t tiffEMIT(uint32_t xchar) {
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
    return 0;
}
static uint32_t tiffQEMIT(uint32_t dummy) {
    return 1;           // always ready to emit
}

static uint32_t tiffBye(uint32_t dummy) {
    exit(10);  return 0;
}


uint32_t UserFunction (uint32_t T, uint32_t N, int fn ) {
    parm1 = N;
    static uint32_t (* const pf[])(uint32_t) = {
        tiffKEYQ, tiffEKEY, tiffEMIT, tiffQEMIT,
        Counter, SPIflashXfer, tiffBye
// add your own here...
    };
    if (fn < sizeof(pf) / sizeof(*pf)) {
        return pf[fn](T);
    } else {
        return 0;
    }
}
