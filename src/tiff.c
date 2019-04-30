#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "vm.h"
#include "tiff.h"
#include "accessvm.h"
#include "compile.h"
#include "fileio.h"
#include "colors.h"
#include <string.h>
#include <ctype.h>

#define MaxKeywords 256
#define MaxFiles 20
#define File FileStack[filedepth]

/*
Tiff is a straight C console application that implements a "minimal" Forth.
“Tiff” is short for “Tiffany”, the main character in the film “Bride of Chucky”.
The C code is a slasher horror show.
*/

int StackSpace = StackSpaceDefault;

char name[MaxTIBsize+1];                // generic scratchpad (maybe not such a great idea making it global)
char name2[MaxTIBsize+1];

int tiffIOR = 0;                        // Interpret error detection when not 0
static int ShowCPU = 0;                 // Enable CPU status display

// Version of getline that converts tabs to spaces upon reading is defined here.
// Well, not really getline. getline is a nice use case for malloc, but let's not.
// And why not omit non-control chars while we're at it?
// We'll just indicate an error if full, you're welcome.

int GetLine(char *line, int length, FILE *stream) {
    if (stream == NULL) {
        return -1;
    }
    int pos = 0;
    while (1) {
        int c = fgetc(stream);
        switch (c) {
            case EOF:      // should never be before a \n
                return -1;
            case '\n':
                line[pos] = '\0';
                if (pos == length-1) {
                    tiffIOR = -62;
                }
                return pos;
            case '\t':
                c = 4 - (pos & 3);    // tab=4
                while ((c--) && (pos < length-1)) {
                    line[pos++] = ' ';
                }
                break;
            default:
                if ((c >= ' ') && (pos < length-1)) {
                    line[pos++] = c;
                }
                break;
        }
    }
}

void UnCase(char *s){
    uint8_t len = strlen(s);
    while (len--) {
        *s = toupper(*s);
        s++;
    }
}

////////////////////////////////////////////////////////////////////////////////
/// tiffPARSE gets the next <delimiter> delimited string from TIBB.
/// tiffPARSENAME gets the next blank delimited token.
/// Both leave their parameters on the stack for compatibility with the Forth
/// versions that will replace them.

static uint32_t TIBhere (void) {        // get the current TIB address
    return (FetchCell(TIBB) + FetchCell(TOIN));
}
static uint8_t TIBchar (void) {         // get the current TIB byte
    uint32_t tibb = FetchCell(TIBB);
    uint32_t toin = FetchCell(TOIN);
    return FetchByte(tibb + toin);
}
static int TIBnotExhausted (void) {     // test for input not exhausted
    uint32_t tibs = FetchCell(TIBS);
    uint32_t toin = FetchCell(TOIN);
    if (TIBchar()==0) return 0;         // terminator found early
    if (toin < tibs) return 1;
    return 0;
}
static void TOINbump (void) {           // point to next TIB byte
    uint32_t toin = FetchCell(TOIN);
    StoreCell(toin+1, TOIN);
}
static int SkipToChar (char c){         // skip to character
    while (TIBnotExhausted()) {
        if (TIBchar() == c) return 1;   // T if found
        TOINbump();
    } return 0;
}
static void SkipWhite (void){           // skip whitespsce (blanks)
    while (TIBnotExhausted()) {
        if (TIBchar() != ' ') break;
        TOINbump();
    }
}
static int Refill(void); // forward reference
static void SkipPast(char c) {
    do {
        if (SkipToChar(c)) {            // found
            TOINbump();                 // skip the terminator
            return;
        }
    } while (Refill());
}

// Parse without skipping leading delimiter, return string on stack.
// Also return length of string for C to check.
static uint32_t iword_PARSE (void) {    // ( delimiter -- addr length )
    uint32_t length = 0;                // returns a string
    uint8_t delimiter = (uint8_t) PopNum();
    PushNum(TIBhere());
    while (TIBnotExhausted()) {
        if (TIBchar() == delimiter) {
            TOINbump();                 // point to after delimiter
            break;
        }
        TOINbump();                     // get string
        length++;
    }
    PushNum(length);
    return length;
}
// Parse skipping blanks, then parsing for the next blank.
// Also return length of string for C to check.
static uint32_t iword_PARSENAME (void) { // ( -- addr length )
    SkipWhite();
    PushNum(' ');
    return iword_PARSE();
}




/// Primordial brain keyword support for host functions.
/// These are interpreted after the word is not found in the Forth dictionary.
/// They prevent dead heads from being put into the dictionary.

static void iword_COMMENT (void) {
    StoreCell(FetchCell(TIBS), TOIN);   // ignore the rest of the TIB
}
static void iword_BYE (void) {
    tiffIOR = -99999;                   // exit Forth
    iword_COMMENT();
}
static void iword_STATEon (void) {               // ]
    StoreCell(1, STATE);
}
static void iword_STATEoff (void) {              // [
    StoreCell(0, STATE);
}
static void iword_DOT (void) {                   // pop and print
    uint32_t n = PopNum();
    printf("%d (%X) ", n, n);
}
static void iword_CaseIns (void) {                 // case insensitive
    StoreByte(0, CASESENS);
}
static void iword_CaseSensitive (void) {                // case sensitive
    StoreByte(1, CASESENS);
}

