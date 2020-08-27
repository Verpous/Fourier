#ifndef SOUND_EDITOR_H
#define SOUND_EDITOR_H

#include "SampledFunction.h"

// Returns nonzero value iff there are changes that have not yet been saved.
char HasUnsavedProgress();

void RealInterleavedFFT(Function*);

void InverseRealInterleavedFFT(Function*);

#endif