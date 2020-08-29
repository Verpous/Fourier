#ifndef WAVE_READ_WRITER_INTERNAL_H
#define WAVE_READ_WRITER_INTERNAL_H

#include "WaveReadWriter.h"

// Allocates and initializes data inside the fileInfo for the file path given.
void AllocateWaveFile(FileInfo**, LPCTSTR);

// Locates important chunks and assigns their offsets at the given addresses. Also determines whether the wave data is a list. Returns zero iff there's a chunk that appeared more than once.
ReadWaveResult FindImportantChunks(FileInfo*, DWORD*, DWORD*, DWORD*);

// Reads the data at the given offset in the file into the format field of the FileInfo. Returns a nonzero value iff it succeeded.
char ReadFormatChunk(FileInfo*, DWORD);

// Reads the data at the given offset in the file into the waveform field of the FileInfo. Returns a nonzero value iff it succeeded.
char ReadWaveformChunk(FileInfo*, DWORD);

// Reads the data at the given offset in the file into the cue field of the FileInfo. Returns a nonzero value iff it succeeded.
char ReadCueChunk(FileInfo*, DWORD);

// Returns true iff the first element has a smaller ChunkStart than the second.
char CuePointComparator(void*, void*);

// Checks that all the chunks that have already been read into the fileInfo are supported and in line with the specifications. Returns FILE_READ_SUCCESS iff the chunks are valid.
ReadWaveResult ValidateFile(FileInfo*);

// Checks that the format is supported and returns a result. The result is FILE_READ_SUCCESS iff the chunk is ok.
ReadWaveResult ValidateFormat(FileInfo*);

// Checks that the waveform chunk is in line with the documentation. The result is FILE_READ_SUCCESS iff the chunk is ok.
ReadWaveResult ValidateWaveform(FileInfo*);

// Checks that the cue chunk is in line with the documentation. The result is FILE_READ_SUCCESS iff the chunk is ok.
ReadWaveResult ValidateCue(FileInfo*);

// Counts the length of a single channel from this file in samples.
unsigned long long CountSampleLength(FileInfo*);

// Copies the given source file into the destination.
void CopyWaveFile(FILE*, FILE*);

// Writes a WAVE file from the given data. The data chunk is filled with junk.
void WriteNewFile(FILE*, FileInfo*);

#endif