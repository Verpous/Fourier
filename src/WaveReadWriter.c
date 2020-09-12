// Fourier - a program for modifying the weights of different frequencies in a wave file.
// Copyright (C) 2020 Aviv Edery.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "WaveReadWriterInternal.h"
#include <tchar.h> // For dealing with strings that may be unicode or ANSI.
#include <ksmedia.h> // For the KSDATAFORMAT_SUBTYPE_PCM macro.
#include <math.h> // For the min macro.
#include <stdlib.h> // For malloc and such.
#include <share.h> // For shflags to _tfsopen.
#include <limits.h> // For integer max values.

#define FOURCC_WAVE     mmioFOURCC('W', 'A', 'V', 'E')
#define FOURCC_FORMAT   mmioFOURCC('f', 'm', 't', ' ')
#define FOURCC_WAVL     mmioFOURCC('w', 'a', 'v', 'l')
#define FOURCC_DATA     mmioFOURCC('d' , 'a', 't', 'a')
#define FOURCC_SILENT   mmioFOURCC('s', 'l', 'n', 't')
#define FOURCC_PLAYLIST mmioFOURCC('p', 'l', 's', 't')
#define FOURCC_SAMPLER  mmioFOURCC('s', 'm', 'p', 'l')

// Various loops use this to know when to stop in case the WAVE file is really stupid and has a lot of chunks with nothing.
#define MAX_CHUNK_ITERATIONS (1 << 16) 

// Buffer size that it used for operations such as file copying.
// This needs to be at least 2^20 in order to always be able to hold one block of PCM data. (4 byte depth limit, 2^16 channels limit means 2^20 blockAlign limit).
#define FILE_READING_BUFFER_LEN MEGAS(16)

// The biggest signed integer possible with the given byte depth.
#define DEPTH_MAX(depth) ((int)(depth == 1 ? CHAR_MAX : depth == 2 ? SHRT_MAX : depth == 3 ? 0x007FFFFF : INT_MAX))
#define DEPTH_MIN(depth) ((int)(depth == 1 ? CHAR_MIN : depth == 2 ? SHRT_MIN : depth == 3 ? 0xFF800000 : INT_MIN))

// A mask where the only bit set is the most significant bit of an int with the given byte depth.
#define DEPTH_HIGH_BIT(depth) (1 << ((depth * CHAR_BIT) - 1))

// A mask where the only set bits are the ones that unavailable in an int with the given byte depth.
#define SIGN_EXTEND_MASK(depth) (depth == 1 ? 0xFFFFFF00 : depth == 2 ? 0xFFFF0000 : depth == 3 ? 0xFF000000 : 0)

#define abs_Float(f) fabsf(f)
#define abs_Double(f) fabs(f)

#define lround_Float(f) lroundf(f)
#define lround_Double(f) lround(f)

void AllocateWaveFile(FileInfo** fileInfo, LPCTSTR path)
{
    *fileInfo = calloc(1, sizeof(FileInfo));

    // Creating copy of path string on heap for long term storage. +1 because of the null character.
    if (path != NULL)
    {
        size_t bufLen = _tcslen(path) + 1;
        (*fileInfo)->path = malloc(sizeof(TCHAR) * bufLen);
        _tcscpy_s((*fileInfo)->path, bufLen, path);
    }
}

void CloseWaveFile(FileInfo* fileInfo)
{
    // Dodging null pointer exceptions like a boss.
    if (fileInfo == NULL)
    {
        return;
    }

    // We're gonna take advantage of the fact that free can take null points and just not do anything in that case.
    free(fileInfo->path);
    free(fileInfo->waveform.segments);

    // Unlike free, fclose does require that we check for NULL beforehand.
    if (fileInfo->file != NULL)
    {
        fclose(fileInfo->file);
    }

    free(fileInfo);
}

#pragma region Opening

ReadWaveResult ReadWaveFile(FileInfo** fileInfo, LPCTSTR path)
{
    // Using _tfsopen rather than _tfopen so we can specify that we want exclusive write access. _tfsopen doesn't have a secure version.
    FILE* file = _tfsopen(path, TEXT("r+b"), _SH_DENYWR);

    // This is the only error that we can return from immediately because after this point, we'll have an open file and memory allocation that will need to be cleared before we can exit.
    if (file == NULL)
    {
        fprintf(stderr, "fopen failed with error: %s.\n", strerror(errno));
        return FILE_CANT_OPEN;
    }

    AllocateWaveFile(fileInfo, path);
    (*fileInfo)->file = file;

    ReadWaveResult result = FILE_READ_SUCCESS; // Important that we initialize this to success, which is 0.

    // Reading the opening part of the file. This includes the RIFF character code, the file size, and the WAVE character code. After this there's all the subchunks.
    // If we read it successfully we also verify that it has the right values in this header.
    if (fread(&((*fileInfo)->header), sizeof(WaveHeader), 1, file) == 1 && (*fileInfo)->header.chunkHeader.id == FOURCC_RIFF && (*fileInfo)->header.id == FOURCC_WAVE)
    {
        // Verifying that the file size is as described. This has the (much desired) effect of also verifying that the file size doesn't exceed 4GB, which we'll rely on when we use functions like fseek from now on.
        _fseeki64(file, 0, SEEK_END);

        if (_ftelli64(file) - sizeof(ChunkHeader) == (*fileInfo)->header.chunkHeader.size)
        {
            // Now we're gonna search for the chunks listed below. The rest aren't relevant to us.
            // The variables are initialized to 0 because it is a value they can't possibly get otherwise, and it'll help us determine which chunks were found later on.
            DWORD formatChunk = 0, waveformChunk = 0;
            result |= FindImportantChunks(*fileInfo, &formatChunk, &waveformChunk);

            // Now we have the positions of all the relevant chunks and we know about which ones don't exist. Verifying some constraints about chunks that must exist, sometimes under certain conditions.
            if (!ResultHasError(result) && waveformChunk != 0 && formatChunk != 0)
            {
                // Reading all the relevant chunks. For optional chunks, these functions return true when the chunk doesn't exist.
                if (ReadFormatChunk(*fileInfo, formatChunk) && ReadWaveformChunk(*fileInfo, waveformChunk))
                {
                    // By the time we're here, the reading part is mostly done (only thing left is the waveform itself which is not yet needed to be read).
                    // This function is already getting too nested, so the remainder of its work will be done by ValidateFile, which actually checks all the file data to see if it's supported and in line with the specifications.
                    result |= ValidateFile(*fileInfo);
                }
                else
                {
                    fprintf(stderr, "Failed to read one of the important chunks.\n");
                    result = FILE_BAD_WAVE;
                }
            }
        }
        else
        {
            fprintf(stderr, "File size is %lld while it says it should be %lu.\n", _ftelli64(file) - sizeof(ChunkHeader), (*fileInfo)->header.chunkHeader.size);
            result = FILE_BAD_SIZE;
        }
    }
    else
    {
        fprintf(stderr, "File does not have a RIFF or WAVE header or they could not be read.\n");
        result = FILE_NOT_WAVE;
    }

    // Freeing memory if the operation was a failure.
    if (ResultHasError(result))
    {
        CloseWaveFile(*fileInfo);
    }

    return result;
}

