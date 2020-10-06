#ifndef SOUND_EDITOR_INTERNAL_H
#define SOUND_EDITOR_INTERNAL_H

#include "SoundEditor.h"

#define SOUND_EDITOR_INTERNAL_H_TYPED_CONTENTS(type)																											\
/* Creates a cache of things the sound editor wants to reuse as long as it's dealing with real interleaved functions of the same length as the one given.*/		\
/* Returns NULL in case of failure, which is probably caused by there not being enough memory available.*/														\
SoundEditorCache* InitializeSoundEditor_##type(Function_##type*);																								\
																																								\
/* Swaps each element in the array with its bit-reverse. Assumes function length is a power of two.*/															\
void BitReverseArr_##type(Function_##type, SoundEditorCache*);																									\
																																								\
/* Calculates the DFT of a real sequence in complex interleaved form.*/																							\
void RealInterleavedFFT_##type(Function_##type, SoundEditorCache*);																								\
																																								\
/* Takes half the DFT of a real sequence and calculates the original interleaved sequence.*/																	\
void InverseRealInterleavedFFT_##type(Function_##type, SoundEditorCache*);																						\
																																								\
/* In-place cooley-tukey FFT. Assumes function length is a power of two.*/																						\
void FFT_##type(Function_##type f, SoundEditorCache*);																											\
																																								\
/* In-place inverse FFT. Assumes function length is a power of two.*/																							\
void InverseFFT_##type(Function_##type f, SoundEditorCache*);

// Returns the reverse of a number with respect to a number of digits given.
unsigned long long BitReverse(unsigned int, unsigned long long);

// Deallocates memory for the current modification and its next ones until the end.
void DeallocateModificationsNextwards(Modification*);

// Deallocates memory for the given modification and its previous ones until the end.
void DeallocateModificationsPrevwards(Modification*);

// Deallocates a single modification.
void DeallocateModification(Modification*);

// Applies the given modification on the right function from the channels data.
void ApplyModificationInternal(Function**, Modification*);

SOUND_EDITOR_INTERNAL_H_TYPED_CONTENTS(DoubleComplex)
SOUND_EDITOR_INTERNAL_H_TYPED_CONTENTS(FloatComplex)

#endif