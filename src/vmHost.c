#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "vm.h"
#include "accessvm.h"
#include "rs232.h"
#define MAXFILES 64

/*
Host functions operate on the stack and have access to host resources.
Parameter passing is through the stack, whose pointer is passed to them.
The return value is the number of cells to offset the stack immediately after return.
*/

// =============================================================================
// COM port stuff adds about 5K to the executable. Not bad for Windows.

static int activeport;

// COM port parameters: N = BaudRate, T = Port, returns ior=0 if opened
static int opencomm (uint32_t *s) {  // ( rate port -- ior )
    int r = RS232_OpenComport(s[0], s[1], "8N2", 0);
    if (!r) activeport = s[0];
    s[1] = r;
    return 1;
}
static int closecomm (uint32_t *s) {  // ( -- )
    RS232_CloseComport(activeport);
    return 0;
}
// Send one byte
static int commemit (uint32_t *s) {  // ( c -- )
    RS232_SendByte(activeport, s[0]);
    return 1;
}

static unsigned char buf[4];
static int full = 0;

static int commQkeyC (void) {
    if (full) {
        return 1;
    }
    int r = RS232_PollComport(activeport, buf, 1);
    full = r;
    return r;
}

static int commQkey (uint32_t *s) {  // ( -- n )
    s[-1] = commQkeyC();
    return -1;
}

static int commkey (uint32_t *s) {  // ( -- c )
    while (commQkeyC() == 0) { Sleep(1); }
    full = 0;
    s[-1] = buf[0];
    return -1;
}

// =============================================================================
// File access words

static char name[1024];

// unimplemented or already in Tiff:
// 11.6.1.0765 BIN
// 11.6.1.2165 S"
// 11.6.1.1717 INCLUDE-FILE
// 11.6.1.1718 INCLUDED
// 11.6.1.2218 SOURCE-ID

// Rather than cast uint32_t to and from file pointers and wait for C to break
// the code, use an array of actual file pointers.

FILE * filehandle[MAXFILES];

void vmHostInit(void) {
    for (int i = 0; i<MAXFILES; i++) {
        filehandle[i] = NULL;
    }
}

static int getFilePointer(FILE * fp) {
    for (int i = 0; i<MAXFILES; i++) {
        if (filehandle[i] == NULL) {
            filehandle[i] = fp;
            return i;                   // first free slot
        }
    }
    return -1;                          // none found
}
static FILE * FilePointer(uint32_t fp) {
    if (fp<MAXFILES) {
        return filehandle[fp];
    }
    return NULL;                        // translate to a nicer invalid pointer
}

static int CLOSE_FILE ( uint32_t *s ) { // 11.6.1.0900 ( fileid -- ior )
    FILE * f = FilePointer(s[0]);
    int r = fclose(f);
    filehandle[s[0]] = NULL;
    s[0] = r ? -62 : 0;
    return 0;
}
// Supported?   R/O  R/W  W/O
// CREATE-FILE  no   yes  yes
// OPEN-FILE    yes  yes  no
static int CREATE_FILE ( uint32_t *s ) { // 11.6.1.1010 ( c-addr u fam -- fileid ior )
    FetchString(name, s[2], s[1]);
    char * fam = "rb";
    switch (s[0]) {
        case 2: fam = "wb";   break;
        case 3: fam = "rb+";  break;
        default:  break;
    }
    FILE *fp = fopen(name, fam);
    s[1] = (fp == NULL) ? -62 : 0;
    s[2] = getFilePointer(fp);
    return 1;
}
static int READ_FILE ( uint32_t *s ) { // 11.6.1.2080 ( c-addr u1 fileid -- u2 ior )
    uint32_t address = s[2];
    uint32_t length = s[1];
    FILE * f = FilePointer(s[0]);
    s[2] = 0;  s[1] = 0;
    for (uint32_t i=0; i<length; i++) {
        int c = fgetc(f);
        if (c == EOF) {
            s[2] = i;
            return 1;
        }
        StoreByte(c, address++);
        s[2]++;
    }
    return 1;
}

/*
All lines in the file must end in newline, including the last.
This allows too-long lines to be truncated to fit the specified buffer.
The trailing CRLF is not stored in the buffer, which is a behavior you can't count on in all ANS Forths.
For ANS compatibility, you should define your buffer two bytes bigger than u1.
If you want to detect truncated lines, compare u1 to u2.
*/
static int READ_LINE ( uint32_t *s ) { // 11.6.1.2090 ( c-addr u1 fileid -- u2 flag ior )
    uint32_t address = s[2];
    uint32_t length = s[1];
    FILE * f = FilePointer(s[0]);
    s[2] = 0;  s[1] = 0;  s[0] = 0;
    int pos = 0;
    while (1) {
        int c = fgetc(f);
        switch (c) {
            case EOF:
                return 0;
            case '\n':
                s[1] = -1;
                return 0;
            default:
                if ((c >= ' ') && (pos < length)) {
                    StoreByte(c, address++);
                    s[2]++;
                }
                break;
        }
        if (ferror(f)) {
            s[0] = -71;
            return 0;
        }
    }
    return 0;
}