ReadWaveResult FindImportantChunks(FileInfo* fileInfo, DWORD* formatChunk, DWORD* waveDataChunk)
{
    // Counters for how many of each chunk we have, because it's a problem if there's more than 1 of anything.
    int formatChunks = 0, waveDataChunks = 0;
    ChunkHeader chunk;
    FILE* file = fileInfo->file;
    ReadWaveResult result = FILE_READ_SUCCESS;
    DWORD iterations = 0;

    // Moving to where we'll start searching from.
    _fseeki64(file, sizeof(WaveHeader), SEEK_SET);

    // We'll keep iterating till the end of the file. We could make it stop once we find all chunks, but I don't think it's so important that we do.
    while (fread(&chunk, sizeof(ChunkHeader), 1, file) == 1)
    {
        // Theoretically, you could devise a file that has looked like a WAVE file until now, but actually has 536,870,910 empty chunks and that's it. Well I'm not falling for that!
        if (iterations++ > MAX_CHUNK_ITERATIONS)
        {
            fprintf(stderr, "Reached max iterations in FindImportantChunks.\n");
            return FILE_BAD_WAVE;
        }

        DWORD chunkPos = (DWORD)(_ftelli64(file) - sizeof(ChunkHeader));

        // Here we use the chunk ID to determine if it's a chunk we're interested in, and storing its position in the file if it is.
        switch (chunk.id)
        {
            case FOURCC_FORMAT:
                *formatChunk = chunkPos;
                formatChunks++;
                break;
            case FOURCC_LIST:
                ; // When the chunk is a list, we want to check if it is a list of wave data. If it is, this is our wave data chunk. Otherwise we ignore it.
                FOURCC listType;

                if (fread(&listType, sizeof(FOURCC), 1, file) != 1 || listType != FOURCC_WAVL)
                {
                    // If we're here then this isn't a wave data list. Backtracking that sizeof(FOURCC) we just read so that the file position will be where the other cases expect it to be.
                    _fseeki64(file, -((__int64)sizeof(FOURCC)), SEEK_CUR);
                    break;
                }

                fileInfo->waveform.isList = TRUE;

            // Note that we don't break from the last case when it's a wave data list, because that case is equivalent to this one.
            case FOURCC_DATA:
                *waveDataChunk = chunkPos;
                waveDataChunks++;
                break;
            // The following two chunks describe how to loop certain segments of the file. I ignore this information, but want to give a warning about it.
            case FOURCC_PLAYLIST:
            case FOURCC_SAMPLER:
                result |= FILE_CHUNK_WARNING;
                break;
            default:
                break;
        }

        // Now to set the file position such that it points to where the next chunk starts.
        // To do this we have to consider that chunks with odd lengths have a padding byte at the end.
        // Note that size == 0 doesn't result in an infinite loop because in each iteration we at least move sizeof(ChunkHeader) when we call fread.
        __int64 nextPosOffset = chunk.size + (chunk.size % 2);
        _fseeki64(file, nextPosOffset, SEEK_CUR);
    }

    // We could have checked these conditions as the counters were getting counted and stop immediately when one of these reaches 2, but this way is cleaner IMO.
    if (formatChunks > 1 || waveDataChunks > 1)
    {
        result = FILE_BAD_WAVE;
    }
    
    return result;
}

char ReadFormatChunk(FileInfo* fileInfo, DWORD chunkOffset)
{
    FILE* file = fileInfo->file;
    _fseeki64(file, chunkOffset, SEEK_SET);
    fread(&(fileInfo->format.header), sizeof(ChunkHeader), 1, file); // Reading chunk header separately because we will need some of its data.
    return fread(&(fileInfo->format.contents), min(sizeof(WAVEFORMATEXTENSIBLE), (size_t)fileInfo->format.header.size), 1, file) == 1;
}

