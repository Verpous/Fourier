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

#define carg_DoubleComplex(x) carg(x)
#define carg_FloatComplex(x) cargf(x)

#define cabs_DoubleComplex(x) cabs(x)
#define cabs_FloatComplex(x) cabsf(x)

#define cos_DoubleReal(x) cos(x)
#define cos_FloatReal(x) cosf(x)

#define ceil_DoubleReal(x) ceil(x)
#define ceil_FloatReal(x) ceilf(x)

#define RootOfUnity_DoubleComplex(k, N) cexp_DoubleComplex((CAST(-2.0 * M_PI, DoubleComplex)  * I * (k)) / (N))
#define RootOfUnity_FloatComplex(k, N) cexp_FloatComplex((CAST(-2.0 * M_PI, FloatComplex)  * I * (k)) / (N))

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

void InverseRealInterleavedFFT(Function* f)
{
    switch (GetType(f))
    {
        case FloatComplexType:
            InverseRealInterleavedFFT_FloatComplex(*((Function_FloatComplex*)f));
            break;
        case DoubleComplexType:
            InverseRealInterleavedFFT_DoubleComplex(*((Function_DoubleComplex*)f));
            break;
        default:
            fprintf(stderr, "Tried to IFFT an invalid type.\n");
            break;
    }
}

char ApplyModification(unsigned long long fromSample, unsigned long long toSample, ChangeType changeType, double changeAmount, double smoothing, unsigned short channel, Function** channelsData, Modification** modificationStack)
{
    // Deallocating changes that were applied and then undone. A new modification means a new branching of the modifications tree, and we are only interested in the path of the tree we're currently on.
    DeallocateModificationsNextwards((*modificationStack)->next);

    // Creating a modification struct for this change.
    Modification* modification = malloc(sizeof(Modification));
    modification->startSample = fromSample;
    modification->oldFunc = CreatePartialClone(channelsData[channel], fromSample, toSample); // Recording the state of the samples that are about to be changed.
    modification->changeType = changeType;
    modification->changeAmount = changeAmount;
    modification->smoothing = smoothing;
    modification->channel = channel;
    modification->prev = *modificationStack;
    modification->next = NULL;

    // Exiting if malloc failed. Note that by doing this after deallocating next modifications, we lose them but if we switch the order, allocating memory may fail where it would otherwise succeed.
    if (modification->oldFunc == NULL)
    {
        return FALSE;
    }

    // Assigning this change to the new top of the stack.
    (*modificationStack)->next = modification;
    *modificationStack = modification;

    // This actually makes the change in the channel data.
    ApplyModificationInternal(channelsData, modification);
    return TRUE;
}

