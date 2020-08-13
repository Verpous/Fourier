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
#include <tchar.h>
#include <ksmedia.h>

static FileInfo* fileInfo = NULL;

void CreateFileInfo(LPCTSTR path)
{
    fileInfo = calloc(1, sizeof(FileInfo));

    // Creating copy of path string on heap for long term storage. +1 because of the null character.
    fileInfo->path = malloc(sizeof(TCHAR) * (_tcslen(path) + 1));
    _tcscpy(fileInfo->path, path);

    fileInfo->formatExtens.Format.cbSize = 22; // This value must be set to at least 22 (and in our case exactly 22) when using the WAVEFORMATEXTENSIBLE struct.
    //fileInfo->formatExtens.SubFormat = KSDATAFORMAT_SUBTYPE_PCM; // This must also be set for the format we support. It probably doesn't matter, but whatever.
}

void DestroyFileInfo()
{
    if (fileInfo->path != NULL)
    {
        free(fileInfo->path);
    }

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
    ReadWaveResult result;

    // Reading the opening part of the file. This includes the RIFF character code, the file size, and the WAVE character code. After this there's all the subchunks.
    if (fread(&waveHeader, sizeof(WaveHeader), 1, file) == 1)
    {
        // Checking that the file is a RIFF file and a WAVE file.
        if (waveHeader.chunkHeader.id == FOURCC_RIFF && waveHeader.id == mmioFOURCC('W', 'A', 'V', 'E'))
        {
            // Now we're gonna search for the chunks listed below. The rest aren't relevant to us.
            // The variables are initialized to 0 because it is a value they can't possibly get otherwise, and it'll help us determine which chunks were found later on.
            size_t formatChunk = 0, waveDataChunk = 0, factChunk = 0, cueChunk = 0, playlistChunk = 0;
            char waveDataIsList = FALSE;
            char chunksValid = FindImportantChunks(&formatChunk, &waveDataChunk, &factChunk, &cueChunk, &playlistChunk, &waveDataIsList);

            // Now we have the positions of all the relevant chunks and we know about which ones don't exist. Verifying some constraints about chunks that must exist, sometimes under certain conditions.
            if (chunksValid && waveDataChunk != 0 && formatChunk != 0 && (factChunk != 0 || !waveDataIsList) && (playlistChunk == 0 || cueChunk != 0))
            {
                // TODO: read data from the various chunks and do stuff with it.
                
            }
            else
            {
                result = FILE_BAD_WAVE;
            }
        }
        else
        {
            result = FILE_NOT_WAVE;
        }
    }
    else
    {
        result = FILE_MISC_ERROR;
    }

    // Freeing memory if the operation was a failure.
    if (result != FILE_READ_SUCCESS)
    {
        DestroyFileInfo();
    }

    return result;
}

char FindImportantChunks(size_t* formatChunk, size_t* waveDataChunk, size_t* factChunk, size_t* cueChunk, size_t* playlistChunk, char* waveDataIsList)
{
    // Counters for how many of each chunk we have, because it's a problem if there's more than 1 of anything.
    int formatChunks = 0, waveDataChunks = 0, factChunks = 0, cueChunks = 0, playlistChunks = 0;
    ChunkHeader chunk;

    // We'll keep iterating till the end of the file. We could make it stop once we find all chunks, but I don't think it's so important that we do.
    while (fread(&chunk, sizeof(ChunkHeader), 1, fileInfo->file) == 1)
    {
        size_t chunkPos = ftell(fileInfo->file) - sizeof(ChunkHeader);

        // Here we use the chunk ID to determine if it's a chunk we're interested in, and storing its position in the file if it is.
        switch (chunk.id)
        {
            case mmioFOURCC('f', 'm', 't', ' '):
                *formatChunk = chunkPos;
                formatChunks++;
                break;
            case mmioFOURCC('L', 'I', 'S', 'T'):
                ; // When the chunk is a list, we want to check if it is a list of wave data. If it is, this is our wave data chunk. Otherwise we ignore it.
                FOURCC listType;

                if (fread(&listType, sizeof(FOURCC), 1, fileInfo->file) != 1 || listType != mmioFOURCC('w', 'a', 'v', 'l'))
                {
                    // If we're here then this isn't a wave data list. Backtracking that sizeof(FOURCC) we just read so that the file position will be where the other cases expect it to be.
                    fseek(fileInfo->file, -((long)sizeof(FOURCC)), SEEK_CUR);
                    break;
                }

                *waveDataIsList = TRUE;

            // Note that we don't break from the last case when it's a wave data list, because that case is equivalent to this one.
            case mmioFOURCC('d' , 'a', 't', 'a'):
                *waveDataChunk = chunkPos;
                waveDataChunks++;
                break;
            case mmioFOURCC('f', 'a', 'c', 't'):
                *factChunk = chunkPos;
                factChunks++;
                break;
            case mmioFOURCC('c', 'u', 'e', ' '):
                *cueChunk = chunkPos;
                cueChunks++;
                break;
            case mmioFOURCC('p', 'l', 's', 't'):
                *playlistChunk = chunkPos;
                playlistChunks++;
                break;
        }

        // Now to set the file position such that it points to where the next chunk starts.
        // To do this we have to consider that chunks with odd lengths have a padding byte at the end.
        long nextPosOffset = chunk.size + (chunk.size % 2);
        fseek(fileInfo->file, nextPosOffset, SEEK_CUR);
    }

    return formatChunks <= 1 && waveDataChunks <= 1 && factChunks <= 1 && cueChunks <= 1 && playlistChunks <= 1;
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