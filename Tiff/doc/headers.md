# Header space

## To Hash or Not to Hash

Typical header space will be kept flash memory, so headers are subject to the constraints of writing only to erased memory. Some Forths use a hashed dictionary, where the name to be searched uses a hash to resolve the dictionary thread to search. Is this necessary? Consider the case of headers being kept in SPI flash, clocked at 25 MHz by an MCU's SPI port. Random 32-bit reads spend about 4 usec setting the address and reading the 32-bit link followed by a 32-bit packed name and name length. A typical Forth app tends to be in the 1000-word range. With one dictionary thread, that would take 4 ms to search. Using the scheme of following the list forward to find the end, since it doesn't have an end pointer until it's programmed once into the flash, add another 3 ms. So, 7 ms for a search.

A Forth app with 10,000 words would take 70 seconds to load at that rate, but since the dictionary is built up linearly from nothing it would be more like 35 seconds. Reloading the app would be rarely done. Half a minute for something rarely done isn't bad. More reasonable hardware would use quad SPI at 50 MHz, or 10x the speed. So, three seconds.

Tiff, being a PC app, should be quite fast even while reading the flash image by stepping a simulated stack computer. Overall, there's no need for hashing the dictionary.
