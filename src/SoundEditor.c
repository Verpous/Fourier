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

// This is required in order to get constants like M_PI.
#define _USE_MATH_DEFINES

#include "SoundEditorInternal.h"
#include "MyUtils.h"
#include <complex.h> // For dealing with complex numbers.
#include <math.h> // For min.
#include <limits.h> // For CHAR_BIT.
#include <stdio.h> // For fprintf.

#define cexp_DoubleComplex(x) cexp(x)
#define cexp_FloatComplex(x) cexpf(x)

#define conj_DoubleComplex(x) conj(x)
#define conj_FloatComplex(x) conjf(x)

#define RootOfUnity_DoubleComplex(k, N) cexp_DoubleComplex((CAST(-2.0 * M_PI, DoubleComplex)  * I * (k)) / (N))
#define RootOfUnity_FloatComplex(k, N) cexp_FloatComplex((CAST(-2.0 * M_PI, FloatComplex)  * I * (k)) / (N))

char HasUnsavedProgress()
{
    return 0;
}

void RealInterleavedFFT(Function* f)
{
    switch (GetType(f))
    {
        case FloatComplexType:
            RealInterleavedFFT_FloatComplex(*((Function_FloatComplex*)f));
            break;
        case DoubleComplexType:
            RealInterleavedFFT_DoubleComplex(*((Function_DoubleComplex*)f));
            break;
        default:
            fprintf(stderr, "Tried to FFT an invalid type.\n");
            break;
    }
}

unsigned long long BitReverse(unsigned int digits, unsigned long long n)
{
    int i;
    unsigned long long reversed = 0, mask = 1;

    // First iterating on the left half of the number.
    for (i = 0; (2 * i) + 1 < digits; i++, mask <<= 1)
    {
        unsigned long long bit = mask & n;
        bit <<= digits - 1 - i;
        reversed |= bit;
    }

    // Now iterating on the right half.
    for (; i < digits; i++, mask <<= 1)
    {
        unsigned long long bit = mask & n;
        bit >>= digits - 1 - i;
        reversed |= bit;
    }

    return reversed;
}

#define SOUND_EDITOR_C_TYPED_CONTENTS(type)                                                                                                             \
                                                                                                                                                        \
void BitReverseArr_##type(Function_##type f)                                                                                                            \
{                                                                                                                                                       \
    unsigned long long len = NumOfSamples_##type(&f);                                                                                                   \
    unsigned int digits = CountTrailingZeroes(len);                                                                                                     \
                                                                                                                                                        \
    for (unsigned long long i = 0; i < len; i++)                                                                                                        \
    {                                                                                                                                                   \
        unsigned long long reversed = BitReverse(digits, i);                                                                                            \
                                                                                                                                                        \
        /* Avoiding reversing the same thing twice.*/                                                                                                   \
        if (reversed > i)                                                                                                                               \
        {                                                                                                                                               \
            double complex temp = get(f, i);                                                                                                            \
            get(f, i) = get(f, reversed);                                                                                                               \
            get(f, reversed) = temp;                                                                                                                    \
        }                                                                                                                                               \
    }                                                                                                                                                   \
}                                                                                                                                                       \
                                                                                                                                                        \