void ApplyModificationInternal(Function** channelsData, Modification* modification)
{
    #define APPLY_MODIFICATION_INTERNAL_TYPED(precision)                                                                                                            \
    Function_##precision##Complex channelData = *(((Function_##precision##Complex**)channelsData)[modification->channel]);                                          \
    unsigned long long length = NumOfSamples(modification->oldFunc);                                                                                                \
    unsigned long long startSample = modification->startSample;                                                                                                     \
    precision##Real smoothing = modification->smoothing, changeAmount = modification->changeAmount;                                                                 \
                                                                                                                                                                    \
    /* After this threshold the graph plateaus.*/                                                                                                                   \
    precision##Real tukeyThreshold = (smoothing * length) / CAST(2.0, precision##Real);                                                                             \
                                                                                                                                                                    \
    /* This gets reused in a lot of calculations.*/                                                                                                                 \
    precision##Real piDivThreshold = CAST(M_PI, precision##Real) / tukeyThreshold;                                                                                  \
                                                                                                                                                                    \
    /* Need to have this rounded up.*/                                                                                                                              \
    unsigned long long tukeyThresholdInt = ceil_##precision##Real(tukeyThreshold);                                                                                  \
                                                                                                                                                                    \
    /* Because we apply the changes symmetrically, there's a risk that the halfway point will get applied twice when the length is even.*/                          \
    /* The following calculations are for finding out where we need to stop applying changes symmetrically in order to not double dip on the halfway point.*/       \
    unsigned long long halfwayPoint = (length -  1) / 2;                                                                                                            \
    unsigned long long tukeyThresholdClamped = tukeyThresholdInt == halfwayPoint + 1 && length % 2 == 0 ? halfwayPoint : tukeyThresholdInt;                         \
    unsigned long long halfwayPointClamped = length % 2 == 0 ? halfwayPoint : halfwayPoint - 1;                                                                     \
                                                                                                                                                                    \
    if (modification->changeType == MULTIPLY)                                                                                                                       \
    {                                                                                                                                                               \
        unsigned long long i;                                                                                                                                       \
                                                                                                                                                                    \
        /* The window function is piecewise so we'll apply it in two parts.*/                                                                                       \
        /* In the first part, 0 <= n < tukeyWindow, w[n] and w[length - 1 - n] are equal to 0.5 * (1 - cos(n / piDivThreshold)).*/                                  \
        for (i = 0; i < tukeyThresholdClamped; i++)                                                                                                                 \
        {                                                                                                                                                           \
            precision##Real multiplier = changeAmount * (CAST(0.5, precision##Real) - (CAST(0.5, precision##Real) * cos_##precision##Real(piDivThreshold * i)));    \
            get(channelData, startSample + i) *= multiplier;                                                                                                        \
            get(channelData, startSample + (length - 1 - i)) *= multiplier; /* Applying the change symmetrically.*/                                                 \
        }                                                                                                                                                           \
                                                                                                                                                                    \
        /* In the second part, tukeyWindow <= n <= (length - 1) / 2, w[n] and w[length - 1 - n] are equal to 1.*/                                                   \
        for (; i <= halfwayPointClamped; i++)                                                                                                                       \
        {                                                                                                                                                           \
            get(channelData, startSample + i) *= changeAmount;                                                                                                      \
            get(channelData, startSample + (length - 1 - i)) *= changeAmount;                                                                                       \
        }                                                                                                                                                           \
                                                                                                                                                                    \
        if (length % 2 == 0)                                                                                                                                        \
        {                                                                                                                                                           \
            get(channelData, startSample + i) *= changeAmount;                                                                                                      \
        }                                                                                                                                                           \
    }                                                                                                                                                               \
    else /* Additive change.*/                                                                                                                                      \
    {                                                                                                                                                               \
        unsigned long long i;                                                                                                                                       \
                                                                                                                                                                    \
        /* The window function is piecewise so we'll apply it in two parts.*/                                                                                       \
        /* In the first part, 0 <= n < tukeyWindow, w[n] and w[length - 1 - n] are equal to 0.5 * (1 - cos(n / piDivThreshold)).*/                                  \
        for (i = 0; i < tukeyThresholdClamped; i++)                                                                                                                 \
        {                                                                                                                                                           \
            precision##Real addition = changeAmount * (CAST(0.5, precision##Real) - (CAST(0.5, precision##Real) * cos_##precision##Real(piDivThreshold * i)));      \
            precision##Real val1 = get(channelData, startSample + i), val2 = get(channelData, startSample + (length - 1 - i));                                      \
                                                                                                                                                                    \
            /* Calculating the new magnitudes, which are the old ones + the addition clamped at 0.*/                                                                \
            precision##Real magnitude1 = Clamp##precision(cabs_##precision##Complex(val1) + addition, 0.0, INFINITY);                                               \
            precision##Real magnitude2 = Clamp##precision(cabs_##precision##Complex(val2) + addition, 0.0, INFINITY);                                               \
                                                                                                                                                                    \
            /* Setting the new numbers to have the same phase as before but with the new magnitudes.*/                                                              \
            get(channelData, startSample + i) = magnitude1 * cexp(I * carg_##precision##Complex(val1));                                                             \
            get(channelData, startSample + (length - 1 - i)) = magnitude2 * cexp(I * carg_##precision##Complex(val2)); /* Applying the change symmetrically.*/      \
        }                                                                                                                                                           \
                                                                                                                                                                    \
        /* In the second part, tukeyWindow <= n <= (length - 1) / 2, w[n] and w[length - 1 - n] are equal to 1.*/                                                   \
        for (; i <= halfwayPointClamped; i++)                                                                                                                       \
        {                                                                                                                                                           \
            precision##Real val1 = get(channelData, startSample + i), val2 = get(channelData, startSample + (length - 1 - i));                                      \
            precision##Real magnitude1 = Clamp##precision(cabs_##precision##Complex(val1) + changeAmount, 0.0, INFINITY);                                           \
            precision##Real magnitude2 = Clamp##precision(cabs_##precision##Complex(val2) + changeAmount, 0.0, INFINITY);                                           \
            get(channelData, startSample + i) = magnitude1 * cexp(I * carg_##precision##Complex(val1));                                                             \
            get(channelData, startSample + (length - 1 - i)) = magnitude2 * cexp(I * carg_##precision##Complex(val2));                                              \
        }                                                                                                                                                           \
                                                                                                                                                                    \
        if (length % 2 == 0)                                                                                                                                        \
        {                                                                                                                                                           \
            precision##Real val = get(channelData, startSample + i);                                                                                                \
            precision##Real magnitude = Clamp##precision(cabs_##precision##Complex(val) + changeAmount, 0.0, INFINITY);                                             \
            get(channelData, startSample + i) = magnitude * cexp(I * carg_##precision##Complex(val));                                                               \
        }                                                                                                                                                           \
    }

    // Normally I would have this in a switch statement, but there's lots of variable declarations inside these macros that won't compile in a switch statement.
    if (GetType(channelsData[modification->channel]) == FloatComplexType)
    {
        APPLY_MODIFICATION_INTERNAL_TYPED(Float)
    }
    else
    {
        APPLY_MODIFICATION_INTERNAL_TYPED(Double)
    }
}

void UndoLastModification(Function** channelsData, Modification** modificationStack)
{
    Modification* current = *modificationStack;
    Modification* prev = current->prev;

    if (prev == NULL)
    {
        fprintf(stderr, "Tried to undo even though there's nothing to undo.\n");
        return;
    }

    // Restoring the backed up values back into the channel data.
    CopySamples(channelsData[current->channel], current->oldFunc, current->startSample, 0, NumOfSamples(current->oldFunc));

    // Assigning the last modification as our current position on the stack.
    *modificationStack = prev;
}

void RedoLastModification(Function** channelsData, Modification** modificationStack)
{
    Modification* next = (*modificationStack)->next;

    if (next == NULL)
    {
        fprintf(stderr, "Tried to redo even though there's nothing to redo.\n");
        return;
    }

    // Applying the modification again.
    ApplyModificationInternal(channelsData, next);

    // Assigning this modification as our current position on the stack.
    *modificationStack = next;
}

void InitializeModificationStack(Modification** modificationStack)
{
    // The stack is initialized with an empty modification that represents the point before any changes were made.
    *modificationStack = calloc(1, sizeof(Modification));
}

void DeallocateModificationStack(Modification** modificationStack)
{
    if (*modificationStack != NULL)
    {
        DeallocateModificationsNextwards((*modificationStack)->next);
        DeallocateModificationsPrevwards(*modificationStack);
        *modificationStack = NULL;
    }
}

void DeallocateModificationsNextwards(Modification* modificationStack)
{
    Modification* current = modificationStack;

    while (current != NULL)
    {
        Modification* next = current->next;
        DeallocateModification(current);
        current = next;
    }
}

void DeallocateModificationsPrevwards(Modification* modificationStack)
{
    Modification* current = modificationStack;

    while (current != NULL)
    {
        Modification* prev = current->prev;
        DeallocateModification(current);
        current = prev;
    }
}

void DeallocateModification(Modification* modification)
{
    if (modification->oldFunc != NULL)
    {
        DeallocateFunctionInternals(modification->oldFunc);
        free(modification->oldFunc);
    }

    free(modification);
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
    unsigned long long len = NumOfSamples(&f);                                                                                                          \
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
    unsigned long long len = NumOfSamples(&f);                                                                                                          \
    FFT_##type(f);                                                                                                                                      \
                                                                                                                                                        \
    /* Applying postprocessing to extract the DFT of the original function from the DFT of the interleaved one.*/                                       \
    /* The math is according to the document linked above.*/                                                                                            \
    /* get(f, 0) is a special case because there is no get(f, len - 0)*/                                                                                \
    get(f, 0) = CAST(0.5, type) * ((get(f, 0) * CAST(1.0 - I, type)) + (conj_##type(get(f, 0)) * CAST(1.0 + I, type)));                                 \
                                                                                                                                                        \
    for (unsigned long long k = 1; k < len / 2; k++)                                                                                                    \
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
    unsigned long long len = NumOfSamples(&f);                                                                                                          \
                                                                                                                                                        \
    /* Applying preprocessing to revert back to the DFT of the interleaved function.*/                                                                  \
    /* The math is from the same document as the one I used for the forward transform.*/                                                                \
    /* get(f, 0) is a special case because there is no get(f, len - 0)*/                                                                                \
    get(f, 0) = CAST(0.5, type) * ((get(f, 0) * CAST(1.0 + I, type)) + (conj_##type(get(f, 0)) * CAST(1.0 - I, type)));                                 \
                                                                                                                                                        \
    for (unsigned long long k = 1; k < len / 2; k++)                                                                                                    \
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
    unsigned long long len = NumOfSamples(&f);                                                                                                          \
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
    unsigned long long len = NumOfSamples(&f);                                                                                                          \
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