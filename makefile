# C Compiler.
CC:=gcc

# C compiler flags.
# -Wno-unknown-pragmas gets rid of warnings about regions in the code.
# -O3 makes gcc optimize hard.
# -std=c18 is the default, but I want to ensure it stays this way because apparently inline functions are very sensitive to the standard being used.
CFlags=-Wall -Wno-comment -Wno-unknown-pragmas -c -Iinclude -O3 -std=c18

# Linker flags.
# -mwindows makes it so when you run the program it doesn't open cmd.
LFlags:=-Wall -mwindows

# Libraries that we link.
# comdlg32 makes open/save file dialogs work.
# ksuser makes the KSDATAFORMAT_SUBTYPE_PCM macro work.
# comctl32 makes various common controls work.
# shlwapi makes PathStripPath work.
LinkedLibs:=-lcomdlg32 -lksuser -lcomctl32 -lshlwapi

.PHONY: all debug unicode ansi run runx runvscode clean

all: unicode

# Compiles and links everything with debug information that is useful to debuggers.
debug: CFlags += -g
debug: all

# Compiles and links everything for unicode strings.
unicode: CFlags += -D UNICODE -D _UNICODE
unicode: bin/fourier

# Compiles and links everything for ANSI strings.
ansi: bin/fourier

# Compiles and runs. Output streams are redirected to a log.
run: unicode
	bin/fourier >>errors.log 2>&1

# "Run exclusively". Same as run, but won't try to compile it.
runx:
	bin/fourier >>errors.log 2>&1

# Same as run but without the io redirection.
runstdio: unicode
	bin/fourier

# Empties the bin folder.
clean:
	rm -f bin/*

# The following targets do the actual job of compiling and linking all the different files. You'll probably never run them directly.
bin/fourier: bin/WindowsMain.o bin/WaveReadWriter.o bin/SoundEditor.o bin/MyUtils.o bin/SampledFunction.o bin/Resources.o
	$(CC) $(LFlags) bin/WindowsMain.o bin/WaveReadWriter.o bin/SoundEditor.o bin/MyUtils.o bin/SampledFunction.o bin/Resources.o $(LinkedLibs) -o bin/fourier

bin/WindowsMain.o: src/WindowsMain.c
	$(CC) $(CFlags) -o bin/WindowsMain.o src/WindowsMain.c

bin/WaveReadWriter.o: src/WaveReadWriter.c
	$(CC) $(CFlags) -o bin/WaveReadWriter.o src/WaveReadWriter.c

bin/SoundEditor.o: src/SoundEditor.c
	$(CC) $(CFlags) -o bin/SoundEditor.o src/SoundEditor.c
	
bin/MyUtils.o: src/MyUtils.c
	$(CC) $(CFlags) -o bin/MyUtils.o src/MyUtils.c

bin/SampledFunction.o: src/SampledFunction.c
	$(CC) $(CFlags) -o bin/SampledFunction.o src/SampledFunction.c

bin/Resources.o: resources/Resources.rc
	windres -Iinclude -o bin/Resources.o resources/Resources.rc