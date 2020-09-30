#ifndef SOUND_EDITOR_INTERNAL_H
#define SOUND_EDITOR_INTERNAL_H

#include "SoundEditor.h"

// Deallocates memory for the current modification and its next ones until the end.
void DeallocateModificationsNextwards(Modification*);

// Deallocates memory for the given modification and its previous ones until the end.
void DeallocateModificationsPrevwards(Modification*);

// Deallocates a single modification.
void DeallocateModification(Modification*);

// Applies the given modification on the right function from the channels data.
void ApplyModificationInternal(Function**, Modification*);

// Returns the reverse of a number with respect to a number of digits given.
unsigned long long BitReverse(unsigned int, unsigned long long);

#define SOUND_EDITOR_INTERNAL_H_TYPED_CONTENTS(type)																											\
																																								\
/* Swaps each element in the array with its bit-reverse. Assumes function length is a power of two.*/															\
void BitReverseArr_##type(Function_##type f);																													\
																																								\
/* Calculates the DFT of a real sequence in complex interleaved form.*/																							\
void RealInterleavedFFT_##type(Function_##type);																												\
																																								\
/* Takes half the DFT of a real sequence and calculates the original interleaved sequence.*/																	\
void InverseRealInterleavedFFT_##type(Function_##type);																											\
																																								\
/* In-place cooley-tukey FFT. Assumes function length is a power of two.*/																						\
void FFT_##type(Function_##type f);																																\
																																								\
/* In-place inverse FFT. Assumes function length is a power of two.*/																							\
void InverseFFT_##type(Function_##type f);

#define SOUND_EDITOR_INTERNAL_H_PRECISION_CONTENTS(precision)																									\
																																								\
/* Calculates and returns the k'th twiddle factor for a length N DFT.*/																							\
/* TODO: mark as inline.*/																																		\
precision##Complex RootOfUnity_##precision##Complex(unsigned long long, precision##Real);

SOUND_EDITOR_INTERNAL_H_TYPED_CONTENTS(DoubleComplex)
SOUND_EDITOR_INTERNAL_H_TYPED_CONTENTS(FloatComplex)

SOUND_EDITOR_INTERNAL_H_PRECISION_CONTENTS(Double)
SOUND_EDITOR_INTERNAL_H_PRECISION_CONTENTS(Float)

#endif