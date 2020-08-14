CC:=gcc # C Compiler
CFlags=-Wall -Wno-unknown-pragmas -c -Iinclude # C Compiler Flags. -Wno-unknown-pragmas gets rid of warnings about regions in the code.
LFlags:=-Wall -mwindows # Linker Flags. -mwindows means that when you run the program it doesn't open cmd.

.PHONY: all run runx clean unicode ansi

# Default target is unicode
all: unicode

# Appending UNICODE definition to CFLAGS so everything will use unicode, then compiling.
unicode: CFlags += -D UNICODE -D _UNICODE
unicode: bin/fourier

# Compiling without defining UNICODE means we're targeting ANSI.
ansi: bin/fourier

# Linking with lcomdlg32 makes open/save file dialogs work.
# Linking with lksuser makes the KSDATAFORMAT_SUBTYPE_PCM macro work.
bin/fourier: bin/main.o bin/WindowManager.o bin/FileManager.o bin/SoundEditor.o
	$(CC) $(LFlags) bin/main.o bin/WindowManager.o bin/FileManager.o bin/SoundEditor.o -lcomdlg32 -lksuser -o bin/fourier

bin/main.o: src/main.c
	$(CC) $(CFlags) -o bin/main.o src/main.c

bin/WindowManager.o: src/WindowManager.c
	$(CC) $(CFlags) -o bin/WindowManager.o src/WindowManager.c

bin/FileManager.o: src/FileManager.c
	$(CC) $(CFlags) -o bin/FileManager.o src/FileManager.c

bin/SoundEditor.o: src/SoundEditor.c
	$(CC) $(CFlags) -o bin/SoundEditor.o src/SoundEditor.c

# Compiles and runs. Output streams are redirected to a log.
run: unicode
	bin/fourier >>log.log 2>&1

# "Run exclusively". Same as run, but won't try to compile it.
runx:
	bin/fourier >>log.log 2>&1
	
clean:
	rm -f bin/*