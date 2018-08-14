#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "vm.h"
#include "tiff.h"
#include "vmaccess.h"
#include "compile.h"
#include "fileio.h"
#include "colors.h"
#include <string.h>
#include <ctype.h>
#include <sys/time.h>

#define MaxKeywords 256
#define MaxFiles 20
#define File FileStack[filedepth]

char name[MaxTIBsize+1];                // generic scratchpad (maybe not such a great idea making it global)

/**
* Returns the current time in microseconds.
*/
long getMicrotime(){
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);
    return currentTime.tv_sec * (int)1e6 + currentTime.tv_usec;
}

int tiffIOR = 0;                        // Interpret error detection when not 0
int ShowCPU = 0;                        // Enable CPU status display
int printed = 0;                         // flag, T if text was printed on console

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
void tiffCISon (void) {                 // enable CPU display mode
    CaseInsensitive = 1;
}
void tiffCISoff (void) {                // disable CPU display mode
    CaseInsensitive = 0;
}

void tiffCPUgo (void) {                 // Enter the single stepper
    ShowCPU = 1;
    vmTEST();
}
void tiffCLS (void) {                   // clear screen
    printf("\033[2J");                  // CLS
    if (ShowCPU) printf("\033[%dH", DumpRows+2);  // cursor below CPU
}
void tiffROMstore (void) {
    uint32_t a = PopNum();
    uint32_t n = PopNum();
    StoreROM (n, a);
}

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
	CommaH ((File.FID << 24) | 0xFFFFFF);              // [-3]: File ID | where
	CommaH (((File.LineNumber & 0xFF)<<24)  +  (xtc & 0xFFFFFF)); // [-2]
	CommaH (((File.LineNumber & 0xFF00)<<16) + (xte & 0xFFFFFF)); // [-1]
	uint32_t wid = FetchCell(CURRENT);                 // CURRENT -> Wordlist
	uint32_t link = FetchCell(wid);
	StoreCell (FetchCell(HP), wid);
	CommaH (((Size&0xFF)<<24) | link);                 // [0]: Upper size | link
	CommaXstring(name, CommaH, flags);                 // [1]: Name
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
// When a file is included, the rest of the TIB is discarded.
// A new file is pushed onto the file stack

