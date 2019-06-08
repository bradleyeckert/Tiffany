#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "accessvm.h"
#include "rs232.h"
#include <fcntl.h>

/*
Host functions operate on the stack and have access to host resources.
Parameter passing is through the stack, whose pointer is passed to them.
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

static int R_O             ( uint32_t *s ) { // 11.6.1.2054 ( -- fam )
    s[-1] = ('r'<<0) | ('b'<<8);
    return -1;
}
static int R_W             ( uint32_t *s ) { // 11.6.1.2056 ( -- fam )
    s[-1] = ('r'<<0) | ('b'<<8) | ('+'<<16);
    return -1;
}
static int W_O             ( uint32_t *s ) { // 11.6.1.2425 ( -- fam )
    s[-1] = ('w'<<0) | ('b'<<8);
    return -1;
}
static int CLOSE_FILE      ( uint32_t *s ) { // 11.6.1.0900 ( fileid -- ior )
    int r = fclose( (FILE *) s[0] );    // assume pointer fits in uint32_t
    if (r) r = -62;
    s[0] = r;  return 0;
}
static char fam[4];
static void getfam (uint32_t x) {            // copy the fam string
    fam[0] = x & 0xFF;
    fam[1] = (x>>8) & 0xFF;
    fam[2] = (x>>16) & 0xFF;
    fam[3] = (x>>24) & 0xFF;
}
static int CREATE_FILE     ( uint32_t *s ) { // 11.6.1.1010 ( c-addr u fam -- fileid ior )
    getfam(s[0]);
    FetchString(name, s[2], s[1]);
    printf("[%s]=%s\n", name, fam);
    FILE *fp = fopen(name, fam);
    s[0] = 0;  if (fp == NULL) s[0] = -63;
    s[1] = fileno(fp);  return 1;
}


/*
static int CREATE-FILE     ( uint32_t *s ) { // 11.6.1.1010 ( c-addr u fam -- fileid ior )
static int DELETE-FILE     ( uint32_t *s ) { // 11.6.1.1190 ( c-addr u -- ior )
static int FILE-POSITION   ( uint32_t *s ) { // 11.6.1.1520 ( fileid -- ud ior )
static int FILE-SIZE       ( uint32_t *s ) { // 11.6.1.1522 ( fileid -- ud ior )
static int OPEN-FILE       ( uint32_t *s ) { // 11.6.1.1970 ( c-addr u fam -- fileid ior )
static int READ-FILE       ( uint32_t *s ) { // 11.6.1.2080 ( c-addr u1 fileid -- u2 ior )
static int READ-LINE       ( uint32_t *s ) { // 11.6.1.2090 ( c-addr u1 fileid -- u2 flag ior )
static int REPOSITION-FILE ( uint32_t *s ) { // 11.6.1.2142 ( ud fileid -- ior )
static int RESIZE-FILE     ( uint32_t *s ) { // 11.6.1.2147 ( ud fileid -- ior )
static int WRITE-FILE      ( uint32_t *s ) { // 11.6.1.2480 ( c-addr u fileid -- ior )
static int WRITE-LINE      ( uint32_t *s ) { // 11.6.1.2485 ( c-addr u fileid -- ior )
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
// void FetchString(char *s, int32_t address, uint8_t length);        // get string
// void StoreString(char *s, int32_t address);                  // store string


static int testout (uint32_t *s) {  // ( offset length -- )
    FetchString(name, s[1], s[0]);
    printf("testout[%s]\n", name);
    return 2;
}


// Host Functions are accessed through:

int HostFunction (uint32_t fn, uint32_t * s) {
    static int (* const pf[])(uint32_t*) = {
    opencomm, closecomm, commemit, commQkey, commkey, testout,
    R_O, R_W, W_O, CLOSE_FILE, CREATE_FILE
// add your own here...
    };
    if (fn < sizeof(pf) / sizeof(*pf)) {
        return pf[fn](s);
    } else {
        return 0;
    }
}

