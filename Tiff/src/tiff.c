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

char name[MaxTIBsize+1];                // generic scratchpad (maybe not such a great idea making it global)
char name2[MaxTIBsize+1];

int tiffIOR = 0;                        // Interpret error detection when not 0
int ShowCPU = 0;                        // Enable CPU status display
int printed = 0;                        // flag, T if text was printed on console

// Version of getline that converts tabs to spaces upon reading is defined here
// getline is POSIX anyway, not necessarily in your C. Acknowledgment:
// https://stackoverflow.com/questions/735126/are-there-alternate-implementations-of-gnu-getline-interface/735472#735472
// if typedef doesn't exist (msvc, blah)
typedef intptr_t ssize_t;
ssize_t GetLine(char **lineptr, size_t *n, FILE *stream) {
    size_t pos;
    int c;

    if (lineptr == NULL || stream == NULL || n == NULL) {
        errno = EINVAL;
        return -1;
    }

    c = fgetc(stream);
    if (c == EOF) {
        return -1;
    }

    if (*lineptr == NULL) {
        *lineptr = malloc(128);
        if (*lineptr == NULL) {
            return -1;
        }
        *n = 128;
    }

    pos = 0;
    while(c != EOF) {
        if (pos + 1 >= *n) {
            size_t new_size = *n + (*n >> 2);
            if (new_size < 128) {
                new_size = 128;
            }
            char *new_ptr = realloc(*lineptr, new_size);
            if (new_ptr == NULL) {
                return -1;
            }
            *n = new_size;
            *lineptr = new_ptr;
        }
        if (c == '\t') {
            int padding = 4 - (pos & 3);    // tab=4
            while (padding--) {
                ((unsigned char *)(*lineptr))[pos ++] = ' ';
            }
        } else {
            ((unsigned char *)(*lineptr))[pos ++] = c;
        }
        if (c == '\n') {
            break;
        }
        c = fgetc(stream);
    }

    (*lineptr)[pos] = '\0';
    return pos;
}

void UnCase(char *s){
    uint8_t len = strlen(s);
    while (len--) {
        *s = toupper(*s);
        s++;
    }
}


/// Primordial brain keyword support for host functions.
/// These are interpreted after the word is not found in the Forth dictionary.
/// They prevent dead heads from being put into the dictionary.

void tiffCOMMENT (void) {
    StoreCell(FetchCell(TIBS), TOIN);   // ignore the rest of the TIB
}
void tiffBYE (void) {
    tiffIOR = -99999;                   // exit Forth
    tiffCOMMENT();
}
void tiffSTATEon (void) {               // ]
    StoreCell(1, STATE);
}
void tiffSTATEoff (void) {              // [
    StoreCell(0, STATE);
}
void tiffDOT (void) {                   // pop and print
    uint32_t n = PopNum();
    printf("%d (%X) ", n, n);  printed = 1;
}
void tiffCaseIns (void) {                 // case insensitive
    StoreByte(0, CASESENS);
}
void tiffCaseSensitive (void) {                // case sensitive
    StoreByte(1, CASESENS);
}

void tiffCPUgo (void) {                 // Enter the single stepper
    ShowCPU = 1;
    vmTEST();
}
void tiffCLS (void) {                   // clear screen
    printf("\033[2J");                  // CLS
    if (ShowCPU) printf("\033[%dH", DumpRows+2);  // cursor below CPU
}

void tiffNOOP (void) {
}

/* void tiffROMstore (void) {
    uint32_t a = PopNum();
    uint32_t n = PopNum();
    StoreROM (n, a);
}*/

struct FileRec FileStack[MaxFiles];
int filedepth = 0;

// The dictionary list is assumed to be at the beginning of header space.
// "include" compiles the next node in the list

uint32_t FilenameListHead = HeadPointerOrigin;
int FileID = 0;

void initFilelist (void) {
    FilenameListHead = HeadPointerOrigin;
    FileID = 0;
}