void tiffINCLUDED (char *name) {
    StoreCell(1, SOURCEID);
    filedepth++;
    strcpy (File.FilePath, name);
    File.fp = fopen(name, "r");
#ifdef VERBOSE
    printf("\nOpening file %s, handle 0x%X\n", name, (uint32_t)File.fp);
#endif
    File.LineNumber = 0;
    File.Line[0] = 0;
    File.FID = FileID;
    tiffCOMMENT();
    if (File.fp == NULL) {
        tiffIOR = -199;
    } else {
        uint32_t hp = FetchCell(HP);
        if (!FileID) {                  // not the first header
            StoreROM(hp, FilenameListHead);
            FilenameListHead = hp;
        }
        CommaH(0xFFFFFFFF);             // forward link
        CommaHstring(File.FilePath);
        FileID++;
        if (FileID == 255) tiffIOR = -99;
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
    StoreByte(1, COLONDEF);
    StoreCell(1, STATE);
}

void tiffDEFER (void) {
    FollowingToken(name, 32);
    NewGroup();
    CommaHeader(name, FetchCell(CP), ~16, 1, 0xC0);
    // xtc must be multiple of 8 ----^
    CompDefer();
}

void tiffWORDS (void) {
    FollowingToken(name, 32);
    tiffWords (name, 0);
}

void tiffXWORDS (void) {
    FollowingToken(name, 32);
    tiffWords (name, 1);
}
void tiffSAVEcrom (void) {
    FollowingToken(name, 32);
    SaveROMasC (name);
}
void tiffSAVEcaxi (void) {
    FollowingToken(name, 32);
    SaveAXIasC (name, 1024);
}

void benchmark(void) {
    long now = getMicrotime();
    uint32_t i, sum;
    for (i=0; i<1000000; i++) {
        sum += FetchCell(0x4000+(i&0xFF));
    }
    now = getMicrotime() - now;
    printf("%d ps", (unsigned int)now);  printed = 1;
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
        strcpy(ErrorString, name);
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
void tiffIS (void) {                    // patch ROM defer
    tiffTICK();
    uint32_t dest = PopNum();
    StoreROM(0xFC000000 + (PopNum() >> 2), dest);
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
void tiffRunBrk (void) {                 // run to breakpoint
    tiffTICK();
    breakpoint = PopNum();
    tiffFUNC(breakpoint);
    tiffCPUon();
}

void tiffSTATS (void) {
#ifdef TRACEABLE
    static unsigned long mark;
    printf("\nClock cycles elapsed: %lu, since last: %lu ",
           cyclecount, cyclecount-mark);
    mark = cyclecount;
#endif
    uint32_t hp = FetchCell(HP);
    printf("\nROM usage: %d bytes of %d",
           FetchCell(CP), ROMsize*4);  printed = 1;
    printf("\nFlash usage: HP = %d of %d, header space is %d bytes of %d",
           hp, AXIsize*4, hp-HeadPointerOrigin, AXIsize*4-HeadPointerOrigin);
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


void LoadKeywords(void) {               // populate the list of gator brain functions
    keywords = 0;                       // start empty
    AddKeyword("bye",     tiffBYE);
    AddKeyword("[",       tiffSTATEoff);
    AddKeyword("]",       tiffSTATEon);
    AddKeyword("\\",      tiffCOMMENT);
    AddKeyword("//",      tiffCOMMENT); // too much cog dis switching between C and Forth
    AddKeyword(".",       tiffDOT);
    AddKeyword("stats",   tiffSTATS);
    AddKeyword(".static", ListOpcodeCounts);
    AddKeyword("+cpu",    tiffCPUon);
    AddKeyword("-cpu",    tiffCPUoff);
    AddKeyword("cpu",     tiffCPUgo);
    AddKeyword("dbg",     tiffRunBrk);  // set and run to breakpoint(s)
    AddKeyword("cls",     tiffCLS);
    AddKeyword("CaseSensitive",   tiffCISoff);
    AddKeyword("CaseInsensitive", tiffCISon);
    AddKeyword("include", tiffINCLUDE);
    AddKeyword("equ",     tiffEQU);
    AddKeyword("words",   tiffWORDS);
    AddKeyword("xwords",  tiffXWORDS);
    AddKeyword(":noname", tiffNONAME);
    AddKeyword(":",       tiffCOLON);
    AddKeyword(";",       Semicolon);
    AddKeyword("exit",    SemiExit);
    AddKeyword("defer",   tiffDEFER);
    AddKeyword("is",      tiffIS);
    AddKeyword("if",      CompIf);
    AddKeyword("then",    CompThen);
    AddKeyword("begin",   CompBegin);
    AddKeyword("+until",  CompPlusUntil);
    AddKeyword("rom!",    tiffROMstore);
    AddKeyword("bench",   benchmark);
    AddKeyword("h'",      tiffHTICK);
    AddKeyword("'",       tiffTICK);
    AddKeyword("macro",   tiffMACRO);
    AddKeyword("call-only", tiffCALLONLY);
    AddKeyword("anonymous", tiffANON);
    AddKeyword("see",     tiffSEE);
    AddKeyword("dasm",    tiffDASM);
    AddKeyword("replace-xt", ReplaceXTs);   // Replace XTs  ( NewXT OldXT -- )
    AddKeyword("save-rom",   tiffSAVEcrom);
    AddKeyword("save-flash", tiffSAVEcaxi);
    AddKeyword("iwords",   ListKeywords);   // internal words, after the dictionary
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
    AddEquate ("op_-bran", opMiBran);
    AddEquate ("op_jmp",   opJUMP);
    AddEquate ("op_w!+",   opWstorePlus);
    AddEquate ("op_w@+",   opWfetchPlus);
    AddEquate ("op_and",   opAND);
    AddEquate ("op_litx",  opLitX);
    AddEquate ("op_>r",    opPUSH);
    AddEquate ("op_call",  opCALL);
    AddEquate ("op_0=",    opZeroEquals);
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
    AddEquate ("op_-c",    opSUBC);
    AddEquate ("op_rp!",   opSetRP);
    AddEquate ("op_@",     opFetch);
    AddEquate ("op_-if:",  opSKIPGE);
    AddEquate ("op_sp",    opSP);
    AddEquate ("op_@as",   opFetchAS);
    AddEquate ("op_sp!",   opSetSP);
    AddEquate ("op_c@",    opCfetch);
    AddEquate ("op_port",  opPORT);
    AddEquate ("op_ifc:",  opSKIPNC);
    AddEquate ("op_lit",   opLIT);
    AddEquate ("op_up",    opUP);
    AddEquate ("op_!as",   opStoreAS);
    AddEquate ("op_up!",   opSetUP);
    AddEquate ("op_r@",    opRfetch);
    AddEquate ("op_com",   opCOM);
    AddEquate ("hp0", HeadPointerOrigin);  // bottom of head space
    AddEquate ("base",       BASE);         // tiff.h variable names
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
    AddEquate ("c_wids",     WIDS);
    AddEquate ("c_called",   CALLED);
    AddEquate ("c_slot",     SLOT);
    AddEquate ("c_litpend",  LITPEND);
    AddEquate ("c_colondef", COLONDEF);
    AddEquate ("calladdr",   CALLADDR);
    AddEquate ("nextlit",    NEXTLIT);
    AddEquate ("iracc",      IRACC);
    AddEquate ("context",    CONTEXT);
    AddEquate ("forthwid",   FORTHWID);
    AddEquate ("tib",        TIB);
    AddEquate ("head",       HEAD);

}

int NotKeyword (char *key) {            // do a command, return 0 if found
    int i = 0;
    char str1[16], str2[16];
    if (strlen(key) < 16) {
        for (i = 0; i < keywords; i++) { // scan the list for the word name
            strcpy(str1, key);
            strcpy(str2, HostWord[i].name);
            if (CaseInsensitive) {
                strlwr(str1);
                strlwr(str2);
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
void SkipWhite (void){                  // skip whitespsce (blanks)
    while (TIBnotExhausted()) {
        if (TIBchar() != ' ') break;
        TOINbump();
    }
}
// Parse without skipping delimiter, return string on stack.
// Also return length of string for C to check.
uint32_t tiffPARSE (void) {             // ( delimiter -- addr length )
    uint32_t length = 0;                // returns a string
    uint8_t delimiter = (uint8_t) PopNum();
    PushNum(TIBhere());
    while (TIBnotExhausted()) {
        if (TIBchar() == delimiter) break;
        TOINbump();                     // get string
        if (length==31) break;          // token size limit
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
        uint32_t ht = tiffFIND();     // search for keyword in the dictionary
        if (ht) {  // ( 0 ht )
            PopNum();
            PopNum();
            uint32_t xt;
            if (FetchCell(STATE)) {     // compiling
                xt = 0xFFFFFF & FetchCell(ht - 8);
            } else {                    // interpreting
                xt = 0xFFFFFF & FetchCell(ht - 4);
            }
            tiffFUNC(xt);
        } else {
            // dictionary search using ROM head space not implemented yet.
            // Assume it falls through with ( c-addr len ) on the stack.
            uint32_t length = PopNum();
            if (length>31) length=32;   // sanity check
            uint32_t address = PopNum();
            FetchString(name, address, (uint8_t)length);
            if (NotKeyword(name)) {    // try to convert to number
                char *eptr;
                long int x = strtol(name, &eptr, 0); // automatic base (C format)
                if ((x == 0) && ((errno == EINVAL) || (errno == ERANGE))) {
                    bogus: strcpy(ErrorString, name); // not a number
                    tiffIOR = -13;
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
            else printf(" <<< Local Keyword\n");
#endif
            if (Sdepth()<0) {
                tiffIOR = -4;
            }
            if (Rdepth()<0) {
                tiffIOR = -6;
            }
            if (tiffIOR) goto ex;
        }
    }
ex: PopNum();                           // keyword is an empty string
    PopNum();
}

// Keyboard input uses the default terminal cooked mode.
// Since getline includes the trailing LF (or CR), we lop it off.

char *DefaultFile = "tiff.f";           // Default file to load from

void tiffQUIT (char *cmdline) {
    int loaded = 0;
    size_t bufsize = MaxTIBsize;        // for getline
    char *buf;                          // text from stdin (keyboard)
    int length, source, TIBaddr;
    int NoHi = 0;                       // suppress extra "ok"s
    LoadKeywords();
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
                        FILE *test = fopen(DefaultFile, "r");
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
                        length = getline(&buf, &bufsize, stdin);   // get input line
                        if (length >= MaxTIBsize) tiffIOR = -62;
#if (OKstyle==0)        // undo the newline that getline generated
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
                    buf = File.Line;
                    length = getline(&buf, &bufsize, File.fp);
                    if (length < 0) {  // EOF
#ifdef VERBOSE
                        printf("Closing file handle 0x%X\n", (uint32_t)File.fp);
#endif
                        fclose (File.fp);
                        filedepth--;
                        if (filedepth == 0) {
                            StoreCell(0, SOURCEID);
                        }
                        tiffCOMMENT();
                    } else {
                        if (length >= MaxTIBsize) tiffIOR = -62;
                        if (length > 0) length--; // remove LF or CR
                        File.Line[length] = 0;
                        File.LineNumber++;
                        StoreCell((uint32_t)length, TIBS);
                        StoreString(File.Line, (uint32_t)TIBaddr);
                    }
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
                    break;
                default:
                    break;
            }
        }
        if (tiffIOR == -99999) return;  // produced by BYE
        ColorError();
        ErrorMessage(tiffIOR);
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

