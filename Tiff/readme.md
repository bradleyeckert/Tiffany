# Tiff, a PC host for Mforth

Tiff is a C console application that implements a minimal Forth. “Tiff” is short for “Tiffany”, the main character in the film “Bride of Chucky”.

Tiff uses a simulated Em Forth CPU to execute compiled code as needed. It’s designed to load a Forth system mostly from scratch,
gradually handing off all control to the simulated CPU. The resulting ROM image is binary compatible with the FPGA or ASIC based CPU,
which runs the same Forth system (big or small) as Tiff.

## Host Forth

The Host Forth is based on C. It implements an interpreter and a number of words using C. They use data structures in the Forth VM whenever possible. This tends to be more wordy than simply using C data structures, but it allows C functions to be replaced by Forth versions as they become available.

After loading up a system, Tiff can be used to run a Forth application, but it's meant to generate a Forth system in C source that will be embedded in another C project. You would compile and run that C application to run the Forth.

## Target Forth

A stack machine is simulated by the VM, so the code looks like machine code that's made from Forth primitives. The dictionary uses a dual-xt model. Depending on `state`, one xt or the other of the word from the input stream is executed. A dual-xt model allows for a classical search order, which is the feature of Forth most useful in creating domain-specific languages.

## Not ANS

Wait, what? Okay, mostly ANS. Flash-based Forths require small syntax differences. The main difference has to do with compiling headers. You don't want to define now and patch later because patching non-blank flash contents is tricky business. I mean you can, but you shouldn't assume you can. For example, instead of declaring a word `immediate` after it's been defined, it should be declared as such before the header is compiled.

Word headers have dual xts as well as optional data fields. In other to compile various kinds of words, multiple flavors of `:` are provided. These flavors are:

- `:` compiles a word whose compile-time action compiles the word.
- `macro:` compiles a word whose compile-time action copies the word as a macro.
- `immed:` compiles a word whose compile-time action is the word.
- `::` compiles a word whose compile-time action is on the stack.

With these, there will be some minor incompatibility with ANS Forths, but not so much that a compatibility layer is hard to make so as to run your code on those systems. For example, an application might use `immed:`.

To support the current syntax of `create` and `does>`, let's have the interpreter gracefully handle undefined xts.

Normally, STATE selects which xt to execute. If the xt is blank (all '1's), it is not executed. Instead, it assumes a default action that uses the `w` field in the header. If STATE=0, push `w` onto the stack. Otherwise, compile `w` as a literal. This allows `create` to have blank xts and a 'w' field set to whatever address space is used for the data.

`does>` patches the xts of a header made by `create`, which it can only do because they are blank. `>body` returns the `w` value.

There are two or three possible memory spaces that can be used by `create` and similar words. The normative cross compiler standard provides CDATA, IDATA and UDATA to select these. Good enough. That standard went down the dual wordlist road, but otherwise has a lot of good ideas.

