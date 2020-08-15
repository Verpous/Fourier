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

#include "FileManager.h"
#include "SoundEditor.h"
#include "MyUtils.h"
#include <tchar.h>
#include <ksmedia.h>
#include <mmreg.h>
#include <math.h>
#include <stdlib.h>

#define FOURCC_WAVE mmioFOURCC('W', 'A', 'V', 'E')
#define FOURCC_FORMAT mmioFOURCC('f', 'm', 't', ' ')
#define FOURCC_WAVL mmioFOURCC('w', 'a', 'v', 'l')
#define FOURCC_DATA mmioFOURCC('d' , 'a', 't', 'a')
#define FOURCC_SILENT mmioFOURCC('s', 'l', 'n', 't')
#define FOURCC_CUE mmioFOURCC('c', 'u', 'e', ' ')
#define FOURCC_PLAYLIST mmioFOURCC('p', 'l', 's', 't')
#define FOURCC_SAMPLER mmioFOURCC('s', 'm', 'p', 'l')

#define MAX_CHUNK_ITERATIONS 1 << 16

static FileInfo* fileInfo = NULL;

void CreateFileInfo(LPCTSTR path)
{
    fileInfo = calloc(1, sizeof(FileInfo));

    // Creating copy of path string on heap for long term storage. +1 because of the null character.
    fileInfo->path = malloc(sizeof(TCHAR) * (_tcslen(path) + 1));
    fileInfo->waveform.isList = FALSE;
    _tcscpy(fileInfo->path, path);
}

void DestroyFileInfo()
{
    // Dodging null pointer exceptions like a boss.
    if (fileInfo == NULL)
    {
        return;
    }

    // We're gonna take advantage of the fact that free can take null points and just not do anything in that case.
    free(fileInfo->path);
    free(fileInfo->waveform.segments);
    
    if (fileInfo->cue != NULL)
    {
        free(fileInfo->cue->pointsArr);
        free(fileInfo->cue);
    }

    // Unlike free, fclose does require that we check for NULL beforehand.
    if (fileInfo->file != NULL)
    {
        fclose(fileInfo->file);
    }

    free(fileInfo);
    fileInfo = NULL;
}

ReadWaveResult ReadWaveFile(LPCTSTR path)
{
    if (fileInfo != NULL)
    {
        DestroyFileInfo();
    }

    FILE* file = _tfopen(path, TEXT("r+b"));

    // This is the only error that we can return from immediately because after this point, we'll have an open file and memory allocation that will need to be cleared before we can exit.
    if (file == NULL)
    {
        return FILE_CANT_OPEN;
    }

    CreateFileInfo(path);
    fileInfo->file = file;

    WaveHeader waveHeader;
    ReadWaveResult result = FILE_READ_SUCCESS; // Important that we initialize this to success, which is 0.

    // Reading the opening part of the file. This includes the RIFF character code, the file size, and the WAVE character code. After this there's all the subchunks.
    // If we read it successfully we also verify that it has the right values in this header.
    if (fread(&waveHeader, sizeof(WaveHeader), 1, file) == 1 && waveHeader.chunkHeader.id == FOURCC_RIFF && waveHeader.id == mmioFOURCC('W', 'A', 'V', 'E'))
    {
        // Verifying that the file size is as described. This has the (much desired) effect of also verifying that the file size doesn't exceed 4GB, which we'll rely on when we use functions like fseek from now on.
        _fseeki64(file, 0, SEEK_END);

        if (_ftelli64(file) == (__int64)waveHeader.chunkHeader.size)
        {
            // Now we're gonna search for the chunks listed below. The rest aren't relevant to us.
            // The variables are initialized to 0 because it is a value they can't possibly get otherwise, and it'll help us determine which chunks were found later on.
            DWORD formatChunk = 0, waveformChunk = 0, cueChunk = 0;
            result |= FindImportantChunks(&formatChunk, &waveformChunk, &cueChunk);

            // Now we have the positions of all the relevant chunks and we know about which ones don't exist. Verifying some constraints about chunks that must exist, sometimes under certain conditions.
            if (ReadWaveResultCode(result) == FILE_READ_SUCCESS && waveformChunk != 0 && formatChunk != 0)
            {
                // Reading all the relevant chunks. For optional chunks, these functions return true when the chunk doesn't exist.
                if (ReadFormatChunk(formatChunk) && ReadWaveformChunk(waveformChunk) && ReadCueChunk(cueChunk))
                {
                    // By the time we're here, the reading part is mostly done (only thing left is the waveform itself which is not yet needed to be read).
                    // This function is already getting too nested, so the remainder of its work will be done by ValidateFile, which actually checks all the file data to see if it's supported and in line with the specifications.
                    result |= ValidateFile();
                }
                else
                {
                    result |= FILE_BAD_WAVE;
                }
            }
        }
        else
        {
            result |= FILE_BAD_SIZE;
        }
    }
    else
    {
        result |= FILE_NOT_WAVE;
    }

    // Freeing memory if the operation was a failure.
    if (ReadWaveResultCode(result) != FILE_READ_SUCCESS)
    {
        DestroyFileInfo();
    }

    return result;
}

