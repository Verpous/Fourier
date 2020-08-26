#ifndef SOUND_EDITOR_INTERNAL_H
#define SOUND_EDITOR_INTERNAL_H

#include "SoundEditor.h"

// Calculates log2 of a number assuming it is a power of two.
unsigned int Log2(unsigned long long);

// Returns the reverse of a number with respect to a number of digits given.
unsigned long long BitReverse(unsigned int, unsigned long long);

#define SOUND_EDITOR_INTERNAL_H_TYPED_CONTENTS(type)                                                                                                             \
                                                                                                                                                        \
/* Swaps each element in the array with its bit-reverse. Assumes function length is a power of two.*/                                                   \
void BitReverseArr_##type(Function_##type f);                                                                                                           \
                                                                                                                                                        \
/* As described in this document: https://www.ti.com/lit/an/spra291/spra291.pdf?ts=1597858546752&ref_url=https%253A%252F%252Fwww.google.co.il%252F*/    \
/* We treat f as if it's already a complex sequence where the real parts correspond to even indices of g, and the imaginary parts correspond to odds*/  \
/* This function just applies the FFT and then the postprocessing to extract the DFT of g from it.*/                                                    \
void RealInterleavedFFT_##type(Function_##type);                                                                                                        \
                                                                                                                                                        \
/* Takes half the DFT of a real sequence and calculates the original interleaved sequence described above.*/                                            \
void InverseRealInterleavedFFT_##type(Function_##type);                                                                                                 \
                                                                                                                                                        \
/* In-place cooley-tukey FFT. Assumes function length is a power of two.*/                                                                              \
void FFT_##type(Function_##type f);                                                                                                                     \
                                                                                                                                                        \
/* In-place inverse FFT. Assumes function length is a power of two.*/                                                                                   \
void InverseFFT_##type(Function_##type f);

SOUND_EDITOR_INTERNAL_H_TYPED_CONTENTS(DoubleComplex)
SOUND_EDITOR_INTERNAL_H_TYPED_CONTENTS(FloatComplex)

#endif