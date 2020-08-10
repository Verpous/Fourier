#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <windows.h>

#define FILE_FILTER TEXT("Wave files\0*.wav\0")

// Takes a path to a WAVE file and verifies that it is a WAVE file, then reads its data into memory.
void ReadWaveFile(LPCTSTR);

// Writes the modified data from memory back to the file that is currently open.
void WriteWaveFile();

// Creates a new file with the modified data that we have in memory.
void WriteNewWaveFile(LPCTSTR);

#endif