static void iword_CPUgo (void) {                 // Enter the single stepper
    ShowCPU = 1;
    vmTEST();
}
static void iword_CLS (void) {                   // clear screen
    printf("\033[2J");                  // CLS
    if (ShowCPU) printf("\033[%dH", DumpRows+2);  // cursor below CPU
}

static void iword_NOOP (void) {
}

/* static void iword_ROMstore (void) {
    uint32_t a = PopNum();
    uint32_t n = PopNum();
    StoreROM (n, a);
}*/

struct FileRec FileStack[MaxFiles];
int filedepth = 0;

// The dictionary list is assumed to be at the beginning of header space.
// "include" compiles the next node in the list

static uint32_t FilenameListHead;
static int FileID = 0;

void initFilelist (void) { /* EXPORT */
    FilenameListHead = HeadPointerOrigin;
    FileID = 0;
}

/*
| Cell | \[31:24\]                        | \[23:0\]                           |
| ---- |:---------------------------------| ----------------------------------:|
| -3   | Source File ID                   | spare byte | definition length     |
| -2   | Source Line, Low byte            | xtc, Execution token for compile   |
| -1   | Source Line, High byte           | xte, Execution token for execute   |
| 0    | # of instructions in definition  | Link                               |
| 1    | tag                              | Name Text, first 3 characters      |
| 2    | 4th name character               | Name Text, chars 5-7, etc...       |
*/
// Use Size=-1 if unknown
void CommaHeader (char *name, uint32_t xte, uint32_t xtc, int Size, int flags){
    uint16_t LineNum = FetchHalf(LINENUMBER);               /* EXPORT */
	uint8_t fileid = FetchByte(FILEID);
	uint32_t   wid = FetchCell(CURRENT);                  // CURRENT -> Wordlist
    uint32_t  link = FetchCell(wid);
	CommaH ((fileid << 24) | 0xFF0000 | (Size & 0xFFFF)); // [-3]: File ID | size
	CommaH (((LineNum & 0xFF)<<24)  +  (xtc & 0xFFFFFF)); // [-2]
	CommaH (((LineNum & 0xFF00)<<16) + (xte & 0xFFFFFF)); // [-1]
	StoreCell (FetchCell(HP), wid);
	CommaH (0xFF000000 | link);                           // [0]: spare | link
	CompString(name, (flags<<4)+7, HP);
}

static void FollowingToken (char *str, int max) {  // parse the next blank delimited string
    iword_PARSENAME();
    int length = PopNum();
    int addr = PopNum();
    if (length >= max) {
        length = max;
        tiffIOR = -19;
    }
    FetchString(str, addr, (uint8_t)length);
//    printf("\n|%s|, >IN=%d ", str, FetchCell(TOIN));
}

// [IF ] [ELSE] [THEN]

static void iword__ELSE (void) {
    int level = 1;
    while (level) {
        FollowingToken(name, MaxTIBsize);
        int length = strlen(name);
        if (length) {
            UnCase(name);
            if (!strcmp(name, "[IF]")) {
                level++;
            }
            if (!strcmp(name, "[THEN]")) {
                level--;
            }
            if (!strcmp(name, "[ELSE]") && (level==1)) {
                level--;
            }
        } else {                        // EOL
            if (!Refill()) {            // unexpected EOF
                tiffIOR = -58;
                return;
            }
        }
    }
}
static void iword__IF (void) {
    int flag = PopNum();
    if (!flag) {
        iword__ELSE();
    }
}


// WORDS primitive

/*
| Cell | \[31:24\]                        | \[23:0\]                           |
| ---- |:---------------------------------| ----------------------------------:|
| -3   | Source File ID                   | tag | definition length            |
| -2   | Source Line, Low byte            | xtc, Execution token for compile   |
| -1   | Source Line, High byte           | xte, Execution token for execute   |
| 0    | # of instructions in definition  | Link                               |
| 1    | spare                            | Name Text, first 3 characters      |
| 2    | 4th name character               | Name Text, chars 5-7, etc...       |
*/
static void PrintWordlist(uint32_t WID, char *substring, int verbosity) {
    char key[32];
    char name[32];
    char wordname[32];
    if (strlen(substring) == 0)         // zero length string same as NULL
        substring = NULL;
    if (verbosity) {
        printf("NAME             LEN    XTE    XTC FID  LINE  TAG    VALUE FLAG HEAD\n");
    }
    uint32_t xtc, xte, tag;
    do {
        uint8_t length = FetchByte(WID+4);
        FetchString(wordname, WID+5, length & 0x1F);
        if (substring) {
            if (strlen(substring) > 31) goto done;  // no words this long
            strcpy(key, substring);
            strcpy(name, wordname);
            if (!FetchByte(CASESENS)) {
                UnCase(key);
                UnCase(name);
            }
            char *s = strstr(name, key);
            if (s != NULL) goto good;
        } else {
good:
            tag = FetchCell(WID-12);
            xte = FetchCell(WID-4);
            xtc = FetchCell(WID-8);
            int color = (length>>5) & 7;
            if (!(xtc & 0xFFFFFF)) color += 8;      // immediate
            WordColor(color);
            int flags = length>>5;
            if (verbosity) {                        // long version
                uint32_t linenum = ((xte>>16) & 0xFF00) + (xtc>>24);
                printf("%-17s%3d%7X%7X",
                       wordname, FetchHalf(WID-12), xte&0xFFFFFF, xtc&0xFFFFFF);
                if (linenum == 0xFFFF)
                    printf("  --    --");
                else
                    printf("%4X%6d", tag>>24, linenum);
                printf("%5X%9X%5d %X\n",
                       (tag>>24)&0xFF, FetchCell(WID-16), flags, WID);
                ColorNormal();
            } else {
                printf("%s", wordname);
                ColorNormal();
                printf(" ");
            }
        }
        WID = FetchCell(WID) & 0xFFFFFF;
    } while (WID);
done:
    ColorNormal();
}

