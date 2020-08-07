CC:=gcc # C Compiler
CFlags=-Wall -c -Iinclude # C Compiler Flags
LFlags:=-Wall # Linker Flags

.PHONY: all run runx clean unicode ansi

# Default target is unicode
all: unicode

# Appending UNICODE definition to CFLAGS so everything will use unicode, then compiling.
unicode: CFlags += -D UNICODE -D _UNICODE
unicode: bin/fourier

# Compiling without defining UNICODE means we're targeting ANSI.
ansi: bin/fourier

# Linking with lcomdlg32 makes open/save file dialogs work.
bin/fourier: bin/main.o
	$(CC) $(LFlags) bin/main.o -lcomdlg32 -o bin/fourier

bin/main.o: src/main.c
	$(CC) $(CFlags) -o bin/main.o src/main.c

# Compiles and runs.
run: unicode
	bin/fourier

# "Run exclusively". Doesn't try to compile it, just runs it if it exists.
runx:
	bin/fourier
	
clean:
	rm -f bin/*