void RealInterleavedFFT_##type(Function_##type f)                                                                                                       \
{                                                                                                                                                       \
    unsigned long long len = NumOfSamples_##type(&f);                                                                                                   \
    FFT_##type(f);                                                                                                                                      \
                                                                                                                                                        \
    /* Applying postprocessing to extract the DFT of the original function from the DFT of the interleaved one.*/                                       \
    /* The math is according to the document linked above.*/                                                                                            \
    for (unsigned long long k = 0; k < len / 2; k++)                                                                                                    \
    {                                                                                                                                                   \
        type fOfK = get(f, k);                                                                                                                          \
        type fOfLenMinusK = get(f, len - k);                                                                                                            \
        type rootK = RootOfUnity_##type(k, 2 * len);                                                                                                    \
        type rootLenMinusK = CAST(-1.0, type) / rootK; /* I did some math, and this equals RootOfUnity(len - k, 2 * len)*/                              \
        type val1 = I * rootK, val2 = I * rootLenMinusK;                                                                                                \
        type coeffA1 = CAST(0.5, type) * (CAST(1.0, type) - val1), coeffB1 = CAST(0.5, type) * (CAST(1.0, type) + val1);                                \
        type coeffA2 = CAST(0.5, type) * (CAST(1.0, type) - val2), coeffB2 = CAST(0.5, type) * (CAST(1.0, type) + val2);                                \
        get(f, k) = (fOfK * coeffA1) + (conj_##type(fOfLenMinusK) * coeffB1);                                                                           \
        get(f, len - k) = (fOfLenMinusK * coeffA2) + (conj_##type(fOfK) * coeffB2);                                                                     \
    }                                                                                                                                                   \
}                                                                                                                                                       \
                                                                                                                                                        \
void InverseRealInterleavedFFT_##type(Function_##type f)                                                                                                \
{                                                                                                                                                       \
    unsigned long long len = NumOfSamples_##type(&f);                                                                                                   \
                                                                                                                                                        \
    /* Applying preprocessing to revert back to the DFT of the interleaved function.*/                                                                  \
    /* The math is from the same document as the one I used for the forward transform.*/                                                                \
    for (unsigned long long k = 0; k < len / 2; k++)                                                                                                    \
    {                                                                                                                                                   \
        type fOfK = get(f, k);                                                                                                                          \
        type fOfLenMinusK = get(f, len - k);                                                                                                            \
        type rootK = RootOfUnity_##type(k, 2 * len);                                                                                                    \
        type rootLenMinusK = CAST(-1.0, type) / rootK; /* I did some math, and this equals RootOfUnity(len - k, 2 * len)*/                              \
        type val1 = I * rootK, val2 = I * rootLenMinusK;                                                                                                \
        type coeffA1 = conj_##type(CAST(0.5, type) * (CAST(1.0, type) - val1)), coeffB1 = conj_##type(CAST(0.5, type) * (CAST(1.0, type) + val1));      \
        type coeffA2 = conj_##type(CAST(0.5, type) * (CAST(1.0, type) - val2)), coeffB2 = conj_##type(CAST(0.5, type) * (CAST(1.0, type) + val2));      \
        get(f, k) = (fOfK * coeffA1) + (conj_##type(fOfLenMinusK) * coeffB1);                                                                           \
        get(f, len - k) = (fOfLenMinusK * coeffA2) + (conj_##type(fOfK) * coeffB2);                                                                     \
    }                                                                                                                                                   \
                                                                                                                                                        \
    /* Now applying inverse FFT. The result will be the original interleaved sequence of even and odd reals (with changes we've applied).*/             \
    InverseFFT_##type(f);                                                                                                                               \
}                                                                                                                                                       \
                                                                                                                                                        \
/* Most of the comments in this function refer to a picture of the recursion tree the algorithm follows, which I saw here:*/                            \
/* https://www.geeksforgeeks.org/iterative-fast-fourier-transformation-polynomial-multiplication/*/                                                     \
/* The algorithm itself is a modified version of this: https://stackoverflow.com/a/37729648/12553917. */                                                \
void FFT_##type(Function_##type f)                                                                                                                      \
{                                                                                                                                                       \
    unsigned long long len = NumOfSamples_##type(&f);                                                                                                   \
                                                                                                                                                        \
    /* Bit-reversing the array sorts it by the order of the leaves in the recursion tree.*/                                                             \
    BitReverseArr_##type(f);                                                                                                                            \
                                                                                                                                                        \
    /* The stride is the length of each sub-tree in the current level.*/                                                                                \
    /*We start from 2 because the level with length-1 sub-trees is trivial and assumed to already be contained in the array.*/                          \
    unsigned long long stride = 2, halfStride = 1;                                                                                                      \
    unsigned int logLen = CountTrailingZeroes(len);                                                                                                     \
                                                                                                                                                        \
    /* Each iteration of this loop climbs another level up the recursion tree.*/                                                                        \
    for (unsigned int j = 0; j < logLen; j++)                                                                                                           \
    {                                                                                                                                                   \
        /* Each iteration of this loop moves to the next tree in the current level.*/                                                                   \
        for (unsigned long long i = 0; i < len; i += stride)                                                                                            \
        {                                                                                                                                               \
            /* Each iteration of this loop moves to the next element in the current tree.*/                                                             \
            /* We only need to iterate over half the elements because in each iteration, we calculate two indices together.*/                           \
            for (unsigned long long k = 0; k < halfStride; k++)                                                                                         \
            {                                                                                                                                           \
                /* i serves as a sort of "base" for the current tree. i + k is the k'th element in the (i / stride)'th tree of this level.*/            \
                type evenSum = get(f, i + k);                                                                                                           \
                type oddSum = RootOfUnity_##type(k, stride) * get(f,i + k + halfStride); /* TODO: roots of unity can be cached at memory expense.*/     \
                get(f, i + k) = evenSum + oddSum;                                                                                                       \
                get(f, i + k + halfStride) = evenSum - oddSum;                                                                                          \
            }                                                                                                                                           \
        }                                                                                                                                               \
                                                                                                                                                        \
        /* In the next level, trees will be twice as big.*/                                                                                             \
        stride *= 2;                                                                                                                                    \
        halfStride *= 2;                                                                                                                                \
    }                                                                                                                                                   \
}                                                                                                                                                       \
                                                                                                                                                        \
void InverseFFT_##type(Function_##type f)                                                                                                               \
{                                                                                                                                                       \
    unsigned long long len = NumOfSamples_##type(&f);                                                                                                   \
                                                                                                                                                        \
    /* Conjugating every sample before applying forward FFT.*/                                                                                          \
    for (unsigned long long i = 0; i < len; i++)                                                                                                        \
    {                                                                                                                                                   \
        get(f, i) = conj_##type(get(f, i));                                                                                                             \
    }                                                                                                                                                   \
                                                                                                                                                        \
    /* Applying forward fft.*/                                                                                                                          \
    FFT_##type(f);                                                                                                                                      \
                                                                                                                                                        \
    /* Conjugating again and dividing by the num of samples.*/                                                                                          \
    for (unsigned long long i = 0; i < len; i++)                                                                                                        \
    {                                                                                                                                                   \
        get(f, i) = conj_##type(get(f, i)) / len;                                                                                                       \
    }                                                                                                                                                   \
}

SOUND_EDITOR_C_TYPED_CONTENTS(DoubleComplex)
SOUND_EDITOR_C_TYPED_CONTENTS(FloatComplex)