char ReadWaveformChunk(FileInfo* fileInfo, DWORD chunkOffset)
{
    FILE* file = fileInfo->file;
    _fseeki64(file, chunkOffset, SEEK_SET);

    if (fileInfo->waveform.isList)
    {
        // I'd rather have segments in an array than a linked list, which means I have to count how many segments there are first.
        ChunkHeader listHeader, segmentHeader;
        DWORD segmentsLength = 0;
        __int64 currentPos = 0; // This is the offset relative to after the 'wavl' FOURCC that we are in.
        fread(&listHeader, sizeof(ChunkHeader), 1, file); // We don't check that this fread succeeded because it has to, since it succeeded before when we found this chunk.
        _fseeki64(file, sizeof(FOURCC), SEEK_CUR); // Positioning ourselves for where we'll start iterating on chunks from.

        while (currentPos < listHeader.size - sizeof(FOURCC) && fread(&segmentHeader, sizeof(ChunkHeader), 1, file) == 1)
        {
            if (segmentHeader.id == FOURCC_DATA || segmentHeader.id == FOURCC_SILENT)
            {
                // Just to be safe, I make sure the WAVE file isn't some really stupid one with thousands or millions of tiny ass chunks designed to screw with me.
                if (segmentsLength++ > MAX_CHUNK_ITERATIONS)
                {
                    fprintf(stderr, "Reached max iterations in ReadWaveformChunk.\n");
                    return FALSE;
                }
            }
            else // Don't allow waveform segments that aren't recognized.
            {
                fprintf(stderr, "Unsupported data segment detected at offset: %lld.\n", chunkOffset + sizeof(ChunkHeader) + sizeof(FOURCC) + currentPos);
                return FALSE;
            }
            
            // Now to set the file position such that it points to where the next chunk starts.
            // To do this we have to consider that chunks with odd lengths have a padding byte at the end.
            // Note that size == 0 doesn't result in an infinite loop because in each iteration we at least move sizeof(ChunkHeader) when we call fread, and we have a cap on iterations.
            __int64 nextPosOffset = segmentHeader.size + (segmentHeader.size % 2);
            currentPos += sizeof(ChunkHeader) + nextPosOffset;
            _fseeki64(file, nextPosOffset, SEEK_CUR);
        }

        // Gotta have at least one segment, and you gotta end exactly where the list size predicted.
        if (segmentsLength == 0 || currentPos != listHeader.size - sizeof(FOURCC))
        {
            fprintf(stderr, "%s.\n", segmentsLength == 0 ? "No data segments found" : "currentPos didn't end exactly on the list size");
            return FALSE;
        }

        // Now that we've counted the samples, we can allocate the array and fill it with data.
        WaveformSegment* segments = malloc(segmentsLength * sizeof(WaveformSegment));
        DWORD i = 0;
        currentPos = 0;
        fileInfo->waveform.segments = segments; // Assigning this now so that if we return early from an error, there won't be a memory leak.
        _fseeki64(file, chunkOffset + sizeof(ChunkHeader) + sizeof(FOURCC), SEEK_SET);

        // This time we won't play it safe with things like iteration count because we're following the same steps, so we know it's safe.
        while (currentPos < listHeader.size - sizeof(FOURCC) && fread(&segmentHeader, sizeof(ChunkHeader), 1, file) == 1)
        {
            // Slim chance but maybe some fread error or something happened in the last loop and not this one which could cause a buffer overflow.
            if (i >= segmentsLength)
            {
                fprintf(stderr, "More waveform segments were found around the second loop than the first.\n");
                return FALSE;
            }

            segments[i].header = segmentHeader;
            segments[i].relativeOffset = currentPos;

            // Moving to next.
            __int64 nextPosOffset = segments[i].header.size + (segments[i].header.size % 2);
            currentPos += sizeof(ChunkHeader) + nextPosOffset;
            _fseeki64(file, nextPosOffset, SEEK_CUR);
            i++;
        }

        // For the same reason we checked that i is valid inside the loop, maybe this time we had errors and read less.
        if (i != segmentsLength)
        {
            fprintf(stderr, "Fewer waveform segments were found around the second loop than the first. First loop: %lu, second loop: %lu.\n", segmentsLength, i);
            return FALSE;
        }

        fileInfo->waveform.offset = chunkOffset + sizeof(ChunkHeader) + sizeof(FOURCC);
        fileInfo->waveform.segmentsLength = segmentsLength;
    }
    else // File has a single data chunk. This makes our jobs easier.
    {
        WaveformSegment* segments = malloc(sizeof(WaveformSegment));
        fread(&(segments->header), sizeof(ChunkHeader), 1, file);
        segments->relativeOffset = 0;

        fileInfo->waveform.offset = chunkOffset;
        fileInfo->waveform.segmentsLength = 1;
        fileInfo->waveform.segments = segments;
    }

    return TRUE;
}

ReadWaveResult ValidateFile(FileInfo* fileInfo)
{
    ReadWaveResult result = FILE_READ_SUCCESS;

    if (ResultHasError(result |= ValidateFormat(fileInfo)))
    {
        return result;
    }

    if (ResultHasError(result |= ValidateWaveform(fileInfo)))
    {
        return result;
    }

    return result;
}