static void tiffWords (char *substring, int verbosity) {
    uint8_t wids = FetchByte(WIDS);
    while (wids--) {
        uint32_t wid = FetchCell(CONTEXT + wids*4);  // search the first list
        PrintWordlist(FetchCell(wid), substring, verbosity);
    }
    ColorNormal();
    printf("\n");
}


// When a file is included, the rest of the TIB is discarded.
// A new file is pushed onto the file stack

static char BOMmarker[4] = {0xEF, 0xBB, 0xBF, 0x00};

static void SwallowBOM(FILE *fp) {      // swallow leading UTF8 BOM marker
    char BOM[4];
    fgets(BOM, 4, fp);
    if (strcmp(BOM, BOMmarker)) {
        rewind(fp);                     // keep beginning of file if no BOM
    }
}

static void iword_INCLUDED (char *name) {
    StoreCell(1, SOURCEID);
    filedepth++;
    strcpy (File.FilePath, name);
    File.fp = fopen(name, "rb");
#ifdef VERBOSE
    printf("\nOpening file %s, handle 0x%X\n", name, (int)File.fp);
#endif
    File.LineNumber = 0;
    File.Line[0] = 0;
    File.FID = FileID;
	StoreByte(FileID, FILEID);
    iword_COMMENT();
    if (File.fp == NULL) {
        tiffIOR = -199;
    } else {
        uint32_t hp = FetchCell(HP);
        StoreROM(hp, FilenameListHead);
        FilenameListHead = hp;
        CommaH(0xFFFFFFFF);             // forward link
        CompString(File.FilePath, 7, HP);
        FileID++;
        if (FileID == 255) tiffIOR = -99;
        SwallowBOM(File.fp);
    }
}

static void iword_INCLUDE (void) {
    FollowingToken(name, MaxTIBsize);
    iword_INCLUDED (name);
}

static void iword_EQU (void) {
    FollowingToken(name, 32);
    CommaH(PopNum());
    CommaHeader(name, ~0, ~1, 0, 0);
}

static void iword_BUFFER (void) {       // allot space in data space
    FollowingToken(name, 32);
    uint32_t dp = FetchCell(DP);
    uint32_t bytes = PopNum();
    for (int i=0; i<bytes; i++) {       // initialize to 0 (non-standard)
        StoreByte(0, dp++);
    }
    StoreCell(dp + bytes, DP);
    CommaH(dp);
    CommaHeader(name, ~0, ~1, 0, 0);
}
static void iword_VAR (void) {
    PushNum(4);
    iword_BUFFER();
}

static void iword_NONAME (void) {
    NewGroup();
    PushNum(FetchCell(CP));
    StoreCell(1, STATE);
}

static void iword_COLON (void) {
    FollowingToken(name, 32);
    NewGroup();
    CommaHeader(name, FetchCell(CP), ~16, -1, 0xE0);
    // xtc must be multiple of 8 ----^
    StoreByte(1, COLONDEF); // there's a header to resolve
    StoreCell(1, STATE);
}

static void iword_DEFER (void) {
    FollowingToken(name, 32);
    NewGroup();
    CommaHeader(name, FetchCell(CP), ~16, 1, 0xC0);
    // xtc must be multiple of 8 ----^
    CompDefer();
}

static void iword_WORDS (void) {        // list words
    FollowingToken(name, 32);
    tiffWords (name, 0);
}
static void iword_XWORDS (void) {       // detailed word list
    FollowingToken(name, 32);
    tiffWords (name, 1);
}
static void iword_MAKE (void) {
    FollowingToken(name, 80);           // template filename
    FollowingToken(name2, 80);          // generated file
    MakeFromTemplate(name, name2);      // fileio.c
}
static void iword_SaveHexImage (void) { // ( flags <filename> -- )
    FollowingToken(name, 80);           // binary image filename
    SaveHexImage(PopNum(), name);       // fileio.c
}
static void iword_LitChar (void) {
    FollowingToken(name, 32);
    Literal(name[0]);
}
static void iword_Char (void) {
    FollowingToken(name, 32);
    PushNum((name[0]));
}

void GetQuotedString (char terminator) {
    PushNum(terminator);
    iword_PARSE();  // ( addr len ) string is on the stack
    uint32_t length = PopNum();
    if (length>MaxTIBsize) length=MaxTIBsize;   // sanity check
    uint32_t address = PopNum();
    FetchString(name, address, (uint8_t)length); // name is the string
}

static void SkipPast(char c);           // forward reference
static void iword_Paren (void) {        // (
    SkipPast(')');
}
static void iword_Brace (void) {        // {
    SkipPast('}');
}

static void iword_DotParen (void) {     // .(
    GetQuotedString(')');
    printf("%s", name);
}
static void iword_CR (void) {
    printf("\n");
}
static void iword_MonoTheme (void) {    // don't use color codes (dumb terminal)
    StoreByte(0, THEME);
}
static void iword_ColorTheme (void) {   // use VT220 color codes etc.
    StoreByte(1, THEME);
}

