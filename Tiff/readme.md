# Tiff, a PC host for Mforth

Tiff is a straight C console application that implements a minimal Forth. “Tiff” is short for “Tiffany”, the main character in the film “Bride of Chucky”.

Tiff uses a simulated Em Forth CPU to execute compiled code as needed. It’s designed to load a Forth system mostly from scratch,
gradually handing off all control to the simulated CPU. The resulting ROM image is binary compatible with the FPGA or ASIC based CPU,
which runs the same Forth system (big or small) as Tiff.

## Host Forth

The Host Forth is based on C. It implements an interpreter and a number of words using C. They use data structures in the Forth VM whenever possible. This tends to be more wordy than simply using C data structures, but it allows C functions to be replaced by Forth versions as they become available.

After loading up a system, Tiff can be used to run a Forth application, but it's meant to generate a Forth system in C source that will be embedded in another C project. You would compile and run that C application to run the Forth.

## Target Forth

A stack machine is simulated by the VM, so the code looks like machine code that's made from Forth primitives. The dictionary uses a dual-xt model. Depending on `state`, one xt or the other of the word from the input stream is executed. A dual-xt model allows for a classical search order, which is the feature of Forth most useful in creating domain-specific languages.