ReadWaveResult ValidateFormat(FileInfo* fileInfo)
{
    WAVEFORMATEXTENSIBLE* formatExt = &(fileInfo->format.contents); // This shortens the code quite a bit.
    ReadWaveResult result = FILE_READ_SUCCESS;

    // I only support WAVE_FORMAT_PCM or WAVE_FORMAT_EXTENSIBLE with the PCM subtype. In case it's extensible, I ensure some of the fields are set appropriately.
    // I found found some weird files with junk padding at the end of the format chunk, so I only require that the chunk be at least the size it needs to be, not exactly.
    // Though some may think it wise, I think verifying that cbSize is at a correct value of at least 22 is unnecessary since the size check for the entire chunk covers it.
    // A lot of WAVE files tend to have a format chunk size of 16 even though it's obsolete, so I subtract a sizeof(WORD) from the required size to support it.
    if ((formatExt->Format.wFormatTag != WAVE_FORMAT_PCM || fileInfo->format.header.size < sizeof(WAVEFORMATEX) - sizeof(WORD)) &&
        (formatExt->Format.wFormatTag != WAVE_FORMAT_EXTENSIBLE || fileInfo->format.header.size < sizeof(WAVEFORMATEXTENSIBLE) || !IsEqualGUID(&(formatExt->SubFormat), &KSDATAFORMAT_SUBTYPE_PCM)))
    {
        fprintf(stderr, "Unsupported format tag: 0x%hX.\n", formatExt->Format.wFormatTag);
        return FILE_BAD_FORMAT;
    }

    if (formatExt->Format.nSamplesPerSec == 0)
    {
        fprintf(stderr, "File has a frequency of 0.\n");
        return FILE_BAD_FREQUENCY;
    }

    // Now we're gonna check that nBlockAlign is equal to (wBitsPerSample * nChannels) / 8, as per the specifications. I considered ignoring errors in this value, but I think it will be useful later.
    // Important to note that we might miss an error here if this division has a remainder. But that onl happens if wBitsPerSample isn't a multiple of 8, which we'll make sure that it is later.
    // The reason for this ordering is that we check the things that are common for both supported formats before branching to check the remaining stuff separately.
    if (formatExt->Format.nBlockAlign != (formatExt->Format.wBitsPerSample * formatExt->Format.nChannels) / 8)
    {
        fprintf(stderr, "nBlockAlign does not equal to (wBitsPerSample * nChannels) / 8.\n");
        return FILE_BAD_WAVE;
    }

    // nAvgBytesPerSec must be the product of nBlockAlign with nSamplesPerSec.
    if (formatExt->Format.nAvgBytesPerSec != formatExt->Format.nBlockAlign * formatExt->Format.nSamplesPerSec)
    {
        fprintf(stderr, "nAvgBytesPerSec is not the product of nBlockAlign and nSamplesPerSec.\n");
        return FILE_BAD_WAVE;
    }

    // File's gotta have channels. The program supports any number of channels that isn't 0.
    if (formatExt->Format.nChannels == 0)
    {
        fprintf(stderr, "File has 0 channels.\n");
        return FILE_BAD_WAVE;
    }

    // Warning if there's more channels than our program lets you edit.
    if (formatExt->Format.nChannels > MAX_NAMED_CHANNELS)
    {
        result |= FILE_CHAN_WARNING;
    }

    // The remaining specifications vary between PCM and EXTENSIBLE, so here we branch to validate the two formats separately.
    if (formatExt->Format.wFormatTag == WAVE_FORMAT_PCM)
    {
        div_t divResult = div(formatExt->Format.wBitsPerSample, 8);

        // We support only bit-depths which are multiples of 8, and not all of them either.
        // For PCM format, the wBitsPerSample is exactly the bit-depth of a single sample of a single channel, which makes our jobs easy.
        if (!(FILE_MIN_DEPTH <= divResult.quot && divResult.quot <= FILE_MAX_DEPTH) || divResult.rem != 0)
        {
            fprintf(stderr, "File is PCM and has an invalid bit depth of %hu.\n", formatExt->Format.wBitsPerSample);
            return FILE_BAD_BITDEPTH;
        }
    }
    else // It's gotta be extensible if we're here.
    {
        div_t divResult = div(formatExt->Samples.wValidBitsPerSample, 8);

        // We support only bit-depths which are multiples of 8, and not all of them either.
        // For extensible format, the wValidBitsPerSample is exactly the bit-depth of a single sample of a single channel.
        if (!(FILE_MIN_DEPTH <= divResult.quot && divResult.quot <= FILE_MAX_DEPTH) || divResult.rem != 0)
        {
            fprintf(stderr, "File is EXTENSIBLE and has an invalid bit depth of %hu.\n", formatExt->Samples.wValidBitsPerSample);
            return FILE_BAD_BITDEPTH;
        }

        // wBitsPerSample has to be a multiple of 8, and at least as large as the wValidBitsPerSample.
        if (formatExt->Format.wBitsPerSample % 8 != 0 || formatExt->Format.wBitsPerSample < formatExt->Samples.wValidBitsPerSample)
        {
            fprintf(stderr, "wBitsPerSample is not a multiple of 8 or it is smaller than the bit-depth.\n");
            return FILE_BAD_WAVE;
        }
    }

    return result;
}

ReadWaveResult ValidateWaveform(FileInfo* fileInfo)
{
    // Caching the sample length and verifying that it's big enough. We need it to be a power of two which divides by two.
    if ((fileInfo->sampleLength = CountSampleLength(fileInfo)) < 2)
    {
        return FILE_BAD_SAMPLES;
    }

    if (fileInfo->waveform.isList)
    {
        ReadWaveResult result = FILE_READ_SUCCESS;

        // Silent chunks are allowed but ignored, so giving a warning about it.
        for (DWORD i = 0; i < fileInfo->waveform.segmentsLength; i++)
        {
            if (fileInfo->waveform.segments[i].header.id == FOURCC_SILENT)
            {
                result |= FILE_CHUNK_WARNING;
                break;
            }
        }

        return result;
    }
    else
    {
        // Verifying is that the file doesn't end before the chunk does. For lists, we had to verify that while reading them.
        if (fileInfo->waveform.offset + fileInfo->waveform.segments[0].header.size > fileInfo->header.chunkHeader.size)
        {
            fprintf(stderr, "File ends before the data chunk says it should.\n");
            return FILE_BAD_WAVE;
        }

        return FILE_READ_SUCCESS;
    }
}

unsigned long long CountSampleLength(FileInfo* fileInfo)
{
    unsigned long long sum;

    if (fileInfo->waveform.isList)
    {
        sum = 0;
        WaveformSegment* segments = fileInfo->waveform.segments;

        // Summing up the sizes of the waveform segments if the silence chunks were "unpackaged" into a stream of PCM.
        for (DWORD i = 0; i < fileInfo->waveform.segmentsLength; i++)
        {
            // Only counting data chunks, because silence chunks are ignored.
            if (segments[i].header.id == FOURCC_DATA)
            {
                sum += segments[i].header.size;
            }
        }
    }
    else
    {
        sum = fileInfo->waveform.segments[0].header.size;
    }

    // The amount of samples per channel is the data byte size divided by the bytes per block.
    return sum / fileInfo->format.contents.Format.nBlockAlign;
}