/*
| Cell | \[31:24\]                        | \[23:0\]                           |
| ---- |:---------------------------------| ----------------------------------:|
| -3   | Source File ID                   | List of words that reference this  |
| -2   | Source Line, Low byte            | xtc, Execution token for compile   |
| -1   | Source Line, High byte           | xte, Execution token for execute   |
| 0    | # of instructions in definition  | Link                               |
| 1    | Name Length                      | Name Text, first 3 characters      |
| 2    | 4th name character               | Name Text, chars 5-7, etc...       |
*/
// Use Size=-1 if unknown
void CommaHeader (char *name, uint32_t xte, uint32_t xtc, int Size, int flags){
    uint16_t LineNum = FetchHalf(LINENUMBER);
	uint8_t fileid = FetchByte(FILEID);
	uint32_t   wid = FetchCell(CURRENT);                  // CURRENT -> Wordlist
    uint32_t  link = FetchCell(wid);
//    /* leave this field blank, not needed. Maybe where field in the future.
    if (link) {
        StoreROM(0xFF000000 + FetchCell(HP), link - 12);  // resolve forward link
	}
//	*/
	CommaH ((fileid << 24) | 0xFFFFFF);                   // [-3]: File ID | where
	CommaH (((LineNum & 0xFF)<<24)  +  (xtc & 0xFFFFFF)); // [-2]
	CommaH (((LineNum & 0xFF00)<<16) + (xte & 0xFFFFFF)); // [-1]
	StoreCell (FetchCell(HP), wid);
	CommaH (((Size&0xFF)<<24) | link);                 // [0]: Upper size | link
	CompString(name, (flags<<4)+7, HP);
}

void FollowingToken (char *str, int max) {  // parse the next blank delimited string
    tiffPARSENAME();
    int length = PopNum();
    int addr = PopNum();
    if (length >= max) {
        length = max;
        tiffIOR = -19;
    }
    FetchString(str, addr, (uint8_t)length);
//    printf("\n|%s|, >IN=%d ", str, FetchCell(TOIN));
}

void tiff_ELSE (void) {
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
        } else {                    // EOL
            if (!Refill()) {        // unexpected EOF
                tiffIOR = -58;
                return;
            }
        }
    }
}

void tiff_IF (void) {
    int flag = PopNum();
    if (!flag) {
        tiff_ELSE();
    }
}


// When a file is included, the rest of the TIB is discarded.
// A new file is pushed onto the file stack

static char BOMmarker[4] = {0xEF, 0xBB, 0xBF, 0x00};

void SwallowBOM(FILE *fp) {             // swallow leading UTF8 BOM marker
    char BOM[4];
    fgets(BOM, 4, fp);
    if (strcmp(BOM, BOMmarker)) {
        rewind(fp);                     // keep beginning of file if no BOM
    }
}

void tiffINCLUDED (char *name) {
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
    tiffCOMMENT();
    if (File.fp == NULL) {
        tiffIOR = -199;
    } else {
        uint32_t hp = FetchCell(HP);
        StoreROM(hp, FilenameListHead);
        FilenameListHead = hp;
        CommaH(0xFFFFFFFF);             // forward link
        CompString(File.FilePath, 3, HP);
        FileID++;
        if (FileID == 255) tiffIOR = -99;
        SwallowBOM(File.fp);
    }
}

void tiffINCLUDE (void) {
    FollowingToken(name, MaxTIBsize);
    tiffINCLUDED (name);
}

void tiffEQU (void) {
    FollowingToken(name, 32);
    CommaH(PopNum());
    CommaHeader(name, ~0, ~1, 0, 0);
}