static void iword_CommaEString (void) { // ,\"
    GetQuotedString('"');
    CompString(name, 1, CP);
}
static void iword_CommaString (void) {  // ,"
    GetQuotedString('"');
    CompString(name, 5, CP);
}
static void CommaEQString (void) {
    GetQuotedString('"');
    CompString(name, 3, CP);            // aligned string
}
static void CommaQString (void) {
    GetQuotedString('"');
    CompString(name, 7, CP);            // aligned string, not escaped
}

static void iword_MsgString (void) {    // ."
    NoExecute();
    CompAhead();
    uint32_t cp = FetchCell(CP);
    CommaQString();
    CompThen();
    CompType(cp);
}
static void iword_CString (void) {      // C"
    if (FetchCell(STATE)) {
        CompAhead();
        uint32_t cp = FetchCell(CP);
        CommaQString();
        CompThen();
        Literal(cp);
    }
}
static void iword_SString (void) {      // S"
    if (FetchCell(STATE)) {
        CompAhead();
        uint32_t cp = FetchCell(CP);
        CommaQString();
        CompThen();
        CompCount(cp);
    }
}

static void iword_EMsgString (void) {   // .\"
    NoExecute();
    CompAhead();
    uint32_t cp = FetchCell(CP);
    CommaEQString();
    CompThen();
    CompType(cp);
}
static void iword_ECString (void) {     // C\"
    if (FetchCell(STATE)) {
        CompAhead();
        uint32_t cp = FetchCell(CP);
        CommaEQString();
        CompThen();
        Literal(cp);
    }
}
static void iword_ESString (void) {     // S\"
    if (FetchCell(STATE)) {
        CompAhead();
        uint32_t cp = FetchCell(CP);
        CommaEQString();
        CompThen();
        CompCount(cp);
    }
}

static uint32_t Htick (void) {    // ( -- )
    iword_PARSENAME();   // ( addr len )
    uint32_t ht = iword_FIND();         // search for keyword in the dictionary
    if (ht) {
        PopNum();
        PopNum();
        return ht;
    } else {
        uint8_t length = (uint8_t)PopNum();
        FetchString(name, PopNum(), length);
        if (length) tiffIOR = -13;
        else tiffIOR = -16;
    }
    return 0;
}

static void iword_HTICK (void) {        // h' = header tick, a primitive of '
    PushNum(Htick());
}
static void iword_TICK (void) {         // '
    uint32_t ht = Htick();
    PushNum(FetchCell(ht-4) & 0xFFFFFF);
}
static void iword_DEFINED (void) {      // [DEFINED]
    iword_PARSENAME();                  // ( addr len )
    uint32_t ht = iword_FIND();         // search for keyword in the dictionary
    PopNum();
    PopNum();
    if (ht) ht = -1;
    PushNum(ht);
}
static void iword_UNDEFINED (void) {    // [UNDEFINED]
    iword_DEFINED();
    uint32_t flag = PopNum();
    PushNum(~flag);
}
void xte_is (void) {                    // ( xt -- ) replace xte
    uint32_t ht = Htick();
    uint32_t xte = FetchCell(ht-4) & ~0xFFFFFF;
    WriteROM(xte | PopNum(), ht-4);     // host gets to overwrite ROM
}
static void iword_IS (void) {                    // patch ROM defer
    iword_TICK();
    uint32_t dest = PopNum();           // xt
    StoreROM(0xFC000000 + (PopNum() >> 2), dest);
}
static void iword_BracketTICK (void) {  // [']
    uint32_t ht = Htick();
    Literal(FetchCell(ht-4) & 0xFFFFFF);// xte
}
/*
static void iword_WordList (void) {
    uint32_t x = FetchCell(DP);
    PushNum(x);
    x += 4;
    StoreCell(x, DP);
    AddWordlistHead(x, NULL);
}
*/
static void iword_MACRO (void) {
    uint32_t wid = FetchCell(CURRENT);
    wid = FetchCell(wid);               // -> current definition
    StoreROM(~4, wid - 8);              // flip xtc from compile to macro
}

static void iword_CALLONLY (void) {
    uint32_t wid = FetchCell(CURRENT);
    wid = FetchCell(wid);               // -> current definition
    StoreROM(~0x80, wid + 4);           // clear the "call only" bit
}

static void iword_ANON (void) {
    uint32_t wid = FetchCell(CURRENT);
    wid = FetchCell(wid);               // -> current definition
    StoreROM(~0x40, wid + 4);           // clear the "anonymous" bit
}


static char *LocateFilename (int id) {  // get filename from FileID
    uint32_t p = HeadPointerOrigin;     // link is at the bottom of header space
    do {
        p = FetchCell(p);               // traverse to filename
    } while (id--);
    FetchString(name, p+4, 32);         // get counted string
    name[name[0]+1] = 0;
    return name+1;
}

static void iword_LOCATE (void) {       // locate source text
    uint32_t ht = Htick();
    uint8_t fileid = FetchByte(ht-9);
    uint8_t lineLo = FetchByte(ht-5);
    uint8_t lineHi = FetchByte(ht-1);
    uint16_t linenum = (lineHi<<8) + lineLo;
    int i, length;
    char *filename = LocateFilename(fileid);

    FILE *fp = fopen(filename, "rb");
    if (!fp) return;                    // can't open file
    SwallowBOM(fp);
    ColorHilight();
    printf("%s\n", filename);
    ColorNormal();

    for (i=1; i<linenum; i++) {         // skip to the definition
        length = GetLine(name, MaxTIBsize, fp);
        if (length < 0) goto ex;        // unexpected EOF
    }
    for (i=0; i<LocateLines; i++) {
        length = GetLine(name, MaxTIBsize, fp);
        if (length < 0) break;          // EOF
        printf("%-4d %s\n", linenum, name);
        linenum++;
    }
ex: fclose(fp);
}

