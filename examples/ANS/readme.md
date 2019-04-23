# ANS Forth Demo

The ANS program runs the VM out of an internal ROM (in vm.c) compiled by `make.bat`.
It provides an ANS Forth.
The program size is 56 kB when compiled (statically linked) as a Code::Blocks console application.

Although the same VM file is used in the ANS Forth as in Mforth, the Mforth has tracing enabled
and has variable ROM and RAM sizes. Letting C do its constant folding and other optimizations to
speeds things up.

For example, `make.bat` leaves Mforth open so you can run `bench`. Type `bye` to quit.
It takes about 500 ms on my i7 laptop.
When ANS is compiled by a C compiler and run, `bench` takes more like 170 ms.
That's a 3x improvement.
