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
#include "MyUtils.h"
#include <tchar.h> // For dealing with strings that may be unicode or ANSI.
#include <ksmedia.h> // For the KSDATAFORMAT_SUBTYPE_PCM macro.
#include <math.h> // For the min macro.
#include <stdlib.h> // For malloc and such.
#include <share.h> // For shflags to _tfsopen.

#define FOURCC_WAVE     mmioFOURCC('W', 'A', 'V', 'E')
#define FOURCC_FORMAT   mmioFOURCC('f', 'm', 't', ' ')
#define FOURCC_WAVL     mmioFOURCC('w', 'a', 'v', 'l')
#define FOURCC_DATA     mmioFOURCC('d' , 'a', 't', 'a')
#define FOURCC_SILENT   mmioFOURCC('s', 'l', 'n', 't')
#define FOURCC_CUE      mmioFOURCC('c', 'u', 'e', ' ')
#define FOURCC_PLAYLIST mmioFOURCC('p', 'l', 's', 't')
#define FOURCC_SAMPLER  mmioFOURCC('s', 'm', 'p', 'l')

#define MAX_CHUNK_ITERATIONS 1 << 16 // Various loops use this to know when to stop in case the WAVE file is really stupid and has a lot of chunks with nothing.

void AllocateWaveFile(FileInfo** fileInfo, LPCTSTR path)
{
    // This won't do anything if a file isn't already open.
    CloseWaveFile(fileInfo);

    *fileInfo = calloc(1, sizeof(FileInfo));

    // Creating copy of path string on heap for long term storage. +1 because of the null character.
    if (path != NULL)
    {
        (*fileInfo)->path = malloc(sizeof(TCHAR) * (_tcslen(path) + 1));
        _tcscpy((*fileInfo)->path, path);
    }
}

void CloseWaveFile(FileInfo** fileInfo)
{
    // Dodging null pointer exceptions like a boss.
    if (!IsFileOpen(*fileInfo))
    {
        return;
    }

    // We're gonna take advantage of the fact that free can take null points and just not do anything in that case.
    free((*fileInfo)->path);
    free((*fileInfo)->waveform.segments);
    
    if ((*fileInfo)->cue != NULL)
    {
        free((*fileInfo)->cue->pointsArr);
        free((*fileInfo)->cue);
    }

    // Unlike free, fclose does require that we check for NULL beforehand.
    if ((*fileInfo)->file != NULL)
    {
        fclose((*fileInfo)->file);
    }

    free(*fileInfo);
    *fileInfo = NULL;
}

ReadWaveResult ReadWaveFile(FileInfo** fileInfo, LPCTSTR path)
{
    // For all errors other than FILE_CANT_OPEN, this would be done when we SetCurrentFile. But for consistency I think it should happen for FILE_CANT_OPEN as well.
    CloseWaveFile(fileInfo);

    // Using _tfsopen rather than _tfopen so we can specify that we want exclusive write access.
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
            DWORD formatChunk = 0, waveformChunk = 0, cueChunk = 0;
            result |= FindImportantChunks(*fileInfo, &formatChunk, &waveformChunk, &cueChunk);

            // Now we have the positions of all the relevant chunks and we know about which ones don't exist. Verifying some constraints about chunks that must exist, sometimes under certain conditions.
            if (!ResultHasError(result) && waveformChunk != 0 && formatChunk != 0)
            {
                // Reading all the relevant chunks. For optional chunks, these functions return true when the chunk doesn't exist.
                if (ReadFormatChunk(*fileInfo, formatChunk) && ReadWaveformChunk(*fileInfo, waveformChunk) && ReadCueChunk(*fileInfo, cueChunk))
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
        CloseWaveFile(fileInfo);
    }

    return result;
}