// This function doesn't check that inputs are valid, because we assume that they were checked by the calling code.
void CreateNewFile(FileInfo** fileInfo, unsigned int length, unsigned int frequency, unsigned int byteDepth)
{
    // New files are identified by having no string associated with their name yet.
    AllocateWaveFile(fileInfo, NULL);

    unsigned int bitDepth = byteDepth * 8;
    unsigned int dataLength = length * frequency * byteDepth;

    // These are NULL for new files.
    (*fileInfo)->file = NULL;

    // Configuring the wave header.
    // Note: when calculating file size, there's no way that it exceeds 4GB because of the limits we impose on frequency, length, and byte depth. It's not even close.
    (*fileInfo)->header.chunkHeader.id = FOURCC_RIFF;
    (*fileInfo)->header.chunkHeader.size = sizeof(FOURCC) + sizeof(FormatChunk) + sizeof(ChunkHeader) + dataLength + (dataLength % 2);
    (*fileInfo)->header.id = FOURCC_WAVE;

    // Configuring format chunk to a WAVE_FORMAT_EXTENSIBLE chunk with subtype PCM, mono and with parameters as given.
    (*fileInfo)->format.header.id = FOURCC_FORMAT;
    (*fileInfo)->format.header.size = sizeof(WAVEFORMATEXTENSIBLE);

    (*fileInfo)->format.contents.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    (*fileInfo)->format.contents.Format.nChannels = 1;
    (*fileInfo)->format.contents.Format.nSamplesPerSec = frequency;
    (*fileInfo)->format.contents.Format.wBitsPerSample = bitDepth;
    (*fileInfo)->format.contents.Format.nBlockAlign = byteDepth; // Equal to (nChannels * wBitsPerSample) / 8, when nChannels is 1 and wBitsPerSample = byteDepth * 8.
    (*fileInfo)->format.contents.Format.nAvgBytesPerSec = frequency * byteDepth; // Equal to nSamplesPerSec * nBlockAlign.
    (*fileInfo)->format.contents.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);

    (*fileInfo)->format.contents.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
    (*fileInfo)->format.contents.dwChannelMask = SPEAKER_FRONT_CENTER; // I used a WAV editing tool to convert an old school mono file to an EXTENSIBLE mono file, and this is the mask it used.
    (*fileInfo)->format.contents.Samples.wValidBitsPerSample = bitDepth;

    // Configuring the waveform to be a single data chunk that we will later place after the format chunk.
    (*fileInfo)->waveform.offset = sizeof(WaveHeader) + sizeof(FormatChunk);
    (*fileInfo)->waveform.isList = FALSE;
    (*fileInfo)->waveform.segmentsLength = 1;
    (*fileInfo)->waveform.segments = malloc(sizeof(WaveformSegment));
    (*fileInfo)->waveform.segments[0].header.id = FOURCC_DATA;
    (*fileInfo)->waveform.segments[0].header.size = dataLength;
    (*fileInfo)->waveform.segments[0].relativeOffset = 0;

    // Setting the sample length for the file.
    (*fileInfo)->sampleLength = length * frequency;
}

#pragma endregion // Opening.