static void iword_DASM (void) {         // disassemble range ( addr len )
    uint32_t length = PopNum();
    uint32_t addr = PopNum();
    if (length > 65535) {               // probably an erroneous length
        printf("Too long to disassemble\n");
        return;
    }
    Disassemble(addr, length);          // compile.c
}
static void iword_SEE (void) {          // disassemble word
    uint32_t ht = Htick();
    uint32_t addr =  FetchCell(ht-4) & 0xFFFFFF;
    uint8_t length = FetchHalf(ht-12);
    if (ht) Disassemble(addr, length);
}
static void iword_CPUon (void) {        // enable CPU display mode
    ShowCPU = 1;
}
static void iword_CPUoff (void) {       // disable CPU display mode
    ShowCPU = 0;
}
static void iword_RunBrk (void) {       // Set breakpoint
    iword_TICK();
    breakpoint = PopNum();
    iword_CPUon();
}
static void iword_NoDbg (void) {        // run to breakpoint
    breakpoint = -1;
    iword_CPUoff();
}

static void iword_STATS (void) {
#ifdef TRACEABLE
    static uint32_t mark;
    printf("\nClock cycles elapsed: %u, since last: %u ",
           cyclecount, cyclecount-mark);
    mark = cyclecount;
    printf("\nMaximum cycles between PAUSEs: %u ", maxRPtime);
    maxRPtime = 0;
#endif
    uint32_t cp = FetchCell(CP);
    uint32_t dp = FetchCell(DP);
    uint32_t hp = FetchCell(HP);
    printf("\nMem Sizes: ROM=%X, RAM=%X, SPI=%X", ROMsize*4, RAMsize*4, SPIflashBlocks*4096);
    printf("\nCP=%X, DP=%X/%X, HP=%X/%X", cp, dp, ROMsize*4, hp, (ROMsize+RAMsize)*4);
}

void iword_COLD(void) {
    VMpor();
    uint32_t PC = 0;
    while (1) {
        PC = VMstep(FetchCell(PC), 0) << 2;
    }
}
static void iword_SAFE(void) {
    VMpor();
    uint32_t PC = 4;
    VMstep(0, 0);   // skip 1st instruction
    while (1) {
        PC = VMstep(FetchCell(PC), 0) << 2;
    }
}

static int keywords = 0;                // # of keywords added at startup
typedef void (*VoidFn)();

struct Keyword {                        // host keywords only have
    char  name[16];                     // a name and
    VoidFn Function;                    // a C function
    uint32_t w;                         // optional data
};
struct Keyword HostWord[MaxKeywords];

static int thisKeyword;                 // index of keyword being executed

static void iword_Equate(void) {        // runtime part of equates
    uint32_t x = HostWord[thisKeyword].w;
    if (FetchCell(STATE)) Literal(x);
    else PushNum(x);
}

static void AddKeyword(char *name, void (*func)()) {
    if (keywords<MaxKeywords){          // ignore new keywords if full
        strcpy (HostWord[keywords].name, name);
        HostWord[keywords].w = 0xf00c0ded; // tag as "not an equ"
        HostWord[keywords++].Function = func;
    } else printf("Please increase MaxKeywords and rebuild.\n");
}
static void AddEquate(char *name, uint32_t value) {
    if (keywords<MaxKeywords){          // ignore new keywords if full
        strcpy (HostWord[keywords].name, name);
        HostWord[keywords].w = value;
        HostWord[keywords++].Function = iword_Equate;
    } else printf("Please increase MaxKeywords and rebuild.\n");
}

static void ListKeywords(void) {
    int i;
    for (i=0; i<keywords; i++) {
        if (HostWord[keywords].w == 0xf00c0ded) {
            ColorHilight();
        } else {
            ColorNormal();
        }
        printf("%s ", HostWord[i].name);
    }
    printf("\n");
}

