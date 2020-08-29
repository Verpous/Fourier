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

#include "SampledFunction.h"
#include "MyUtils.h"
#include <math.h> // for min.

// Important to use a power of two here. Writing this number as 1 << p pisses off gcc.
#define MAX_SEGMENT_LEN MEGAS(32)

#define SAMPLED_FUNCTION_C_TYPED_CONTENTS(type)                                                                                             \
char AllocateFunction_##type(Function_##type* f, unsigned long long length)                                                                 \
{                                                                                                                                           \
    f->funcType = type##Type;                                                                                                               \
    f->segmentLen = min(length, MAX_SEGMENT_LEN);                                                                                           \
    f->segmentCount = length / f->segmentLen;                                                                                               \
                                                                                                                                            \
    if ((f->samples = calloc(f->segmentCount, sizeof(type*))) == NULL)                                                                      \
    {                                                                                                                                       \
        return FALSE;                                                                                                                       \
    }                                                                                                                                       \
                                                                                                                                            \
    for (unsigned long i = 0; i < f->segmentCount; i++)                                                                                     \
    {                                                                                                                                       \
        if ((f->samples[i] = malloc(sizeof(type) * f->segmentLen)) == NULL)                                                                 \
        {                                                                                                                                   \
            return FALSE;                                                                                                                   \
        }                                                                                                                                   \
    }                                                                                                                                       \
                                                                                                                                            \
    return TRUE;                                                                                                                            \
}                                                                                                                                           \
                                                                                                                                            \
void DeallocateFunction_##type(Function_##type* f)                                                                                          \
{                                                                                                                                           \
    if (f->samples != NULL)                                                                                                                 \
    {                                                                                                                                       \
        for (unsigned long i = 0; i < f->segmentCount; i++)                                                                                 \
        {                                                                                                                                   \
            /* It's possible that some of these are NULL, but that's ok because free doesn't throw errors on NULL pointers.*/               \
            free(f->samples[i]);                                                                                                            \
        }                                                                                                                                   \
                                                                                                                                            \
        free(f->samples);                                                                                                                   \
    }                                                                                                                                       \
}                                                                                                                                           \
                                                                                                                                            \
unsigned long long NumOfSamples_##type(Function_##type* f)                                                                                  \
{                                                                                                                                           \
    return f->segmentCount * f->segmentLen;                                                                                                 \
}

unsigned long long NumOfSamples(Function* f)
{
    // The segment count and length are in the same place no matter what type f has, so we just cast it so some type and send it to NumOfSamples of that type.
    return NumOfSamples_FloatComplex((Function_FloatComplex*)f);
}

FunctionType GetType(Function* f)
{
    return *((FunctionType*)f);
}

void DeallocateFunction(Function* f)
{
    switch (GetType(f))
    {
        case FloatComplexType:
            DeallocateFunction_FloatComplex((Function_FloatComplex*)f);
            break;
        case DoubleComplexType:
            DeallocateFunction_DoubleComplex((Function_DoubleComplex*)f);
            break;
        default:
            break;
    }
}

SAMPLED_FUNCTION_C_TYPED_CONTENTS(FloatComplex)
SAMPLED_FUNCTION_C_TYPED_CONTENTS(DoubleComplex)