ReadWaveResult FindImportantChunks(FileInfo* fileInfo, DWORD* formatChunk, DWORD* waveDataChunk, DWORD* cueChunk)
{
    // Counters for how many of each chunk we have, because it's a problem if there's more than 1 of anything.
    int formatChunks = 0, waveDataChunks = 0, cueChunks = 0;
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
            case FOURCC_CUE:
                *cueChunk = chunkPos;
                cueChunks++;
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
    if (formatChunks > 1 || waveDataChunks > 1 || cueChunks > 1)
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

// TODO: since I decided to ignore silence chunks, cue chunks aren't necessary to read anymore. I can delete all the cue-chunk reading code one day.
char ReadCueChunk(FileInfo* fileInfo, DWORD chunkOffset)
{
    if (chunkOffset == 0)
    {
        fileInfo->cue = NULL;
        return TRUE;
    }
    else // Chunk exists. Reading it.
    {
        // Starting with just reading the header and the field which says how many cue points there are.
        CueChunk* cue = malloc(sizeof(CueChunk));
        FILE* file = fileInfo->file;
        fileInfo->cue = cue;
        _fseeki64(file, chunkOffset, SEEK_SET);
        
        // If this read fails there's a problem.
        if (fread(cue, sizeof(CueChunk) - sizeof(CuePoint*), 1, file) != 1)
        {
            fprintf(stderr, "Failed to read cue chunk.\n");
            return FALSE;
        }

        // Ensuring that there's cue points, and also that there isn't so many that it's absurd.
        if (cue->cuePoints == 0 || cue->cuePoints > MAX_CHUNK_ITERATIONS)
        {
            fprintf(stderr, "There are %lu cue points, which is invalid.\n", cue->cuePoints);
            return FALSE;
        }

        // If we're here then the last read was successful. Now using the data we read to read all the cue points.
        CuePoint* pointsArr = malloc(cue->cuePoints * sizeof(CuePoint));
        cue->pointsArr = pointsArr;

        if (fread(pointsArr, sizeof(CuePoint), cue->cuePoints, file) != cue->cuePoints)
        {
            fprintf(stderr, "Failed to read cue points array.\n");
            return FALSE;
        }

        // Sorting the cue points by their ChunkStart will make our lives easier later.
        // Initially I used quicksort here, but then realized the recursion might cause a stack overflow sometimes so I switched to bubblesort.
        Bubblesort(pointsArr, cue->cuePoints, CuePointComparator, sizeof(CuePoint));
        return TRUE;
    }
}

char CuePointComparator(void* p1, void* p2)
{
    return ((CuePoint*)p1)->chunkStart < ((CuePoint*)p2)->chunkStart;
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

    if (ResultHasError(result |= ValidateCue(fileInfo)))
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
        // For PCM format, the BitsPerSample is exactly the bit-depth of a single sample of a single channel, which makes our jobs easy.
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
    // Preventing too many samples in the interest of memory efficiency.
    // TODO: I could manipulate files of 8, 16 bit depths using single precision floats and only use double precision for higher depths. That way I'll be able to raise this limit.
    if ((fileInfo->sampleLength = CountSampleLength(fileInfo)) > FILE_MAX_SAMPLES)
    {
        fprintf(stderr, "File has more samples than the allowed maximum.\n");
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

ReadWaveResult ValidateCue(FileInfo* fileInfo)
{
    // Nothing to verify about non-lists. I mean, we could verify that they point to 0, but there's no reason to as we won't change this later anyway.
    // Cue point is optional so if it's missing, we say it's valid.
    if (!fileInfo->waveform.isList || fileInfo->cue == NULL)
    {
        return FILE_READ_SUCCESS;
    }
    
    DWORD cuePoints = fileInfo->cue->cuePoints;
    DWORD segmentsLength =  fileInfo->waveform.segmentsLength;
    CuePoint* pointsArr = fileInfo->cue->pointsArr;
    WaveformSegment* segments = fileInfo->waveform.segments;

    // For every cue point, we'll check if its chunkStart correspodns to an existing waveform segment.
    for (DWORD i = 0, dataIndex = 0; i < cuePoints; i++)
    {
        // in ReadCueChunk, we sorted the chunks by the chunkStart. So now we can search only for segments from where the last cue point left off.
        while (dataIndex < segmentsLength && pointsArr[i].chunkStart != segments[dataIndex].relativeOffset)
        {
            dataIndex++;
        }

        // If we couldn't find a corresponding waveform segment, it's a failure.
        if (dataIndex == segmentsLength || segments[dataIndex].header.id != pointsArr[i].chunkId)
        {
            fprintf(stderr, "One of the cue points does not correspond to an existing waveform segment.\n");
            return FILE_BAD_WAVE;
        }
    }

    return FILE_READ_SUCCESS;
}

DWORD CountSampleLength(FileInfo* fileInfo)
{
    DWORD sum;

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
    (*fileInfo)->cue = NULL;

    // Configuring the wave header.
    // Note: when calculating file size, there's no way that it exceeds 4GB because of the limits we impose on frequency, length, and byte depth. It's not even close.
    // TODO: not sure about adding dataLength % 2 to the size. The idea is that it would be the padding byte for chunks of odd lengths but is it necessary if it's the last chunk in the file?
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
}

void WriteWaveFile(FileInfo* fileInfo)
{

}

void WriteWaveFileAs(FileInfo* fileInfo, LPCTSTR path)
{

}

char IsFileNew(FileInfo* fileInfo)
{
    return IsFileOpen(fileInfo) && fileInfo->path == NULL;
}

char IsFileOpen(FileInfo* fileInfo)
{
    return fileInfo != NULL;
}

unsigned int GetChannelNames(FileInfo* fileInfo, TCHAR buffer[][24])
{
    // This maps channels their names. If a channel corresponds to the i'th bit in the channel mask, then positions[i] holds its name.
    static const TCHAR* positions[] = {TEXT("Front Left"), TEXT("Front Right"), TEXT("Front Center"), TEXT("Low Frequency"), TEXT("Back Left"), TEXT("Back Right"),
        TEXT("Front Left Of Center"), TEXT("Front Right Of Center"), TEXT("Back Center"), TEXT("Side Left"), TEXT("Side Right"), TEXT("Top Center"),
        TEXT("Top Front Left"), TEXT("Top Front Center"), TEXT("Top Front Right"), TEXT("Top Back Left"), TEXT("Top Back Center"), TEXT("Top Back Right")};

    WAVEFORMATEXTENSIBLE* formatExt = &(fileInfo->format.contents);
    unsigned int nChannels = min(formatExt->Format.nChannels, MAX_NAMED_CHANNELS);

    LPCTSTR chanNumPrefix = TEXT("Channel #");
    unsigned int prefixLen = _tcslen(chanNumPrefix); // I think gcc will compute this at compile time.

    DWORD channelMask;

    if (formatExt->Format.wFormatTag == WAVE_FORMAT_PCM)
    {
        channelMask =   nChannels == 1 ? SPEAKER_FRONT_CENTER :
                        nChannels == 2 ? SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT :
                        0;
    }
    else // WAVE_FORMAT_EXTENSIBLE.
    {
        // Bits that I zero out in this mask are ones that don't map to any particular location in the specifications.
        channelMask = formatExt->dwChannelMask & 0x0003FFFF;
    }

    // This loop occupies the buffer with channel names according to the channel mask and the number of channels.
    for (unsigned long i = 0, mask = 1, pos = 0; i < nChannels; i++)
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
            _tcscpy(buffer[i], positions[pos]);

            // Moving the mask so we won't reuse this position.
            mask <<= 1;
            pos++;
        }
        else // If we're here then there are more channels than set bits. In that case we will just number them.
        {
            // Important to use _tcscpy instead of simple assignment in here and all the other places, because the string buffer is already allocated, we want to write where it points to.
            _tcscpy(buffer[i], chanNumPrefix);
            _ultot(i + 1, buffer[i] + prefixLen, 10);
        }
    }

    return nChannels;
}