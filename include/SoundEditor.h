#ifndef SOUND_EDITOR_H
#define SOUND_EDITOR_H

// Linked list where each link contains an array of samples and some metadata.
typedef struct AudioSegment
{
    double* pcm;
    unsigned int samples;
    unsigned int loops;
    // TODO: store a flag whether this segment is silent and maintain it as I modify the segment or should I just check if it's silent when I get to encoding it?
    struct AudioSegment* next;
} AudioSegment;

typedef struct AudioInfo
{
    AudioSegment* audio;
    double* fourier;
} AudioInfo;

char HasUnsavedProgress();

#endif