static void LoadKeywords(void) {        // populate the list of gator brain functions
    keywords = 0;                       // start empty
    AddKeyword("bye",           iword_BYE);
    AddKeyword("[",             iword_STATEoff);
    AddKeyword("]",             iword_STATEon);
    AddKeyword("[if]",          iword__IF);
    AddKeyword("[then]",        iword_NOOP);
    AddKeyword("[else]",        iword__ELSE);
    AddKeyword("[undefined]",   iword_UNDEFINED);
    AddKeyword("[defined]",     iword_DEFINED);

    AddKeyword("\\",            iword_COMMENT);
    AddKeyword(".",             iword_DOT);
    AddKeyword("(",             iword_Paren);
    AddKeyword("{",             iword_Brace);
    AddKeyword(".(",            iword_DotParen);
    AddKeyword("cr",            iword_CR);
    AddKeyword("theme=mono",    iword_MonoTheme);
    AddKeyword("theme=color",   iword_ColorTheme);

    AddKeyword("stats",         iword_STATS);
    AddKeyword("cold",          iword_COLD);
    AddKeyword("safe",          iword_SAFE);
    AddKeyword(".opcodes",      ListOpcodeCounts);
    AddKeyword(".profile",      ListProfile);
    AddKeyword("+cpu",          iword_CPUon);
    AddKeyword("-cpu",          iword_CPUoff);
    AddKeyword("cpu",           iword_CPUgo);
    AddKeyword("dbg",           iword_RunBrk);  // set and run to breakpoint(s)
    AddKeyword("-dbg",          iword_NoDbg);
    AddKeyword("cls",           iword_CLS);
    AddKeyword("CaseSensitive", iword_CaseSensitive);
    AddKeyword("CaseInsensitive", iword_CaseIns);
    AddKeyword("include",       iword_INCLUDE);
    AddKeyword("equ",           iword_EQU);
    AddKeyword("constant",      iword_EQU);     // non-tickable constant
    AddKeyword("variable",      iword_VAR);     // non-tickable variable

    AddKeyword("words",         iword_WORDS);
    AddKeyword("xwords",        iword_XWORDS);
    AddKeyword("buffer:",       iword_BUFFER);
    AddKeyword(":noname",       iword_NONAME);
    AddKeyword(":",             iword_COLON);
    AddKeyword(";",             CompSemi);
    AddKeyword(",",             CompComma);
    AddKeyword("c,",            CompCommaC);
    AddKeyword("literal",       CompLiteral);
    AddKeyword(",\"",           iword_CommaString);
    AddKeyword(",\\\"",         iword_CommaEString);
    AddKeyword(".\"",           iword_MsgString);
    AddKeyword(".\\\"",         iword_EMsgString);
    AddKeyword("s\"",           iword_SString);
    AddKeyword("s\\\"",         iword_ESString);
    AddKeyword("c\"",           iword_CString);
    AddKeyword("c\\\"",         iword_ECString);

    AddKeyword("[char]",        iword_LitChar);
    AddKeyword("char",          iword_Char);
    AddKeyword("exit",          CompExit);
    AddKeyword("defer",         iword_DEFER);
    AddKeyword("is",            iword_IS);
    AddKeyword("+if",           CompPlusIf);
    AddKeyword("ifnc",          CompIfNC);
    AddKeyword("if",            CompIf);
    AddKeyword("else",          CompElse);
    AddKeyword("then",          CompThen);
    AddKeyword("begin",         CompBegin);
    AddKeyword("again",         CompAgain);
    AddKeyword("until",         CompUntil);
    AddKeyword("+until",        CompPlusUntil);
    AddKeyword("while",         CompWhile);
    AddKeyword("+while",        CompPlusWhile);
    AddKeyword("repeat",        CompRepeat);
    AddKeyword("?do",           CompQDo);
    AddKeyword("do",            CompDo);
    AddKeyword("loop",          CompLoop);
    AddKeyword("i",             CompI);
    AddKeyword("leave",         CompLeave);

    AddKeyword("h'",            iword_HTICK);
    AddKeyword("'",             iword_TICK);
    AddKeyword("[']",           iword_BracketTICK);

    AddKeyword("macro",         iword_MACRO);
    AddKeyword("call-only",     iword_CALLONLY);
    AddKeyword("anonymous",     iword_ANON);
    AddKeyword("see",           iword_SEE);
    AddKeyword("dasm",          iword_DASM);
    AddKeyword("locate",        iword_LOCATE);
    AddKeyword("replace-xt",    ReplaceXTs);    // Replace XTs  ( NewXT OldXT -- )
    AddKeyword("xte-is",        xte_is);        // Replace a word's xte  ( NewXT -- )
    AddKeyword("make",          iword_MAKE);
    AddKeyword("save-hex",      iword_SaveHexImage);

    AddKeyword("iwords",        ListKeywords);  // internal words, after the dictionary

    AddEquate ("RAMsize",       RAMsize*4);
    AddEquate ("ROMsize",       ROMsize*4);
    AddEquate ("SPIflashBlocks", SPIflashBlocks);

    // CPU opcode names
    AddEquate ("op_dup",   opDUP);
    AddEquate ("op_exit",  opEXIT);
    AddEquate ("op_+",     opADD);
    AddEquate ("op_user",  opUSER);
    AddEquate ("op_drop",  opDROP);
    AddEquate ("op_r>",    opPOP);
    AddEquate ("op_2/",    opTwoDiv);
    AddEquate ("op_1+",    opOnePlus);
    AddEquate ("op_swap",  opSWAP);
    AddEquate ("op_c!+",   opCstorePlus);
    AddEquate ("op_c@+",   opCfetchPlus);
    AddEquate ("op_u2/",   opUtwoDiv);
    AddEquate ("op_no:",   opSKIP);
    AddEquate ("op_jmp",   opJUMP);
    AddEquate ("op_w!+",   opWstorePlus);
    AddEquate ("op_w@+",   opWfetchPlus);
    AddEquate ("op_and",   opAND);
    AddEquate ("op_litx",  opLitX);
    AddEquate ("op_>r",    opPUSH);
    AddEquate ("op_call",  opCALL);
    AddEquate ("op_0=",    opZeroEquals);
    AddEquate ("op_0<",    opZeroLess);
    AddEquate ("op_w@",    opWfetch);
    AddEquate ("op_xor",   opXOR);
    AddEquate ("op_reptc", opREPTC);
    AddEquate ("op_4+",    opFourPlus);
    AddEquate ("op_over",  opOVER);
    AddEquate ("op_+",     opADDC);
    AddEquate ("op_!+",    opStorePlus);
    AddEquate ("op_@+",    opFetchPlus);
    AddEquate ("op_2*",    opTwoStar);
    AddEquate ("op_-rept", opMiREPT);
    AddEquate ("op_rp",    opRP);
    AddEquate ("op_rp!",   opSetRP);
    AddEquate ("op_@",     opFetch);
    AddEquate ("op_-if:",  opSKIPGE);
    AddEquate ("op_sp",    opSP);
    AddEquate ("op_@as",   opFetchAS);
    AddEquate ("op_sp!",   opSetSP);
    AddEquate ("op_c@",    opCfetch);
    AddEquate ("op_port",  opPORT);
    AddEquate ("op_ifc:",  opSKIPNC);
    AddEquate ("op_ifz:",  opSKIPNZ);
    AddEquate ("op_lit",   opLIT);
    AddEquate ("op_up",    opUP);
    AddEquate ("op_!as",   opStoreAS);
    AddEquate ("op_up!",   opSetUP);
    AddEquate ("op_r@",    opRfetch);
    AddEquate ("op_com",   opCOM);
    // Initial Value names
    AddEquate ("sp00", TiffSP0);           // empty stacks for terminal
    AddEquate ("rp00", TiffRP0);
    AddEquate ("hp0", HeadPointerOrigin);  // bottom of head space
    // Variable names
    AddEquate ("handler",    HANDLER);     // tiff.h terminal task variable names
    AddEquate ("base",       BASE);
    AddEquate ("hp",         HP);
    AddEquate ("cp",         CP);
    AddEquate ("dp",         DP);
    AddEquate ("state",      STATE);
    AddEquate ("current",    CURRENT);
    AddEquate ("source-id",  SOURCEID);
    AddEquate ("personality",PERSONALITY);
    AddEquate ("tibs",       TIBS);
    AddEquate ("tibb",       TIBB);
    AddEquate (">in",        TOIN);
    AddEquate ("w_linenum",  LINENUMBER);
    AddEquate ("c_fileid",   FILEID);
    AddEquate ("c_noecho",   NOECHO);
    AddEquate ("c_scope",    SCOPE);
    AddEquate ("c_wids",     WIDS);
    AddEquate ("c_called",   CALLED);
    AddEquate ("c_slot",     SLOT);
    AddEquate ("c_litpend",  LITPEND);
    AddEquate ("c_colondef", COLONDEF);
    AddEquate ("c_casesens", CASESENS);
    AddEquate ("c_theme",    THEME);
    AddEquate ("calladdr",   CALLADDR);
    AddEquate ("nextlit",    NEXTLIT);
    AddEquate ("iracc",      IRACC);
    AddEquate ("context",    CONTEXT);
    AddEquate ("forth-wordlist", FORTHWID);
    AddEquate ("tib",        TIB);
    AddEquate ("|tib|",      MaxTIBsize);
    AddEquate ("head",       HEAD);
    AddEquate ("hld",        HLD);
    AddEquate ("blk",        BLK);
    AddEquate ("pad",        PAD);
    AddEquate ("|pad|",      PADsize);
}

