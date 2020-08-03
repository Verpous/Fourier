CC:=gcc # C Compiler
CFlags:=-Wall -c -Iinclude # C Compiler Flags
LFlags:=-Wall # Linker Flags

all: fourier

fourier: bin/main.o
	$(CC) $(LFlags) bin/main.o -o bin/fourier

bin/main.o: src/main.c
	$(CC) $(CFlags) -o bin/main.o src/main.c

run:
	bin/fourier
	
.PHONY: clean

clean:
	rm -f bin/*