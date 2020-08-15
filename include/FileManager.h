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

#define ReadWaveResultCode(x) ((~FILE_READ_WARNING) & (x))
#define ResultHasWarning(x) (FILE_READ_WARNING & (x))

// How this enum works: Results are numbered as usual, but the MSB is OR'd in when you want to differentiate between success without warning and with.
typedef enum
{
    FILE_READ_SUCCESS   = 0, // The fact that this is 0 means you can OR it with an error and get the error.
    FILE_CANT_OPEN      = 1,
    FILE_NOT_WAVE       = 2,
    FILE_BAD_WAVE       = 3,
    FILE_BAD_FORMAT     = 4,
    FILE_BAD_BITDEPTH   = 5,
    FILE_BAD_FREQUENCY  = 6,
    FILE_BAD_SIZE       = 7,
    FILE_MISC_ERROR     = 8,
    FILE_READ_WARNING   = 0x80000000,
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
    ChunkHeader header;
    WAVEFORMATEXTENSIBLE contents;
} FormatChunk;

typedef struct WaveformSegment
{
    ChunkHeader header;
    DWORD relativeOffset; // The offset that the header begins in relative to the offset in the WaveformChunk this belongs to.
} WaveformSegment;

typedef struct WaveformChunk
{
    char isList;
    DWORD offset; // For lists, this should point to the byte after the 'wavl' FOURCC. For non-lists, this points to the first byte in the chunk header of the data chunk.
    DWORD segmentsLength;
    WaveformSegment* segments;
} WaveformChunk;

typedef struct CuePoint
{
    DWORD name;
    DWORD position;
    FOURCC chunkId;
    DWORD chunkStart;
    DWORD blockStart;
    DWORD sampleOffset;
} CuePoint;

typedef struct CueChunk
{
    ChunkHeader header;
    DWORD cuePoints;
    CuePoint* pointsArr;
} CueChunk;

typedef struct FileInfo
{
    FILE* file;
    LPTSTR path;
    FormatChunk format; 
    WaveformChunk waveform;
    CueChunk* cue; // This chunk is in a pointer while others aren't because this one is optional.
} FileInfo;

// Allocates and initializes fileInfo.
void CreateFileInfo(LPCTSTR);

// Deallocates fileInfo and removes the reference.
void DestroyFileInfo();

// Takes a path to a WAVE file and verifies that it is a WAVE file, then reads its data into memory.
ReadWaveResult ReadWaveFile(LPCTSTR);

// Locates important chunks and assigns their offsets at the given addresses. Also determines whether the wave data is a list. Returns zero iff there's a chunk that appeared more than once.
ReadWaveResult FindImportantChunks(DWORD*, DWORD*, DWORD*);

// Reads the data at the given offset in the file into the format field of the FileInfo. Returns a nonzero value iff it succeeded.
char ReadFormatChunk(DWORD);

// Reads the data at the given offset in the file into the waveform field of the FileInfo. Returns a nonzero value iff it succeeded.
char ReadWaveformChunk(DWORD);

// Reads the data at the given offset in the file into the cue field of the FileInfo. Returns a nonzero value iff it succeeded.
char ReadCueChunk(DWORD);

// Returns true iff the first element has a smaller ChunkStart than the second.
char CuePointComparator(void*, void*);

// Checks that all the chunks that have already been read into the fileInfo are supported and in line with the specifications. Returns FILE_READ_SUCCESS iff the chunks are valid.
ReadWaveResult ValidateFile();

// Checks that the format is supported and returns a result. The result is FILE_READ_SUCCESS iff the chunk is ok.
ReadWaveResult ValidateFormat();

// Checks that the waveform chunk is in line with the documentation. The result is FILE_READ_SUCCESS iff the chunk is ok.
ReadWaveResult ValidateWaveform();

// Checks that the cue chunk is in line with the documentation. The result is FILE_READ_SUCCESS iff the chunk is ok.
ReadWaveResult ValidateCue();

// Writes the modified data from memory back to the file that is currently open.
void WriteWaveFile();

// Creates a new file with the modified data that we have in memory.
void WriteNewWaveFile(LPCTSTR);

// Check if a file is new, that is it doesn't have any save location associated with it yet.
char IsFileNew();

#endif