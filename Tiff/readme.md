# Tiff, a PC host for Mforth

Tiff is a C console application that implements a minimal Forth. “Tiff” is short for “Tiffany”, the main character in the film “Bride of Chucky”.
This is the Forth that Chuck should have written. I'm not as smart a programmer as Chuck Moore (who is?) but I'm giving it a go.
Forth needs to work with the rest of the world, which means being highly embeddable.

Tiff uses a simulated Forth CPU to execute compiled code as needed. It’s designed to load a Forth system mostly from scratch,
gradually handing off all control to the simulated CPU. The resulting ROM image is binary compatible with the FPGA or ASIC based CPU,
which runs the same Forth system (big or small) as Tiff.

Tiff is intended to generate C code that encapsulates the Forth application.
The code exports the state of Tiff (dictionaries etc) to something you can include in a C project.
The rationale for this is that Forth's utility as a stand-alone platform is diminishing.
It's better to host a smaller Forth in a C system that leverages the libraries and middleware of today's complex IDEs.
That brings Forth-like scripting and interactivity to C applications.

## Host Forth

The Host Forth is based on C. It implements an interpreter and a number of words using C. They use data structures in the Forth VM whenever possible. This tends to be more wordy than simply using C data structures, but it allows C functions to be replaced by Forth versions as they become available.

After loading up a system, Tiff can be used to run a Forth application, but it's meant to generate a Forth system in C source that will be embedded in another C project. You would compile and run that C application to run the Forth.

## Target Forth

A stack machine is simulated by the VM, so the code looks like machine code that's made from Forth primitives. The dictionary uses a dual-xt model. Depending on `state`, one xt or the other of the word from the input stream is executed. A dual-xt model allows for a classical search order, which is the feature of Forth most useful in creating domain-specific languages.

## Flash-based

Run-time code space, when it's writable, is assumed to be flash. You can change '1's to '0's but not vice versa. You also shouldn't try writing '0's twice, as some chips will over-charge the bit's gate charge making for unreliable behavior. Certain xts are chosen such they can be flipped to other xts by clearing select bits. For example, an xt can be flipped (by IMMEDIATE) from `compile` to `interpret`.

To support the current syntax of `create` and `does>`, let's have the interpreter gracefully handle undefined xts.

Normally, STATE selects which xt to execute. If the xt is blank (all '1's), it is not executed. Instead, it assumes a default action that uses the `w` field in the header. If STATE=0, push `w` onto the stack. Otherwise, compile `w` as a literal. This allows `create` to have blank xts and a 'w' field set to whatever address space is used for the data.

`does>` patches the xts of a header made by `create`, which it can only do because they are blank. `>body` returns the `w` value.

There are two or three possible memory spaces that can be used by `create` and similar words. The normative cross compiler standard provides CDATA, IDATA and UDATA to select these. Good enough. That standard went down the dual wordlist road, but otherwise has a lot of good ideas.