static int FILE_POSITION ( uint32_t *s ) { // 11.6.1.1520 ( fileid -- ud ior )
    FILE * f = FilePointer(s[0]);
    s[0] = 0;  s[-1] = 0;  s[-2] = -65;
    fpos_t pos;
    int ior = fgetpos(f, &pos);
    if (ior == 0) {
        uint64_t x = (uint64_t) pos;
        s[0] = x & 0xFFFFFFFF;
        s[-1] = x >> 32;
        s[-2] = 0;
    }
    return -2;
}
static int REPOSITION_FILE ( uint32_t *s ) { // 11.6.1.2142 ( ud fileid -- ior )
    FILE * f = FilePointer(s[0]);
    uint64_t x = (((uint64_t) s[1])<<32) | (uint64_t) s[2];
    fpos_t pos = (fpos_t) x;
    int ior = fsetpos(f, &pos);
    s[2] = (ior) ? -73 : 0;
    return 2;
}

static int WRITE_FILE ( uint32_t *s ) { // 11.6.1.2480 ( c-addr u fileid -- ior )
    uint32_t address = s[2];
    uint32_t length = s[1];
    FILE * f = FilePointer(s[0]);
    s[2] = 0;
    for (int i=0; i<length; i++) {
        int c = FetchByte(address++);
        int n = fputc(c, f);
        if (n == EOF) {
            s[2] = -75;
            return 2;
        }
    }
    return 2;
}

static int WRITE_LINE ( uint32_t *s ) { // 11.6.1.2480 ( c-addr u fileid -- ior )
    FILE * f = FilePointer(s[0]);
    int r = WRITE_FILE(s);
#ifdef _WIN32
    fputc('\r', f);
#endif
    fputc('\n', f);
    return r;
}

static int FILE_SIZE ( uint32_t *s ) { // 11.6.1.1522 ( fileid -- ud ior )
    FILE * f = FilePointer(s[0]);
    fseek(f, 0L, SEEK_END);
    uint64_t x = ftell(f);
    fseek(f, 0L, SEEK_SET);
    s[0] = x & 0xFFFFFFFF;          // low ud
    s[-1] = (x>>32) & 0xFFFFFFFF;   // high ud
    s[-2] = 0;                      // ior
    return -2;
}

/*
static int OPEN-FILE       ( uint32_t *s ) { // 11.6.1.1970 ( c-addr u fam -- fileid ior )
static int DELETE-FILE     ( uint32_t *s ) { // 11.6.1.1190 ( c-addr u -- ior )

static int RESIZE-FILE     ( uint32_t *s ) { // 11.6.1.2147 ( ud fileid -- ior )
*
-61	RESIZE
-62	CLOSE-FILE
-63	CREATE-FILE
-64	DELETE-FILE
-65	FILE-POSITION
-66	FILE-SIZE
-67	FILE-STATUS
-68	FLUSH-FILE
-69	OPEN-FILE
-70	READ-FILE
-71	READ-LINE
-72	RENAME-FILE
-73	REPOSITION-FILE
-74	RESIZE-FILE
-75	WRITE-FILE
-76	WRITE-LINE
*/
// void FetchString(char *s, int32_t address, uint8_t length);  // get string
// void StoreString(char *s, int32_t address);                  // store string


static int testout (uint32_t *s) {  // ( address length -- )
    FetchString(name, s[1], s[0]);
    printf("testout[%s]\n", name);
    return 2;
}


// Host Functions are accessed through:

int HostFunction (uint32_t fn, uint32_t * s) {
    static int (* const pf[])(uint32_t*) = {
    opencomm, closecomm, commemit, commQkey, commkey,
    testout,
    CLOSE_FILE, CREATE_FILE, CREATE_FILE,     // open-file uses create-file
    READ_FILE, READ_LINE, FILE_POSITION, REPOSITION_FILE, WRITE_FILE, WRITE_LINE,
    FILE_SIZE
// add your own here...
    };
    if (fn < sizeof(pf) / sizeof(*pf)) {
        return pf[fn](s);
    } else {
        return 0;
    }
}

