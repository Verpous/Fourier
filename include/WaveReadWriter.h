#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include "MyUtils.h"
#include "SampledFunction.h"
#include <windows.h> // For various winapi features.
#include <mmreg.h>	 // For various WAVE-related things.
#include <stdio.h>	 // For dealing with files.

// Byte-depths we support. Notice it's in bytes and not bits. This program only supports bit-depths which are multiples of 8.
#define FILE_MIN_DEPTH 1
#define FILE_MAX_DEPTH 4

// Beyond this number of channels, the WAVE file specifications won't tell you what the channels mean.
#define MAX_NAMED_CHANNELS 18
#define CHANNEL_NAME_BUFFER_LEN 24

// If a file has fewer samples than this, it's zero-padded to reach this length.
#define MIN_FOURIER_LENGTH (1 << 16)

#define ResultHasError(x) (0x0000FFFF & (x))
#define ResultHasWarning(x) (0xFFFF0000 & (x))
#define ResultErrorCode(x) ResultHasError(x)
#define ResultWarningCode(x) ResultHasWarning(x)

// How this enum works: Results are numbered as usual, but the MSB is OR'd in when you want to differentiate between success without warning and with.
typedef enum
{
	// Success is 0 so you can OR it with anything.
	FILE_READ_SUCCESS = 0x00000000,

	// The lower 16 bits are used for errors.
	FILE_CANT_OPEN = 0x00000001,
	FILE_NOT_WAVE = 0x00000002,
	FILE_BAD_WAVE = 0x00000004,
	FILE_BAD_FORMAT = 0x00000008,
	FILE_BAD_BITDEPTH = 0x00000010,
	FILE_BAD_FREQUENCY = 0x00000020,
	FILE_BAD_SIZE = 0x00000040,
	FILE_BAD_SAMPLES = 0x00000080,
	FILE_MISC_ERROR = 0x00008000,

	// The higher 16 bits are used for warnings.
	FILE_CHUNK_WARNING = 0x80000000,
	FILE_CHAN_WARNING = 0x40000000,
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

typedef struct FileInfo
{
	FILE* file;
	LPTSTR path;
	WaveHeader header;
	FormatChunk format;
	WaveformChunk waveform;
	DWORD sampleLength;
} FileInfo;

// Closes the given file, and deallocates it.
void CloseWaveFile(FileInfo*);

// Takes a path to a WAVE file and verifies that it is a WAVE file, then reads its data into memory.
ReadWaveResult ReadWaveFile(FileInfo**, LPCTSTR);

// Occupies the given FileInfo with data about what the current file is according to the given new file creation parameters.
void CreateNewFile(FileInfo**, unsigned int, unsigned int, unsigned int);

// Loads the PCM data of the wave file into an array of functions it will allocate at the given address, such that the i'th function corresponds to the i'th channel.
// The data is loaded "interleaved", meaning each sample is complex, the real parts correspond to even indices of PCM samples, and the imaginary parts correspond to odds.
// Additionally, zero-padding is added to bring the sample length to a power of two.
// Returns zero iff there is insufficient memory for the operation.
char LoadPCMInterleaved(FileInfo*, Function***);

// Writes the modified data from memory back to the file. Returns zero iff there is insufficient memory for the operation.
char WriteWaveFile(FILE*, FileInfo*, Function**);

// Creates a new file with the modified data that we have in memory. Returns zero iff it failed to create the new file or there was insufficient memory for the operation.
char WriteWaveFileAs(FileInfo*, LPCTSTR, Function**);

// Check if a file is new, that is it doesn't have any save location associated with it yet.
char IsFileNew(FileInfo*);

// Occupies the array of strings with channel names. Assumes it's large enough to hold MAX_NAMED_CHANNELS strings.
unsigned short GetChannelNames(FileInfo*, TCHAR[][CHANNEL_NAME_BUFFER_LEN]);

// Returns how many editable channels are in this file.
unsigned short GetRelevantChannelsCount(FileInfo*);

#endif