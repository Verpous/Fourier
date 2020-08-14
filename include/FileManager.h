#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <windows.h>
#include <mmreg.h>
#include <stdio.h>

// Frequencies we support, measured in Hertz.
#define FILE_MIN_FREQUENCY 8000
#define FILE_MAX_FREQUENCY 48000

// Byte-depths we support. Notice it's in bytes and not bits. This program only supports bit-depths which are multiples of 8.
#define FILE_MIN_DEPTH 1
#define FILE_MAX_DEPTH 4

typedef enum
{
    FILE_READ_SUCCESS,
    FILE_CANT_OPEN,
    FILE_NOT_WAVE,
    FILE_BAD_WAVE,
    FILE_BAD_FORMAT,
    FILE_BAD_BITDEPTH,
    FILE_BAD_FREQUENCY,
    FILE_TOO_BIG,
    FILE_MISC_ERROR,
} ReadWaveResult;

typedef struct ChunkHeader
{
    FOURCC id;
    DWORD size;
} ChunkHeader;

typedef struct WaveHeader
{
    ChunkHeader chunkHeader;
    FOURCC id;
} WaveHeader;

typedef struct FormatChunk
{
    ChunkHeader chunkHeader;
    WAVEFORMATEXTENSIBLE contents;
} FormatChunk;

typedef struct FileInfo
{
    FILE* file;
    LPTSTR path;
    FormatChunk format;
} FileInfo;


// Allocates and initializes fileInfo.
void CreateFileInfo(LPCTSTR);

// Deallocates fileInfo and removes the reference.
void DestroyFileInfo();

// Takes a path to a WAVE file and verifies that it is a WAVE file, then reads its data into memory.
ReadWaveResult ReadWaveFile(LPCTSTR);

// Locates important chunks and assigns their offsets at the given addresses. Also determines whether the wave data is a list. Returns zero iff there's a chunk that appeared more than once.
// This function assumes that when you call it, the file position is right where the first chunk header should be.
char FindImportantChunks(size_t*, size_t*, size_t*, size_t*, size_t*, char*);

// Reads the data at the given offset in the file into the format field of the FileInfo. Returns a nonzero value iff it succeeded.
char ReadFormatChunk(size_t);

// Checks that the format is supported and returns a result. The result is FILE_READ_SUCCESS iff the file is supported.
ReadWaveResult ValidateFormat();

// Writes the modified data from memory back to the file that is currently open.
void WriteWaveFile();

// Creates a new file with the modified data that we have in memory.
void WriteNewWaveFile(LPCTSTR);

// Check if a file is new, that is it doesn't have any save location associated with it yet.
char IsFileNew();

#endif