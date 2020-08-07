CC:=gcc # C Compiler
CFlags=-Wall -c -Iinclude # C Compiler Flags
LFlags:=-Wall -mwindows # Linker Flags

.PHONY: all run runx clean unicode ansi

# Default target is unicode
all: unicode

# Appending UNICODE definition to CFLAGS so everything will use unicode, then compiling.
unicode: CFlags += -D UNICODE -D _UNICODE
unicode: bin/fourier

# Compiling without defining UNICODE means we're targeting ANSI.
ansi: bin/fourier

# Linking with lcomdlg32 makes open/save file dialogs work.
bin/fourier: bin/main.o bin/WindowManager.o
	$(CC) $(LFlags) bin/main.o bin/WindowManager.o -lcomdlg32 -o bin/fourier

bin/main.o: src/main.c
	$(CC) $(CFlags) -o bin/main.o src/main.c

bin/WindowManager.o: src/WindowManager.c
	$(CC) $(CFlags) -o bin/WindowManager.o src/WindowManager.c

# Compiles and runs. Output streams are redirected to a log.
run: unicode
	bin/fourier >>log.log 2>&1

# "Run exclusively". Same as run, but won't try to compile it.
runx:
	bin/fourier >>log.log 2>&1
	
clean:
	rm -f bin/*