int NotKeyword (char *key) {            // do a command, return 0 if found
    int i = 0;
    char str1[16], str2[16];
    if (strlen(key) < 16) {
        for (i = 0; i < keywords; i++) { // scan the list for the word name
            strcpy(str1, key);
            strcpy(str2, HostWord[i].name);
            if (!FetchByte(CASESENS)) {
                UnCase(str1);
                UnCase(str2);
            }
            if (strcmp(str1, str2) == 0) {
                thisKeyword = i;
                HostWord[i].Function(); // execute it
                return 0;
            }
        }
    }
    return -1;
}

// Interpret the TIB inside the VM, on the VM data stack.
static void iword_INTERPRET(void) {
    while (iword_PARSENAME()) {         // get the next blank delimited keyword
#ifdef VERBOSE
        uint32_t tempLen = PopNum();
        uint32_t tempAddr = PopNum();
        FetchString(name, tempAddr, tempLen);
        printf("\nFind:|%s| at >IN=%X, STATE=%d ", name, FetchCell(TOIN), FetchCell(STATE));
        PushNum(tempAddr);
        PushNum(tempLen);
#endif
        uint32_t ht = iword_FIND();     // search for keyword in the Forth dictionary
        if (ht) {  // ( 0 ht )          // it's a Forth (in ROM) word
            PopNum();
            PopNum();
            uint32_t xte = 0xFFFFFF & FetchCell(ht - 4);
            uint32_t xtc = 0xFFFFFF & FetchCell(ht - 8);
            if (FetchCell(STATE)) {     // compiling
                if (xtc) {
                    tiffFUNC(xtc);
                } else {
                    tiffFUNC(xte);      // xtc was wiped by IMMEDIATE
                }
            } else {                    // interpreting
                tiffFUNC(xte);
            }
        } else {                        // okay, maybe an internal keyword
            uint32_t length = PopNum();
            if (length>31) length=32;   // sanity check
            uint32_t address = PopNum();
            FetchString(name, address, (uint8_t)length);
            if (NotKeyword(name)) {     // try to convert to number
                char *eptr;             // long long to handle 80000000 to FFFFFFFF
                long long int x = strtoll(name, &eptr, FetchCell(BASE));
                if ((x == 0) && ((errno == EINVAL) || (errno == ERANGE))) {
                    bogus: tiffIOR = -13;   // not a number
                } else {
                    if (*eptr) goto bogus;  // points at zero terminator if number
                    if (FetchCell(STATE)) {
#ifdef VERBOSE
                        printf(", Literal %d\n", (int32_t)x);
#endif
                        Literal((uint32_t)x);
                    } else {
#ifdef VERBOSE
                        printf(", Push %d\n", (int32_t)x);
#endif
                        PushNum((uint32_t)x);
                    }
                }
            }
#ifdef VERBOSE
            printf(" <<< Local Keyword\n");
#endif
        }
        if (Sdepth()<0) {
            tiffIOR = -4;
        }
        if (Rdepth()<0) {
            tiffIOR = -6;
        }
        if (tiffIOR) goto ex;
    }
ex: PopNum();                           // keyword is an empty string
    PopNum();
}

