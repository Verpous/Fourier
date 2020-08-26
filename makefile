CC:=gcc # C Compiler
CFlags=-Wall -Wno-unknown-pragmas -c -Iinclude # C Compiler Flags. -Wno-unknown-pragmas gets rid of warnings about regions in the code.
LFlags:=-Wall -mwindows # Linker Flags. -mwindows means that when you run the program it doesn't open cmd.

# comdlg32 makes open/save file dialogs work.
# ksuser makes the KSDATAFORMAT_SUBTYPE_PCM macro work.
# comctl32 makes various common controls work.
# shlwapi makes PathStripPath work.
LinkedLibs:=-lcomdlg32 -lksuser -lcomctl32 -lshlwapi

.PHONY: all run runx clean unicode ansi

# Default target is unicode
all: unicode

# Appending UNICODE definition to CFLAGS so everything will use unicode, then compiling.
unicode: CFlags += -D UNICODE -D _UNICODE
unicode: bin/fourier

# Compiling without defining UNICODE means we're targeting ANSI.
ansi: bin/fourier

bin/fourier: bin/main.o bin/WindowManager.o bin/WaveReadWriter.o bin/SoundEditor.o bin/MyUtils.o bin/SampledFunction.o
	$(CC) $(LFlags) bin/main.o bin/WindowManager.o bin/WaveReadWriter.o bin/SoundEditor.o bin/MyUtils.o bin/SampledFunction.o $(LinkedLibs) -o bin/fourier

bin/main.o: src/main.c
	$(CC) $(CFlags) -o bin/main.o src/main.c

bin/WindowManager.o: src/WindowManager.c
	$(CC) $(CFlags) -o bin/WindowManager.o src/WindowManager.c

bin/WaveReadWriter.o: src/WaveReadWriter.c
	$(CC) $(CFlags) -o bin/WaveReadWriter.o src/WaveReadWriter.c

bin/SoundEditor.o: src/SoundEditor.c
	$(CC) $(CFlags) -o bin/SoundEditor.o src/SoundEditor.c
	
bin/MyUtils.o: src/MyUtils.c
	$(CC) $(CFlags) -o bin/MyUtils.o src/MyUtils.c

bin/SampledFunction.o: src/SampledFunction.c
	$(CC) $(CFlags) -o bin/SampledFunction.o src/SampledFunction.c

# Compiles and runs. Output streams are redirected to a log.
run: unicode
	bin/fourier >>log.log 2>&1

# "Run exclusively". Same as run, but won't try to compile it.
runx:
	bin/fourier >>log.log 2>&1

# Same as run but without the io redirection. Intended for use by the code runner extension in vscode so it can display output.
runvscode: unicode
	bin/fourier
	
clean:
	rm -f bin/*