ReadWaveResult FindImportantChunks(DWORD* formatChunk, DWORD* waveDataChunk, DWORD* cueChunk)
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
                result |= FILE_READ_WARNING;
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
    result |= (formatChunks > 1 || waveDataChunks > 1 || cueChunks > 1) ? FILE_BAD_WAVE : FILE_READ_SUCCESS;
    return result;
}

char ReadFormatChunk(DWORD chunkOffset)
{
    FILE* file = fileInfo->file;
    _fseeki64(file, chunkOffset, SEEK_SET);
    fread(&(fileInfo->format.header), sizeof(ChunkHeader), 1, file); // Reading chunk header separately because we will need some of its data.
    return fread(&(fileInfo->format.contents), min(sizeof(WAVEFORMATEXTENSIBLE), (size_t)fileInfo->format.header.size), 1, file) == 1;
}

char ReadWaveformChunk(DWORD chunkOffset)
{
    FILE* file = fileInfo->file;
    _fseeki64(file, chunkOffset, SEEK_SET);

    if (fileInfo->waveform.isList)
    {
        // I'd rather have segments in an array than a linked list, which means I have to count how many segments there are first.
        ChunkHeader listHeader, segmentHeader;
        DWORD segmentsLength = 0, iterations = 0;
        __int64 currentPos = 0; // This is the offset relative to after the 'wavl' FOURCC that we are in.
        fread(&listHeader, sizeof(ChunkHeader), 1, file); // We don't check that this fread succeeded because it has to, since it succeeded before when we found this chunk.
        _fseeki64(file, sizeof(FOURCC), SEEK_CUR); // Positioning ourselves for where we'll start iterating on chunks from.

        while (currentPos < listHeader.size - sizeof(FOURCC) && fread(&segmentHeader, sizeof(ChunkHeader), 1, file) == 1)
        {
            // Just to be safe, I make sure the WAVE file isn't some really stupid one with thousands or millions of tiny ass chunks designed to screw with me.
            if (iterations++ > MAX_CHUNK_ITERATIONS)
            {
                return FALSE;
            }

            // Even though I don't think it's possible, I stay safe from unsupported chunks even inside this list.
            if (segmentHeader.id == FOURCC_DATA || segmentHeader.id == FOURCC_SILENT)
            {
                segmentsLength++;
            }
            
            // Now to set the file position such that it points to where the next chunk starts.
            // To do this we have to consider that chunks with odd lengths have a padding byte at the end.
            // Note that size == 0 doesn't result in an infinite loop because in each iteration we at least move sizeof(ChunkHeader) when we call fread, and we have a cap on iterations.
            __int64 nextPosOffset = segmentHeader.size + (segmentHeader.size % 2);
            currentPos += sizeof(ChunkHeader) + nextPosOffset;
            _fseeki64(file, nextPosOffset, SEEK_CUR);
        }

        // Gotta have at least one segment.
        if (segmentsLength == 0)
        {
            return FALSE;
        }

        // Now that we've counted the samples, we can allocate the array and fill it with data.
        WaveformSegment* segments = malloc(segmentsLength * sizeof(WaveformSegment));
        DWORD i = 0;
        currentPos = 0;
        _fseeki64(file, chunkOffset + sizeof(ChunkHeader) + sizeof(FOURCC), SEEK_SET);

        // This time we won't play it safe with things like iteration count because we're following the same steps, so we know it's safe.
        while (currentPos < listHeader.size - sizeof(FOURCC) && fread(&segmentHeader, sizeof(ChunkHeader), 1, file) == 1)
        {
            // Even though I don't think it's possible, I stay safe from unsupported chunks even inside this list.
            if (segmentHeader.id == FOURCC_DATA || segmentHeader.id == FOURCC_SILENT)
            {
                segments[i].header = segmentHeader;
                segments[i].relativeOffset = currentPos;
                i++;
            }

            // Moving to next.
            __int64 nextPosOffset = segments[i].header.size + (segments[i].header.size % 2);
            currentPos += sizeof(ChunkHeader) + nextPosOffset;
            _fseeki64(file, nextPosOffset, SEEK_CUR);
        }

        fileInfo->waveform.offset = chunkOffset + sizeof(ChunkHeader) + sizeof(FOURCC);
        fileInfo->waveform.segmentsLength = segmentsLength;
        fileInfo->waveform.segments = segments;
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

char ReadCueChunk(DWORD chunkOffset)
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
            return FALSE;
        }

        // Ensuring that there's cue points, and also that there isn't so many that it's absurd.
        if (cue->cuePoints == 0 || cue->cuePoints > MAX_CHUNK_ITERATIONS)
        {
            return FALSE;
        }

        // If we're here then the last read was successful. Now using the data we read to read all the cue points.
        CuePoint* pointsArr = malloc(cue->cuePoints * sizeof(CuePoint));
        cue->pointsArr = pointsArr;

        if (fread(pointsArr, sizeof(CuePoint), cue->cuePoints, file) != cue->cuePoints)
        {
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

ReadWaveResult ValidateFile()
{
    ReadWaveResult result = FILE_READ_SUCCESS;

    if (ReadWaveResultCode(result |= ValidateFormat()) != FILE_READ_SUCCESS)
    {
        return result;
    }

    if (ReadWaveResultCode(result |= ValidateWaveform()) != FILE_READ_SUCCESS)
    {
        return result;
    }

    if (ReadWaveResultCode(result |= ValidateCue()) != FILE_READ_SUCCESS)
    {
        return result;
    }
    
    return result;
}

ReadWaveResult ValidateFormat()
{
    WAVEFORMATEXTENSIBLE* formatExt = &(fileInfo->format.contents); // This shortens the code quite a bit.

    // I only support WAVE_FORMAT_PCM or WAVE_FORMAT_EXTENSIBLE with the PCM subtype. In case it's extensible, I ensure some of the fields are set appropriately.
    // I found found some weird files with junk padding at the end of the format chunk, so I only require that the chunk be at least the size it needs to be, not exactly.
    // Though some may think it wise, I think verifying that cbSize is at a correct value of at least 22 is unnecessary since the size check for the entire chunk covers it.
    if (!((formatExt->Format.wFormatTag == WAVE_FORMAT_PCM && fileInfo->format.header.size >= sizeof(WAVEFORMATEX)) ||
        (formatExt->Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE && fileInfo->format.header.size >= sizeof(WAVEFORMATEXTENSIBLE) && IsEqualGUID(&(formatExt->SubFormat), &KSDATAFORMAT_SUBTYPE_PCM))))
    {
        return FILE_BAD_FORMAT;
    }

    // Now that we've verified the format, we let's check the sample rate!
    // In the future I might just not even impose any limit on the sample rate. I don't know if there's any reason to. But for now this is here. // TODO: decide on this.
    if (!(FILE_MIN_FREQUENCY <= formatExt->Format.nSamplesPerSec && formatExt->Format.nSamplesPerSec <= FILE_MAX_FREQUENCY))
    {
        return FILE_BAD_FREQUENCY;
    }

    // Now we're gonna check that nBlockAlign is equal to (wBitsPerSample * nChannels) / 8, as per the specifications. I considered ignoring errors in this value, but I think it will be useful later.
    // Important to note that we might miss an error here if this division has a remainder. But that onl happens if wBitsPerSample isn't a multiple of 8, which we'll make sure that it is later.
    // The reason for this ordering is that we check the things that are common for both supported formats before branching to check the remaining stuff separately.
    if (formatExt->Format.nBlockAlign != (formatExt->Format.wBitsPerSample * formatExt->Format.nChannels) / 8)
    {
        return FILE_BAD_WAVE;
    }

    // nAvgBytesPerSec must be the product of nBlockAlign with nSamplesPerSec.
    if (formatExt->Format.nAvgBytesPerSec != formatExt->Format.nBlockAlign * formatExt->Format.nSamplesPerSec)
    {
        return FILE_BAD_WAVE;
    }

    // File's gotta have channels. The program supports any number of channels that isn't 0.
    if (formatExt->Format.nChannels == 0)
    {
        return FILE_BAD_WAVE;
    }

    // The remaining specifications vary between PCM and EXTENSIBLE, so here we branch to validate the two formats separately.
    if (formatExt->Format.wFormatTag == WAVE_FORMAT_PCM)
    {
        div_t divResult = div(formatExt->Format.wBitsPerSample, 8);

        // We support only bit-depths which are multiples of 8, and not all of them either.
        // For PCM format, the BitsPerSample is exactly the bit-depth of a single sample of a single channel, which makes our jobs easy.
        if (!(FILE_MIN_DEPTH <= divResult.quot && divResult.quot <= FILE_MAX_DEPTH) || divResult.rem != 0)
        {
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
            return FILE_BAD_BITDEPTH;
        }

        // wBitsPerSample has to be a multiple of 8, and at least as large as the wValidBitsPerSample.
        if (formatExt->Format.wBitsPerSample % 8 != 0 || formatExt->Format.wBitsPerSample < formatExt->Samples.wValidBitsPerSample)
        {
            return FILE_BAD_WAVE;
        }
    }

    return FILE_READ_SUCCESS;
}

ReadWaveResult ValidateWaveform()
{
    return FILE_READ_SUCCESS;
}

ReadWaveResult ValidateCue()
{
    // Nothing to verify about non-lists. I mean, we could verify that they point to 0, but there's no reason to as we won't change this later anyway.
    if (!fileInfo->waveform.isList)
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
            return FILE_BAD_WAVE;
        }
    }

    return FILE_READ_SUCCESS;
}

void WriteWaveFile()
{

}

void WriteNewWaveFile(LPCTSTR path)
{

}

char IsFileNew()
{
    return fileInfo != NULL && fileInfo->path == NULL;
}