static int Refill(void) {
    int TIBaddr = FetchCell(TIBB);
    StoreCell(0, TOIN);
    int length = GetLine(File.Line, MaxTIBsize, File.fp);
    if (length < 0) {                   // EOF, un-nest
#ifdef VERBOSE
        printf("Closing file handle 0x%X\n", (int)File.fp);
#endif
        fclose (File.fp);
        filedepth--;
        if (filedepth == 0) {
            StoreCell(0, SOURCEID);
        }
        StoreByte(File.FID, FILEID);
        StoreHalf(File.LineNumber, LINENUMBER);
        iword_COMMENT();
        return 0;
    }
    if (length >= MaxTIBsize) tiffIOR = -62;
    while ((length > 0) && (File.Line[length-1] < ' ')) {
        length--;	// trim trailing lf or crlf
    }
    File.Line[length] = 0;
    File.LineNumber++;
    StoreHalf(File.LineNumber, LINENUMBER);
    StoreCell((uint32_t)length, TIBS);
    StoreString(File.Line, (uint32_t)TIBaddr);
    return 1;
}

// Keyboard input uses the default terminal cooked mode.

char *DefaultFile = "mf.f";             // Default file to load from

void tiffQUIT (char *cmdline) {
    int loaded = 0;
    int length, source, TIBaddr;
    LoadKeywords();
    StoreCell(STATUS, FOLLOWER);  	    // only terminal task
    while (1){
        tiffIOR = 0;
        InitializeTIB();
        StoreCell(0, SOURCEID);     	// input is keyboard
        StoreCell(0, STATE);     	    // interpret
        while (1) {
            if (ShowCPU) {
                ColorNone();
                DumpRegs();
            }
            StoreCell(0, TOIN);		    // >IN = 0
            StoreCell(0, TIBS);
            source = FetchCell(SOURCEID);
            TIBaddr = FetchCell(TIBB);

#if (OKstyle)
            int depth;
#endif
            switch (source) {
                case 0:                 // load TIBB from keyboard
                    if (!loaded) {
#ifdef VERBOSE
                        printf("%d\nAttempting to include file %s\n", tiffIOR, DefaultFile);
#endif
                        FILE *test = fopen(DefaultFile, "rb");
                        if (test != NULL) {
                            fclose(test);           // if default file exists
                            iword_INCLUDED(DefaultFile);
                        }
                        loaded = 1;
                        break;
                    }
                    if (cmdline) {      // first time through use command line
                        strcpy (File.Line, cmdline);
                        length = strlen(cmdline);
                        if (length >= MaxTIBsize) tiffIOR = -62;
                        cmdline = NULL; // clear cmdline
                    } else {
#ifdef __linux__
                        CookedMode();
#endif // __linux__
                        length = GetLine(File.Line, MaxTIBsize, stdin);   // get input line
                        File.Line[length] = 0;
                    }
                    StoreCell((uint32_t)length, TIBS);
                    StoreString(File.Line, (uint32_t)TIBaddr);
                    ColorNormal();
                    break;
                case 1:
                    Refill();
                    break;
                default:	            // unexpected
                    StoreCell(0, SOURCEID);
            }
#ifdef VERBOSE
            FetchString(name, FetchCell(TIBB), (uint8_t)FetchCell(TIBS));
            printf("ior before Interpret of |%s| is %d\n", name, tiffIOR);
#endif
            iword_INTERPRET();          // interpret the TIBB until it's exhausted
            if (tiffIOR) break;
/*
Here's an interesting problem. stdin cooked input echo includes the trailing newline.
This newline means that output will appear on a new line.
It's a slightly different look and feel from some Forths.

ok> at the newline means you didn't die. That's always `ok`.
*/

            switch (FetchCell(SOURCEID)) {
                case 0:
#if (OKstyle)
                    depth = Sdepth();
                    if (depth) {
                        printf("%d|ok>", depth);
                    } else {
                        printf("ok>");
                    }
#else
                    printf("ok>");
#endif
                    ColorNone();
                    break;
                default:
                    break;
            }
        }
        if (tiffIOR == -99999) return;  // produced by BYE
        ColorError();
        ErrorMessage(tiffIOR, name);
        while (filedepth) {
            ColorFilePath();
            printf("%s[%d]: ", File.FilePath, File.LineNumber);
            ColorFileLine();
            printf("%s\n", File.Line);
            fclose(File.fp);
            filedepth--;
        }
        ColorNormal();
    }
}