void tiffBUFFER (void) {                // allot space in data space
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
void tiffVAR (void) {
    PushNum(4);
    tiffBUFFER();
}

void tiffNONAME (void) {
    NewGroup();
    PushNum(FetchCell(CP));
    StoreCell(1, STATE);
}

void tiffCOLON (void) {
    FollowingToken(name, 32);
    NewGroup();
    CommaHeader(name, FetchCell(CP), ~16, -1, 0xE0);
    // xtc must be multiple of 8 ----^
    StoreByte(1, COLONDEF); // there's a header to resolve
    StoreCell(1, STATE);
}

void tiffDEFER (void) {
    FollowingToken(name, 32);
    NewGroup();
    CommaHeader(name, FetchCell(CP), ~16, 1, 0xC0);
    // xtc must be multiple of 8 ----^
    CompDefer();
}

void tiffWORDS (void) {                 // list words
    FollowingToken(name, 32);
    tiffWords (name, 0);
}

void tiffXWORDS (void) {                // detailed word list
    FollowingToken(name, 32);
    tiffWords (name, 1);
}
void tiffMAKE (void) {
    FollowingToken(name, 80);           // template filename
    FollowingToken(name2, 80);          // generated file
    MakeFromTemplate(name, name2);      // fileio.c
}
void tiffSaveImg (void) {
    FollowingToken(name, 80);           // binary image filename
    SaveImage(name);                    // fileio.c
}

static void CommaROM (uint32_t x) {
    PushNum(x);
    CompComma();
}
void tiffIDATA (void) {                 // compile RAM initialization table
    WipeTIB();
    uint32_t first = STATUS / 4;        // cell indices
    uint32_t last = FetchCell(DP) / 4;
    uint32_t length = last - first;

    uint32_t *mem = (uint32_t*)malloc(RAMsize*sizeof(uint32_t));
    for (int i=0; i<length; i++) {      // read RAM into temporary buffer
        mem[i] = FetchCell((i+first)*4);
    }
    uint32_t p = 0;                     // format: {length address [data]} ... 0
    uint32_t mark = 0;
    while (p < length) {
        while ((p < length) && (!mem[p])) {
            p++;                        // skip zeros
        }
        mark = p;                       // index of first non-zero value
        while ((p<length)&&(mem[p++])); // find runs of nonzero
        CommaROM(p-mark);               // length of run
        if (p != mark) {
            CommaROM((first+mark)*4);   // start address
        }
        for (int i=mark; i<p; i++) {
            CommaROM(mem[i]);           // data...
        }
    }
    if (mark != p) {
        CommaROM(0);                    // terminate table
    }
    free(mem);
}
void TiffLitChar (void) {
    FollowingToken(name, 32);
    Literal(name[0]);
}
void TiffChar (void) {
    FollowingToken(name, 32);
    PushNum((name[0]));
}

void GetQuotedString (char terminator) {
    PushNum(terminator);
    tiffPARSE();  // ( addr len ) string is on the stack
    uint32_t length = PopNum();
    if (length>MaxTIBsize) length=MaxTIBsize;   // sanity check
    uint32_t address = PopNum();
    FetchString(name, address, (uint8_t)length); // name is the string
}

void SkipPast(char c); // forward reference
void TiffParen (void) {
    SkipPast(')');
}
void TiffBrace (void) {
    SkipPast('}');
}

void TiffDotParen (void) {
    GetQuotedString(')');
    printf("%s", name);
}
void TiffCR (void) {
    printf("\n");
}
void TiffMonoTheme (void) {
    StoreByte(0, THEME);
}
void TiffColorTheme (void) {
    StoreByte(1, THEME);
}

void tiffCommaString (void) {   // ,"
    GetQuotedString('"');
    CompString(name, 1, CP);
}
static void CommaQString (void) {
    GetQuotedString('"');
    CompString(name, 3, CP);       // aligned string
}

void tiffMsgString (void) {     // ."
    NoExecute();
    CompAhead();
    uint32_t cp = FetchCell(CP);
    CommaQString();
    CompThen();
    CompType(cp);
}
void tiffCString (void) {       // C"
    if (FetchCell(STATE)) {
        CompAhead();
        uint32_t cp = FetchCell(CP);
        CommaQString();
        CompThen();
        Literal(cp);
    }
}
void tiffSString (void) {       // S"
    if (FetchCell(STATE)) {
        CompAhead();
        uint32_t cp = FetchCell(CP);
        CommaQString();
        CompThen();
        CompCount(cp);
    }
}

uint32_t Htick (void) {  // ( -- )
    tiffPARSENAME();  // ( addr len )
    uint32_t ht = tiffFIND();           // search for keyword in the dictionary
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

void tiffHTICK (void) {                 // h' = header tick, a primitive of '
    PushNum(Htick());
}
void tiffTICK (void) {                  // '
    uint32_t ht = Htick();
    PushNum(FetchCell(ht-4) & 0xFFFFFF);
}
void xte_is (void) {                    // ( xt -- ) replace xte
    uint32_t ht = Htick();
    uint32_t xte = FetchCell(ht-4) & ~0xFFFFFF;
    WriteROM(xte | PopNum(), ht-4);     // host gets to overwrite ROM
}
void tiffIS (void) {                    // patch ROM defer
    tiffTICK();
    uint32_t dest = PopNum();           // xt
    StoreROM(0xFC000000 + (PopNum() >> 2), dest);
}
void tiffBracketTICK (void) {           // [']
    uint32_t ht = Htick();
    Literal(FetchCell(ht-4) & 0xFFFFFF);// xte
}
void tiffWordList (void) {
    uint32_t x = FetchCell(DP);
    PushNum(x);
    x += 4;
    StoreCell(x, DP);
    AddWordlistHead(x, NULL);
}

char *LocateFilename (int id) {         // get filename from FileID
    uint32_t p = HeadPointerOrigin;     // link is at the bottom of header space
    do {
        p = FetchCell(p);               // traverse to filename
    } while (id--);
    FetchString(name, p+4, 32);         // get counted string
    name[name[0]+1] = 0;
    return name+1;
}

void tiffLOCATE (void) {                // locate source text
    size_t bufsize = MaxTIBsize;        // for GetLine
    char *buf;
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
    buf = name;                         // reuse the name buffer for text line

    for (i=1; i<linenum; i++) {         // skip to the definition
        length = GetLine(&buf, &bufsize, fp);
        if (length < 0) goto ex;        // unexpected EOF
    }
    for (i=0; i<LocateLines; i++) {
        length = GetLine(&buf, &bufsize, fp);
        if (length < 0) break;          // EOF
        printf("%-4d %s", linenum, buf);
        linenum++;
    }
ex: fclose(fp);

}

void tiffDASM (void) {                  // disassemble range ( addr len )
    uint32_t length = PopNum();
    uint32_t addr = PopNum();
    if (length > 65535) {               // probably an erroneous length
        printf("\nToo long to disassemble");
        printed = 1;
        return;
    }
    Disassemble(addr, length);
}
void tiffSEE (void) {                   // disassemble word
    uint32_t ht = Htick();
    uint32_t addr =  FetchCell(ht-4) & 0xFFFFFF;
    uint8_t length = FetchByte(ht+3);
    if (ht) Disassemble(addr, length);
}
void tiffCPUon (void) {                 // enable CPU display mode
    ShowCPU = 1;
}
void tiffCPUoff (void) {                // disable CPU display mode
    ShowCPU = 0;
}
void tiffRunBrk (void) {                // run to breakpoint
    tiffTICK();
    breakpoint = PopNum();
    tiffFUNC(breakpoint);               // Execute breaks immediately
    tiffCPUon();
}
void tiffNoDbg (void) {                // run to breakpoint
    breakpoint = -1;
    tiffCPUoff();
}

void tiffSTATS (void) {
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
    printf("\nMem Sizes: ROM=%X, RAM=%X, SPI=%X", ROMsize*4, RAMsize*4, SPIflashSize*4);
    printf("\nCP=%X, DP=%X/%X, HP=%X/%X", cp, dp, ROMsize*4, hp, (ROMsize+RAMsize)*4);
    printed = 1;
}

void tiffForthWID (void) {
    uint32_t p = HeadPointerOrigin+4;
    PushNum(p);
}


int keywords = 0;                       // # of keywords added at startup
typedef void (*VoidFn)();

struct Keyword {                        // host keywords only have
    char  name[16];                     // a name and
    VoidFn Function;                    // a C function
    uint32_t w;                         // optional data
};
struct Keyword HostWord[MaxKeywords];

int thisKeyword;                        // index of keyword being executed

void tiffEquate(void) {                 // runtime part of equates
    uint32_t x = HostWord[thisKeyword].w;
    if (FetchCell(STATE)) Literal(x);
    else PushNum(x);
}

void AddKeyword(char *name, void (*func)()) {
    if (keywords<MaxKeywords){          // ignore new keywords if full
        strcpy (HostWord[keywords].name, name);
        HostWord[keywords].w = 0xf00c0ded; // tag as "not an equ"
        HostWord[keywords++].Function = func;
    } else printf("\nPlease increase MaxKeywords and rebuild.");
}
void AddEquate(char *name, uint32_t value) {
    if (keywords<MaxKeywords){          // ignore new keywords if full
        strcpy (HostWord[keywords].name, name);
        HostWord[keywords].w = value;
        HostWord[keywords++].Function = tiffEquate;
    } else printf("\nPlease increase MaxKeywords and rebuild.");
}

void ListKeywords(void) {
    int i;
    for (i=0; i<keywords; i++) {
        if (HostWord[keywords].w == 0xf00c0ded) {
            ColorHilight();
        } else {
            ColorNormal();
        }
        printf("%s ", HostWord[i].name);
    }   printed = 1;
}

void tiffWordlistHead(void) {
    PushNum(WordlistHead());
}

void LoadKeywords(void) {               // populate the list of gator brain functions
    keywords = 0;                       // start empty
    AddKeyword("bye",     tiffBYE);
    AddKeyword("[",       tiffSTATEoff);
    AddKeyword("]",       tiffSTATEon);
    AddKeyword("[if]",    tiff_IF);
    AddKeyword("[then]",  tiffNOOP);
    AddKeyword("[else]",  tiff_ELSE);

    AddKeyword("\\",      tiffCOMMENT);
    AddKeyword("//",      tiffCOMMENT); // too much cog dis switching between C and Forth
    AddKeyword(".",       tiffDOT);
    AddKeyword("(",       TiffParen);
    AddKeyword("{",       TiffBrace);
    AddKeyword(".(",      TiffDotParen);
    AddKeyword("cr",      TiffCR);
    AddKeyword("theme=mono",  TiffMonoTheme);
    AddKeyword("theme=color", TiffColorTheme);

    AddKeyword("stats",   tiffSTATS);
    AddKeyword(".opcodes", ListOpcodeCounts);
    AddKeyword(".profile", ListProfile);
    AddKeyword("+cpu",    tiffCPUon);
    AddKeyword("-cpu",    tiffCPUoff);
    AddKeyword("cpu",     tiffCPUgo);
    AddKeyword("dbg",     tiffRunBrk);  // set and run to breakpoint(s)
    AddKeyword("-dbg",    tiffNoDbg);
    AddKeyword("cls",     tiffCLS);
    AddKeyword("CaseSensitive",   tiffCaseSensitive);
    AddKeyword("CaseInsensitive", tiffCaseIns);
    AddKeyword("include", tiffINCLUDE);
    AddKeyword("equ",     tiffEQU);
    AddKeyword("constant",tiffEQU);     // non-tickable constant
    AddKeyword("variable",tiffVAR);     // non-tickable variable

/*    AddKeyword("create",  tiffCREATE);
    AddKeyword("ram",     tiffRAM);
    AddKeyword("rom",     tiffROM);
    AddKeyword("here",    tiffHERE);
    AddKeyword("allot",   tiffALLOT); */

    AddKeyword("words",   tiffWORDS);
    AddKeyword("xwords",  tiffXWORDS);
    AddKeyword("buffer:", tiffBUFFER);
    AddKeyword(":noname", tiffNONAME);
    AddKeyword(":",       tiffCOLON);
    AddKeyword(";",       CompSemi);
    AddKeyword(",",       CompComma);
    AddKeyword("c,",      CompCommaC);
    AddKeyword("literal", CompLiteral);
    AddKeyword(",\"",     tiffCommaString);
    AddKeyword(".\"",     tiffMsgString);
    AddKeyword("s\"",     tiffSString);
    AddKeyword("c\"",     tiffCString);

    AddKeyword("[char]",  TiffLitChar);
    AddKeyword("char",    TiffChar);
    AddKeyword("exit",    CompExit);
    AddKeyword("defer",   tiffDEFER);
    AddKeyword("is",      tiffIS);
    AddKeyword("+if",     CompPlusIf);
    AddKeyword("ifnc",    CompIfNC);
    AddKeyword("if",      CompIf);
    AddKeyword("else",    CompElse);
    AddKeyword("then",    CompThen);
    AddKeyword("begin",   CompBegin);
    AddKeyword("again",   CompAgain);
    AddKeyword("until",   CompUntil);
    AddKeyword("+until",  CompPlusUntil);
    AddKeyword("while",   CompWhile);
    AddKeyword("+while",  CompPlusWhile);
    AddKeyword("repeat",  CompRepeat);
    AddKeyword("?do",     CompQDo);
    AddKeyword("do",      CompDo);
    AddKeyword("loop",    CompLoop);
    AddKeyword("i",       CompI);
    AddKeyword("leave",   CompLeave);
    AddKeyword("widlist", tiffWordlistHead);
    AddKeyword("wordlist", tiffWordList);

//    AddKeyword("rom!",    tiffROMstore);
    AddKeyword("h'",      tiffHTICK);
    AddKeyword("'",       tiffTICK);
    AddKeyword("[']",     tiffBracketTICK);

    AddKeyword("macro",   tiffMACRO);
    AddKeyword("call-only", tiffCALLONLY);
    AddKeyword("anonymous", tiffANON);
    AddKeyword("see",     tiffSEE);
    AddKeyword("dasm",    tiffDASM);
    AddKeyword("locate",  tiffLOCATE);
    AddKeyword("replace-xt", ReplaceXTs);   // Replace XTs  ( NewXT OldXT -- )
    AddKeyword("xte-is",  xte_is);          // Replace a word's xte  ( NewXT -- )
    AddKeyword("make",    tiffMAKE);
    AddKeyword("save-image", tiffSaveImg);  // save binary image

    AddKeyword("idata-make", tiffIDATA);    // compile RAM initialization data structure
    AddKeyword("iwords",  ListKeywords);    // internal words, after the dictionary

    AddEquate ("RAMsize", RAMsize*4);
    AddEquate ("ROMsize", ROMsize*4);
    AddEquate ("SPIflashSize", SPIflashSize*4);

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
    AddEquate ("op_-",     opSUB);
    AddEquate ("op_c!+",   opCstorePlus);
    AddEquate ("op_c@+",   opCfetchPlus);
    AddEquate ("op_u2/",   opUtwoDiv);
    AddEquate ("op_no:",   opSKIP);
    AddEquate ("op_2+",    opTwoPlus);
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
    AddEquate ("op_rept",  opREPT);
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

////////////////////////////////////////////////////////////////////////////////
/// tiffPARSE gets the next <delimiter> delimited string from TIBB.
/// tiffPARSENAME gets the next blank delimited token.
/// Both leave their parameters on the stack for compatibility with the Forth
/// versions that will replace them.

uint32_t TIBhere (void) {               // get the current TIB address
    return (FetchCell(TIBB) + FetchCell(TOIN));
}
uint8_t TIBchar (void) {                // get the current TIB byte
    uint32_t tibb = FetchCell(TIBB);
    uint32_t toin = FetchCell(TOIN);
    return FetchByte(tibb + toin);
}
int TIBnotExhausted (void) {            // test for input not exhausted
    uint32_t tibs = FetchCell(TIBS);
    uint32_t toin = FetchCell(TOIN);
    if (TIBchar()==0) return 0;         // terminator found early
    if (toin < tibs) return 1;
    return 0;
}
void TOINbump (void) {                  // point to next TIB byte
    uint32_t toin = FetchCell(TOIN);
    StoreCell(toin+1, TOIN);
}
int SkipToChar (char c){                // skip to character
    while (TIBnotExhausted()) {
        if (TIBchar() == c) return 1;   // T if found
        TOINbump();
    } return 0;
}
void SkipWhite (void){                  // skip whitespsce (blanks)
    while (TIBnotExhausted()) {
        if (TIBchar() != ' ') break;
        TOINbump();
    }
}
void SkipPast(char c) {
    do {
        if (SkipToChar(c)) {            // found
            TOINbump();                 // skip the terminator
            return;
        }
    } while (Refill());
}

// Parse without skipping leading delimiter, return string on stack.
// Also return length of string for C to check.
uint32_t tiffPARSE (void) {             // ( delimiter -- addr length )
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
uint32_t tiffPARSENAME (void) {         // ( -- addr length )
    SkipWhite();
    PushNum(' ');
    return tiffPARSE();
}
// Interpret the TIB inside the VM, on the VM data stack.
void tiffINTERPRET(void) {
    while (tiffPARSENAME()) {           // get the next blank delimited keyword
#ifdef VERBOSE
        uint32_t tempLen = PopNum();
        uint32_t tempAddr = PopNum();
        FetchString(name, tempAddr, tempLen);
        printf("\nFind:|%s| at >IN=%X, STATE=%d ", name, FetchCell(TOIN), FetchCell(STATE));
        PushNum(tempAddr);
        PushNum(tempLen);
#endif
        uint32_t ht = tiffFIND();       // search for keyword in the Forth dictionary
        if (ht) {  // ( 0 ht )          // it's a Forth (in ROM) word
            PopNum();
            PopNum();
            uint32_t xt;
            if (FetchCell(STATE)) {     // compiling
                xt = 0xFFFFFF & FetchCell(ht - 8);
            } else {                    // interpreting
                xt = 0xFFFFFF & FetchCell(ht - 4);
            }
            tiffFUNC(xt);
        } else {                        // okay, maybe an internal keyword
            uint32_t length = PopNum();
            if (length>31) length=32;   // sanity check
            uint32_t address = PopNum();
            FetchString(name, address, (uint8_t)length);
            if (NotKeyword(name)) {     // try to convert to number
                char *eptr;
                long int x = strtol(name, &eptr, FetchCell(BASE));
                if ((x == 0) && ((errno == EINVAL) || (errno == ERANGE))) {
                    bogus: tiffIOR = -13;   // not a number
                } else {
                    if (*eptr) goto bogus;  // points at zero terminator if number
                    if (FetchCell(STATE)) {
#ifdef VERBOSE
                        printf(", Literal %d\n", (int32_t)x);  printed = 1;
#endif
                        Literal(x);
                    } else {
#ifdef VERBOSE
                        printf(", Push %d\n", (int32_t)x);  printed = 1;
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

int Refill(void) {
    char *buf;                          // text from file
    buf = File.Line;
    int TIBaddr = FetchCell(TIBB);
    size_t bufsize = MaxTIBsize;        // for GetLine
    StoreCell(0, TOIN);
    int length = GetLine(&buf, &bufsize, File.fp);
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
        tiffCOMMENT();
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
// Since GetLine includes the trailing LF (or CR), we lop it off.

char *DefaultFile = "tiff.f";           // Default file to load from

void tiffQUIT (char *cmdline) {
    int loaded = 0;
    size_t bufsize = MaxTIBsize;        // for GetLine
    char *buf;                          // text from stdin (keyboard)
    int length, source, TIBaddr;
    int NoHi = 0;                       // suppress extra "ok"s
    LoadKeywords();
    StoreCell(STATUS, FOLLOWER);  	    // only terminal task
    WordlistHead();
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
            switch (source) {
                case 0:                 // load TIBB from keyboard
                    if (!loaded) {
#ifdef VERBOSE
                        printf("%d\nAttempting to include file %s\n", tiffIOR, DefaultFile);
                        printed = 1;
#endif
                        FILE *test = fopen(DefaultFile, "rb");
                        if (test != NULL) {
                            fclose(test);           // if default file exists
                            tiffINCLUDED(DefaultFile);
                        }
                        loaded = 1;
//                        NoHi = 1;       // tell not to say "ok"
                        break;
                    }
                    if (cmdline) {      // first time through use command line
                        strcpy (File.Line, cmdline);
                        length = strlen(cmdline);
                        if (length >= MaxTIBsize) tiffIOR = -62;
                        cmdline = NULL; // clear cmdline
                    } else {
                        buf = File.Line;
#ifdef __linux__
                        CookedMode();
#endif // __linux__
                        length = GetLine(&buf, &bufsize, stdin);   // get input line
                        if (length >= MaxTIBsize) tiffIOR = -62;
#if (OKstyle==0)        // undo the newline that GetLine generated
                        printf("\033[A\033[%dC", (int)strlen(File.Line));
#endif
                        if (length > 0) length--;   // remove LF or CR
                        File.Line[length] = 0;
                    }
                    StoreCell((uint32_t)length, TIBS);
                    StoreString(File.Line, (uint32_t)TIBaddr);
                    ColorNormal();
                    break;
                case 1:
                    Refill();
                    break;
                default:	// unexpected
                    StoreCell(0, SOURCEID);
            }
#ifdef VERBOSE
            FetchString(name, FetchCell(TIBB), (uint8_t)FetchCell(TIBS));
            printf("ior before Interpret of |%s| is %d\n", name, tiffIOR);
#endif
            tiffINTERPRET();            // interpret the TIBB until it's exhausted
            if (tiffIOR) break;
            switch (FetchCell(SOURCEID)) {
                case 0:
                    if (NoHi) {
                        NoHi = 0;
                        break;
                    }
#if (OKstyle == 0)      // FORTH Inc style
                    printf(" ok\n");
#elif (OKstyle == 1)    // openboot style
                    if (printed) {
                        printed = 0;
                        printf("\n");
                    }
                    printf("ok ");    // should only newline if some text has been output
#else                   // openboot style with stack depth
                    if (printed) {
                        printed = 0;
                        printf("\n");
                    }
                    printf("%d:ok ", Sdepth());
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
        printed = 0;    // already at newline
        ColorNormal();
    }
}

