# Header space

The most important thing about header space is that it lives in flash memory. The basic rule about flash programming is that you can only change a '1' to a '0'. Erased flash starts out at '1'. You may not program a '0' to the same bit twice. A good chip design might mitigate the possible damage caused by this, but you don't know about the parts you're dealing with. Writing a '0' to the same bit twice without erasing in between could over-charge the floating gate causing problems with incomplete erasure and reading of adjacent bits. So, Tiff throws an error if you try to program a '0' into a bit that's already at '0'.

An application can run without header space. It just won't have an interpreter. This is not a problem in embedded systems. SPI flash could be used in development and removed in the final system. Think of physically beheading a system by de-soldering the flash chip.

## To Hash or Not to Hash

Some Forths use a hashed dictionary, where the name to be searched uses a hash to resolve the dictionary thread to search. Is this necessary? Consider the case of headers being kept in SPI flash, clocked at 25 MHz by an MCU's SPI port. Random 32-bit reads spend about 4 usec setting the address and reading the 32-bit link followed by a 32-bit packed name and name length. A typical Forth app tends to be in the 1000-word range. With one dictionary thread, that would take 4 ms to search.

A Forth app with 10,000 words would take 40 seconds to load at that rate, but since the dictionary is built up linearly from nothing it would be more like 20 seconds. Reloading the app would be rarely done. Half a minute for something rarely done isn't bad. More reasonable hardware would use quad SPI at 50 MHz, or 10x the speed.

Tiff, being a PC app, should be fast even while reading the flash image by stepping a simulated stack computer. Benchmarking on a laptop i7 found 150ns per simulated SPI flash fetch. There are two of these per header, so with 1000 words a search should cost 300 usec. Multiplied by 10k searches is 3 sec. A more realistic estimate using a dictionary built from scratch gives a load time of 1.5 seconds.

For each wordlist, there is a RAM variable that holds the pointer to the head of the list. Hashing increases RAM usage. There's no need to hash the dictionary. In the interest of minimizing the size of target code, hashing is not used.

## Header Data Structure

Header space is a section of code space. It uses links and execution tokens wide enough to hit all of code space, including internal ROM and add-on definitions in flash. 24-bit cell addressing allows this range to cover 64M bytes.

The link and the name should be adjacent in memory to allow for faster fetch from SPI flash. A newly created minimal header also contains dual-xts, which are loaded upon header creation or resolved later. Note that unresolved fields in a packed record are allowed. The bits outside the field of interest are written with '1' when nearby fields are resolved. The header is:

| Cell | \[31:24\]                        | \[23:0\]                           |
| ---- |:---------------------------------| ----------------------------------:|
| -3   | Source File ID                   | Forward link                       |
| -2   | Source Line, Low byte            | xtc, Execution token for compile   |
| -1   | Source Line, High byte           | xte, Execution token for execute   |
| 0    | # of instructions in definition  | Link                               |

Immediately after the Link field is a counted string whose first byte is a 5-bit length and three flags.
The name length byte includes `smudge`, `call-only`, and `anonymous` bits that default to '1' and are set to '0' later on.

The Forward link is used at run time to initialize the WID of a wordlist if its WID is not initialized data (IDATA).
This is the case of a dictionary appended to a system, if it doesn't have IDATA.

| Bit | Usage       | Description                                  |
|:---:|:------------|:---------------------------------------------|
| 7   | Call Only   | '0' when tail call optimization is inhibited |
| 6   | Anonymous   | '0' when to be excluded from where list      |
| 5   | Smudged     | '0' when header is findable (used by : etc.) |
| 4:0 | Name Length | 0 to 31                                      |


`immediate` works by clearing a bit in xtc. It's done this way because flash can't program a `0` twice.

### Macro Compilation

The xtc of defined words is either `compile,` (by default) or `macrocomp,`. The two xts observe certain rules so that `macro` can flip bit 2 in xtc from `1` to`0`. These rules are:

1. They are adjacent.
2. The first function of the pair is macro expansion.
3. The pair starts at an address that's a multiple of 8.

Macro expansion is amazingly simple. Each slot of an instruction in a definition that's marked as a macro is compiled until `exit` is encountered.

### Immediate

IMMEDIATE means something different in a flash-based dual-xt Forth. We would like to patch `xtc` to match `xte`, but it's not feasible since `xtc` contains some '0' bits. The same trick used by `macro` is used. `immediate` and `macro` are used only with definitions, which have a limited selection of `xtc`s. Among these are `compile,`,  `comp-macro`, and `immediate`. Using 16-byte alignment, the code for these words can form a jump table:

cp @ 4 - 12 and  cp +!  \ align to 4 bytes past 16-byte boundary

- jmp do-immediate
- jmp comp-macro
- jmp compile,

This allows the default compile action to be `compile,`. By flipping bit 2 or bit 3 in `xtc` from '1' to '0', that gets changed to `comp-macro` or `do-immediate` respectively.

## Wordlists

A wordlist is created by creating a RAM variable and initializing it to 0. The WID is the address of that variable. To append to that wordlist, you would put that WID in CURRENT. A wordlist starts out empty, with its head pointer at 0. The first element in the list gets its link set to 0.

There is a list of wordlists, pointed to by `WORDLISTS`. The header structure is:

| Cell | Usage                          |
|:----:|-------------------------------:|
| 0    | Link                           |
| 1    | WID                            |
| 2    | Optional name (counted string) |

## Source File ID

There is a forward linked list for filenames. If a filename exists in the list, its position in the list is its ID number. If it must be appended, the filename is added to the list and the previously blank forward link is populated.

When Tiff's "include" opens a file, it adds the filename to this list by compiling a -1 (blank link) and the filename to head space, which uses HP as its pointer. Then it resolves the previous element's forward link.


