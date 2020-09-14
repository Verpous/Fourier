# Fourier
A Windows program that lets you manipulate the strengths of different frequencies in a wave file. Currently in development.

Compilation instructions:
1. Install [MSYS2](https://www.msys2.org/)
2. Install mingw-w64 for 64-bit using MSYS2
3. Add mingw-w64's bin folder to PATH (should look something like "C:\msys64\mingw64\bin")
4. Clone this repository and create a folder named "bin" inside of it (if it doesn't already exist)
5. Run [make](https://www.gnu.org/software/make/) inside the root directory of the repository. This will create the program executable named "fourier.exe" inside the bin folder

If you wish, you can run the commands "make unicode" or "make ansi" to specify whether the program should use UTF-8 or ANSI strings. The default is unicode.

You can run "make clean" to empty the bin folder if you want to recompile everything.

The makefile includes a few additional targets which are explained inside the makefile via comments.