char LoadPCMInterleaved(FileInfo* fileInfo, Function*** channelsData)
{
    // This macro is basically this entire function, except for all the different byte depths we can have.
    #define LOAD_PCM_INTERLEAVED_TYPED(precision, depth)                                                                                                                                    \
    *channelsData = calloc(relevantChannels, sizeof(Function_##precision##Complex*));                                                                                                       \
                                                                                                                                                                                            \
    for (WORD i = 0; i < relevantChannels; i++)                                                                                                                                             \
    {                                                                                                                                                                                       \
        (*channelsData)[i] = calloc(1, sizeof(Function_##precision##Complex));                                                                                                              \
                                                                                                                                                                                            \
        if (!AllocateFunctionInternals_##precision##Complex((*channelsData)[i], paddedLength / 2))                                                                                          \
        {                                                                                                                                                                                   \
            free(buffer);                                                                                                                                                                   \
            return FALSE;                                                                                                                                                                   \
        }                                                                                                                                                                                   \
    }                                                                                                                                                                                       \
                                                                                                                                                                                            \
    /* This declaration needs to be made inside the macro, but the variable name has to be distinct per macro call to not piss off gcc so we append the depth.*/                            \
    Function_##precision##Complex** funcs##depth = *((Function_##precision##Complex***)channelsData);                                                                                       \
                                                                                                                                                                                            \
    /* Skipping the part where we read from the file if the file is newly created.*/                                                                                                        \
    if (!IsFileNew(fileInfo))                                                                                                                                                               \
    {                                                                                                                                                                                       \
        /* We'll read the data segment by segment.*/                                                                                                                                        \
        for (DWORD i = 0; i < segmentsLength; i++)                                                                                                                                          \
        {                                                                                                                                                                                   \
            /* Silent chunks are ignored.*/                                                                                                                                                 \
            if (segments[i].header.id == FOURCC_DATA)                                                                                                                                       \
            {                                                                                                                                                                               \
                _fseeki64(file, ((__int64)fileInfo->waveform.offset) + segments[i].relativeOffset + sizeof(ChunkHeader), SEEK_SET);                                                         \
                size_t blocksInChunk = segments[i].header.size / blockAlign;                                                                                                                \
                                                                                                                                                                                            \
                /* Reading PCM in chunks of buffer size and converting to complex interleaved samples until everything from this chunk has been read.*/                                     \
                for (size_t blocksRead = 0; blocksRead < blocksInChunk; blocksRead += bufferBlockLen)                                                                                       \
                {                                                                                                                                                                           \
                    size_t currentBlocks = min(bufferBlockLen, blocksInChunk - blocksRead);                                                                                                 \
                    fread(buffer, blockAlign, currentBlocks, file);                                                                                                                         \
                                                                                                                                                                                            \
                    /* Loading the data block by block.*/                                                                                                                                   \
                    for (size_t b = 0; b < currentBlocks; b++)                                                                                                                              \
                    {                                                                                                                                                                       \
                        /* Loading each relevant sample in the block.*/                                                                                                                     \
                        for (WORD c = 0; c < relevantChannels; c++)                                                                                                                         \
                        {                                                                                                                                                                   \
                            /* First, we read the sample in its integer form. All byte depths get read into a 32-bit integer.*/                                                             \
                            /* The upside of this is less repeat code and easier conversion of biased to two's complement for 8-bit PCM.*/                                                  \
                            /* The downside is branching (which should be resolved at compile time) and we have to sign extend everything.*/                                                \
                            int sample = 0;                                                                                                                                                 \
                            memcpy(&sample, buffer + (b * blockAlign) + (c * containerSize), depth);                                                                                        \
                                                                                                                                                                                            \
                            /* 8-bit files use biased representation instead of two's complement. Converting it.*/                                                                          \
                            if (depth == 1)                                                                                                                                                 \
                            {                                                                                                                                                               \
                                sample -= 128;                                                                                                                                              \
                            }                                                                                                                                                               \
                            /* In this part we sign-extend the other int types, except 32-bit because it doesn't need it.*/                                                                 \
                            else if (depth != 4)                                                                                                                                            \
                            {                                                                                                                                                               \
                                if ((DEPTH_HIGH_BIT(depth) & sample) != 0)                                                                                                                  \
                                {                                                                                                                                                           \
                                    sample |= SIGN_EXTEND_MASK(depth);                                                                                                                      \
                                }                                                                                                                                                           \
                            }                                                                                                                                                               \
                                                                                                                                                                                            \
                            /* Now we convert it to a float in the range of -1 to 1.*/                                                                                                      \
                            /* The conversion also balances out the fact that there's more negative than positive numbers in two's complement.*/                                            \
                            precision##Real realSample = ((sample + CAST(0.5, precision##Real)) / (DEPTH_MAX(depth) + CAST(0.5, precision##Real)));                                         \
                                                                                                                                                                                            \
                            /* If this is an even sample, we want to assign it to the real part of get(funcs[c], sampleIndex/2). Otherwise we want to assign it to the imaginary part.*/    \
                            /* To do this, I get the address of the sampleIndex/2'th sample, cast it to its real float type, and then assign one step ahead if the sample index is odd.*/   \
                            /* This works because complex numbers are represented as two adjacent floats in memory, with the real part being first.*/                                       \
                            *(CAST(&get(*(funcs##depth[c]), sampleIndex / 2), precision##Real*) + (sampleIndex % 2)) = realSample;                                                          \
                        }                                                                                                                                                                   \
                                                                                                                                                                                            \
                        sampleIndex++;                                                                                                                                                      \
                    }                                                                                                                                                                       \
                }                                                                                                                                                                           \
            }                                                                                                                                                                               \
        }                                                                                                                                                                                   \
    }                                                                                                                                                                                       \
                                                                                                                                                                                            \
    /* Padding until we reach that power of two length. This also has the job of occupying all the data for new files. We pad what 0 would get converted to, as opposed to 0 itself.*/      \
    /* TODO: potential optimization - use something like memset to add the zero padding more efficiently than going one by one.*/                                                           \
    for (; sampleIndex < paddedLength; sampleIndex++)                                                                                                                                       \
    {                                                                                                                                                                                       \
        for (WORD c = 0; c < relevantChannels; c++)                                                                                                                                         \
        {                                                                                                                                                                                   \
            *(CAST(&get(*(funcs##depth[c]), sampleIndex / 2), precision##Real*) + (sampleIndex % 2)) = CAST(0.5, precision##Real) / (DEPTH_MAX(depth) + CAST(0.5, precision##Real));        \
        }                                                                                                                                                                                   \
    }

    // This is where the function actually starts executing from. The macro needs to be above it.
    FILE* file = fileInfo->file;
    WORD relevantChannels = GetRelevantChannelsCount(fileInfo); // The number of channels we want to load.
    WORD containerSize = fileInfo->format.contents.Format.wBitsPerSample / 8; // The amount of bytes each sample effectively takes up.
    WORD byteDepth = fileInfo->format.contents.Format.wFormatTag == WAVE_FORMAT_PCM ? containerSize : fileInfo->format.contents.Samples.wValidBitsPerSample / 8; // The amount of bytes each sample takes up that isn't padding.
    WORD blockAlign = fileInfo->format.contents.Format.nBlockAlign; // The amount of bytes each block of one sample per channel takes up.
    unsigned long long paddedLength = NextPowerOfTwo(fileInfo->sampleLength); // The sample length of a channel of data, rounded up to the next power of two.

    DWORD segmentsLength = fileInfo->waveform.segmentsLength;
    WaveformSegment* segments = fileInfo->waveform.segments;
    size_t bufferBlockLen = FILE_READING_BUFFER_LEN / blockAlign; // The buffer length is such that this will never be 0.
    void* buffer = malloc(bufferBlockLen * blockAlign); // Buffer size is the largest number less/equal to FILE_READING_BUFFER_LEN that divides by blockAlign.
    unsigned long long sampleIndex = 0; // This is actually sort of double the index. It'll be more clear in the comments inside the macro it's used in.

    if (buffer == NULL)
    {
        return FALSE;
    }

    // To be efficient with memory while not sacrificing precision, byte depths of 1,2 get converted to single precision floats, and 3,4 get converted to double precision.
    switch (byteDepth)
    {
        case 1:
            LOAD_PCM_INTERLEAVED_TYPED(Float, 1)
            break;
        case 2:
            LOAD_PCM_INTERLEAVED_TYPED(Float, 2)
            break;
        case 3:
            LOAD_PCM_INTERLEAVED_TYPED(Double, 3)
            break;
        case 4:
            LOAD_PCM_INTERLEAVED_TYPED(Double, 4)
            break;
        default: // This case should never happen.
            fprintf(stderr, "Somehow the byte depth isn't supported.\n");
            break;
    }

    free(buffer);
    return TRUE;
}

char WriteWaveFile(FILE* file, FileInfo* fileInfo, Function** channelsData)
{
    // This macro does most of this function's work, and generalizes it for different byte depths. Needs to be declared at the top before it's used.
    // This function is quite similar to LoadPCMInterleaved, because it essentially inverses it.*/
    #define WRITE_WAVE_FILE_TYPED(precision, depth)                                                                                                                                 \
    ;Function_##precision##Complex** funcs##depth = (Function_##precision##Complex**)channelsData;                                                                                  \
                                                                                                                                                                                    \
    /* Writing the data segment by segment.*/                                                                                                                                       \
    for (DWORD i = 0; i < segmentsLength; i++)                                                                                                                                      \
    {                                                                                                                                                                               \
        /* Silent chunks are kept as is.*/                                                                                                                                          \
        if (segments[i].header.id == FOURCC_DATA)                                                                                                                                   \
        {                                                                                                                                                                           \
            _fseeki64(file, fileInfo->waveform.offset + segments[i].relativeOffset + sizeof(ChunkHeader), SEEK_SET);                                                                \
            size_t blocksInChunk = segments[i].header.size / blockAlign;                                                                                                            \
                                                                                                                                                                                    \
            /* Writing to the segment in chunks of bufferBlockLen.*/                                                                                                                \
            for (size_t blocksWritten = 0; blocksWritten < blocksInChunk; blocksWritten += bufferBlockLen)                                                                          \
            {                                                                                                                                                                       \
                size_t currentBlocks = min(bufferBlockLen, blocksInChunk - blocksWritten);                                                                                          \
                                                                                                                                                                                    \
                /* In order to preserve the content in channels we do not modify, I have to read what's currently in there first.*/                                                 \
                fread(buffer, blockAlign, currentBlocks, file);                                                                                                                     \
                _fseeki64(file, -blockAlign * currentBlocks, SEEK_CUR); /* Stepping back that read we just did, for writing later.*/                                                \
                                                                                                                                                                                    \
                /* First we have to occupy the buffer with the samples from all the channels in the WAVE formatting.*/                                                              \
                for (size_t b = 0; b < currentBlocks; b++)                                                                                                                          \
                {                                                                                                                                                                   \
                    for (WORD c = 0; c < relevantChannels; c++)                                                                                                                     \
                    {                                                                                                                                                               \
                        /* Taking the float value of the sample which should be roughly in the range [-1, 1].*/                                                                     \
                        precision##Real sample = *(CAST(&get(*(funcs##depth[c]), sampleIndex / 2), precision##Real*) + (sampleIndex % 2));                                          \
                                                                                                                                                                                    \
                        /* Scaling it up so it matches the range of numbers of the given depth.*/                                                                                   \
                        sample = (DEPTH_MAX(depth) * sample) - CAST(0.5, precision##Real);                                                                                          \
                                                                                                                                                                                    \
                        /* Applying triangular dither. The sum of a number between [-1, 0] and a number between [0, 1] has a triangular distribution and is between [-1, 1].*/      \
                        sample += RandRange##precision(-1.0, 0.0) + RandRange##precision(0.0, 1.0);                                                                                 \
                                                                                                                                                                                    \
                        /* Clamping the sample to the range of possible integer values.*/                                                                                           \
                        sample = Clamp##precision(sample, DEPTH_MIN(depth), DEPTH_MAX(depth));                                                                                      \
                                                                                                                                                                                    \
                        /* Rounding the sample to the nearest integer value.*/                                                                                                      \
                        int quantized = lround_##precision(sample);                                                                                                                 \
                                                                                                                                                                                    \
                        /* Converting 8-bit files to unsigned.*/                                                                                                                    \
                        if (depth == 1)                                                                                                                                             \
                        {                                                                                                                                                           \
                            quantized += 128;                                                                                                                                       \
                        }                                                                                                                                                           \
                                                                                                                                                                                    \
                        memcpy(buffer + (b * blockAlign) + (c * containerSize), &quantized, depth);                                                                                 \
                    }                                                                                                                                                               \
                                                                                                                                                                                    \
                    sampleIndex++;                                                                                                                                                  \
                }                                                                                                                                                                   \
                                                                                                                                                                                    \
                /* Writing the data to file.*/                                                                                                                                      \
                fwrite(buffer, blockAlign, currentBlocks, file);                                                                                                                    \
            }                                                                                                                                                                       \
        }                                                                                                                                                                           \
    }

    WORD relevantChannels = GetRelevantChannelsCount(fileInfo); // The number of channels we're editing.
    WORD containerSize = fileInfo->format.contents.Format.wBitsPerSample / 8; // The amount of bytes each sample effectively takes up.
    WORD byteDepth = fileInfo->format.contents.Format.wFormatTag == WAVE_FORMAT_PCM ? containerSize : fileInfo->format.contents.Samples.wValidBitsPerSample / 8; // The amount of bytes each sample takes up that isn't padding.
    WORD blockAlign = fileInfo->format.contents.Format.nBlockAlign; // The amount of bytes each block of one sample per channel takes up.
    
    DWORD segmentsLength = fileInfo->waveform.segmentsLength;
    WaveformSegment* segments = fileInfo->waveform.segments;
    size_t bufferBlockLen = FILE_READING_BUFFER_LEN / blockAlign; // The buffer length is such that this will never be 0.
    void* buffer = malloc(bufferBlockLen * blockAlign); // Buffer size is the largest number less/equal to FILE_READING_BUFFER_LEN that divides by blockAlign.
    unsigned long long sampleIndex = 0; // This is actually sort of double the index. It'll be more clear in the comments inside the macro it's used in.

    if (buffer == NULL)
    {
        return FALSE;
    }

    // To be efficient with memory while not sacrificing precision, byte depths of 1,2 get converted to single precision floats, and 3,4 get converted to double precision.
    switch (byteDepth)
    {
        case 1:
            WRITE_WAVE_FILE_TYPED(Float, 1)
            break;
        case 2:
            WRITE_WAVE_FILE_TYPED(Float, 2)
            break;
        case 3:
            WRITE_WAVE_FILE_TYPED(Double, 3)
            break;
        case 4:
            WRITE_WAVE_FILE_TYPED(Double, 4)
            break;
        default: // This case should never happen.
            fprintf(stderr, "Somehow the byte depth isn't supported.\n");
            break;
    }

    free(buffer);
    fflush(file);
    return TRUE;
}

char WriteWaveFileAs(FileInfo* fileInfo, LPCTSTR path, Function** channelsData)
{
    char success;
    FILE* newFile = _tfsopen(path, TEXT("w+b"), _SH_DENYWR);

    if (newFile == NULL)
    {
        return FALSE;
    }

    // For the sake of simplicity, the way this function works is it readies a file with junk values in the data chunk, and then calls WriteWaveFile which saves changes onto existing files.
    // This isn't the most performant way but it is simple and has no code repetition.
    if (IsFileNew(fileInfo))
    {
        success = WriteNewFile(newFile, fileInfo);
    }
    else
    {
        success = CopyWaveFile(newFile, fileInfo->file);
    }

    // In case of failure to write the new file, reverting to how things were before.
    if (!success || !WriteWaveFile(newFile, fileInfo, channelsData))
    {
        fclose(newFile);
        _tremove(path);
        return FALSE;
    }

    // If the file isn't new, want to free resources from the last file.
    if (!IsFileNew(fileInfo))
    {
        fclose(fileInfo->file);
        free(fileInfo->path);
    }
    
    // Assigning values from the new file into fileInfo.
    fileInfo->file = newFile;
    size_t bufLen = _tcslen(path) + 1;
    fileInfo->path = malloc(sizeof(TCHAR) * bufLen);
    _tcscpy_s(fileInfo->path, bufLen, path);

    return TRUE;
}

char WriteNewFile(FILE* file, FileInfo* fileInfo)
{
    _fseeki64(file, 0, SEEK_SET);
    fwrite(&(fileInfo->header), sizeof(WaveHeader), 1, file);
    fwrite(&(fileInfo->format), sizeof(FormatChunk), 1, file);
    fwrite(&(fileInfo->waveform.segments[0].header), sizeof(ChunkHeader), 1, file);

    // Filling data chunk with junk.
    void* buffer = malloc(FILE_READING_BUFFER_LEN * sizeof(char));
    size_t amountWritten, dataSize = fileInfo->waveform.segments[0].header.size;

    if (buffer == NULL)
    {
        return FALSE;
    }

    for (amountWritten = 0; amountWritten + FILE_READING_BUFFER_LEN < dataSize; amountWritten += FILE_READING_BUFFER_LEN)
    {
        fwrite(buffer, sizeof(char), FILE_READING_BUFFER_LEN, file);
    }

    // The last write might be for less than FILE_READING_BUFFER_LEN so we do it outside the loop.
    fwrite(buffer, sizeof(char), dataSize - amountWritten, file);

    // Adding a padding byte if the data size is odd.
    if (dataSize % 2 == 1)
    {
        fputc(0, file);
    }

    free(buffer);
    return TRUE;
}

char CopyWaveFile(FILE* dest, FILE* src)
{
    _fseeki64(src, 0, SEEK_SET);
    void* buffer = malloc(FILE_READING_BUFFER_LEN * sizeof(char));

    if (buffer == NULL)
    {
        return FALSE;
    }

    while (!feof(src))
    {
        size_t amountRead = fread(buffer, sizeof(char), FILE_READING_BUFFER_LEN, src);
        fwrite(buffer, sizeof(char), amountRead, dest);
    }

    free(buffer);
    return TRUE;
}

char IsFileNew(FileInfo* fileInfo)
{
    return fileInfo != NULL && fileInfo->file == NULL;
}

unsigned short GetChannelNames(FileInfo* fileInfo, TCHAR buffer[][CHANNEL_NAME_BUFFER_LEN])
{
    // This maps channels their names. If a channel corresponds to the i'th bit in the channel mask, then positions[i] holds its name.
    static const TCHAR* positions[] = {TEXT("Front Left"), TEXT("Front Right"), TEXT("Front Center"), TEXT("Low Frequency"), TEXT("Back Left"), TEXT("Back Right"),
        TEXT("Front Left Of Center"), TEXT("Front Right Of Center"), TEXT("Back Center"), TEXT("Side Left"), TEXT("Side Right"), TEXT("Top Center"),
        TEXT("Top Front Left"), TEXT("Top Front Center"), TEXT("Top Front Right"), TEXT("Top Back Left"), TEXT("Top Back Center"), TEXT("Top Back Right")};

    unsigned short relevantChannels = GetRelevantChannelsCount(fileInfo);
    LPCTSTR chanNumPrefix = TEXT("Channel #");
    unsigned int prefixLen = _tcslen(chanNumPrefix); // I think gcc will compute this at compile time.

    DWORD channelMask;

    if (fileInfo->format.contents.Format.wFormatTag == WAVE_FORMAT_PCM)
    {
        channelMask =   relevantChannels == 1 ? SPEAKER_FRONT_CENTER :
                        relevantChannels == 2 ? SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT :
                        0;
    }
    else // WAVE_FORMAT_EXTENSIBLE.
    {
        // Bits that I zero out in this mask are ones that don't map to any particular location in the specifications.
        channelMask = fileInfo->format.contents.dwChannelMask & 0x0003FFFF;
    }

    // This loop occupies the buffer with channel names according to the channel mask and the number of channels.
    for (unsigned long i = 0, mask = 1, pos = 0; i < relevantChannels; i++)
    {
        // Seeking the next set bit. All the bits starting from MAX_NAMED_CHANNELS are 0 so we stop there.
        while (pos < MAX_NAMED_CHANNELS && (mask & channelMask) == 0)
        {
            mask <<= 1;
            pos++;
        }

        // pos indicates what bit is currently on in the mask. If it's less than MAX_NAMED_CHANNELS, the mask must've found a set bit in the channel mask, which means this channel maps to that position.
        if (pos < MAX_NAMED_CHANNELS)
        {
            _tcscpy_s(buffer[i], CHANNEL_NAME_BUFFER_LEN, positions[pos]);

            // Moving the mask so we won't reuse this position.
            mask <<= 1;
            pos++;
        }
        else // If we're here then there are more channels than set bits. In that case we will just number them.
        {
            // Important to use _tcscpy instead of simple assignment in here and all the other places, because the string buffer is already allocated, we want to write where it points to.
            _tcscpy_s(buffer[i], CHANNEL_NAME_BUFFER_LEN, chanNumPrefix);
            _ultot_s(i + 1, buffer[i] + prefixLen, CHANNEL_NAME_BUFFER_LEN - prefixLen, 10);
        }
    }

    return relevantChannels;
}

unsigned short GetRelevantChannelsCount(FileInfo* fileInfo)
{
    return min(fileInfo->format.contents.Format.nChannels, MAX_NAMED_CHANNELS);
}