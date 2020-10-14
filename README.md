# Fourier
A simple Windows program that lets you manipulate the strengths of different frequencies in .wav files. This program can be used to do Fourier synthesis, Fourier analysis, and equalization.

Compilation instructions:
1. Install [MSYS2](https://www.msys2.org/)
2. Install mingw-w64 and [make](https://www.gnu.org/software/make/) for 64-bit using MSYS2
3. Add mingw-w64 and make's bin folders to PATH (should look something like "C:\msys64\mingw64\bin" and "C:\msys64\usr\bin", respectively)
4. Clone this repository
5. Run make inside the root directory of the repository. This will create the program executable named "fourier.exe" inside a folder named "bin"

You can run "make clean" to empty the bin folder if you want to recompile everything.

The makefile includes several additional targets which are explained inside the makefile via comments.
