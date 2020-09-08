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

// Important to use a power of two here.
#define MAX_SEGMENT_LEN MEGAS(32)

// TODO: allocatefunction doesn't calculate segment count and length well for non-powers-of-two.
#define SAMPLED_FUNCTION_C_TYPED_CONTENTS(type)                                                                                                         \
char AllocateFunctionInternals_##type(Function_##type* f, unsigned long long length)                                                                    \
{                                                                                                                                                       \
    f->funcType = type##Type;                                                                                                                           \
    f->segmentLen = min(length, MAX_SEGMENT_LEN);                                                                                                       \
    f->segmentCount = length / f->segmentLen;                                                                                                           \
                                                                                                                                                        \
    if ((f->samples = calloc(f->segmentCount, sizeof(type*))) == NULL)                                                                                  \
    {                                                                                                                                                   \
        return FALSE;                                                                                                                                   \
    }                                                                                                                                                   \
                                                                                                                                                        \
    for (unsigned long i = 0; i < f->segmentCount; i++)                                                                                                 \
    {                                                                                                                                                   \
        if ((f->samples[i] = malloc(sizeof(type) * f->segmentLen)) == NULL)                                                                             \
        {                                                                                                                                               \
            return FALSE;                                                                                                                               \
        }                                                                                                                                               \
    }                                                                                                                                                   \
                                                                                                                                                        \
    return TRUE;                                                                                                                                        \
}                                                                                                                                                       \
                                                                                                                                                        \
void DeallocateFunctionInternals_##type(Function_##type* f)                                                                                             \
{                                                                                                                                                       \
    if (f->samples != NULL)                                                                                                                             \
    {                                                                                                                                                   \
        for (unsigned long i = 0; i < f->segmentCount; i++)                                                                                             \
        {                                                                                                                                               \
            /* It's possible that some of these are NULL, but that's ok because free doesn't throw errors on NULL pointers.*/                           \
            free(f->samples[i]);                                                                                                                        \
        }                                                                                                                                               \
                                                                                                                                                        \
        free(f->samples);                                                                                                                               \
    }                                                                                                                                                   \
}                                                                                                                                                       \
                                                                                                                                                        \
Function_##type* CreatePartialClone_##type(Function_##type* f, unsigned long long from, unsigned long long to)                                          \
{                                                                                                                                                       \
    Function_##type* clone = malloc(sizeof(type));                                                                                                      \
                                                                                                                                                        \
    if (AllocateFunctionInternals_##type(clone, to - from + 1)) /* +1 because we're editing [from, to] (inclusive).*/                                   \
    {                                                                                                                                                   \
        CopySamples_##type(*clone, *f, 0, from, to - from + 1);                                                                                         \
    }                                                                                                                                                   \
    else                                                                                                                                                \
    {                                                                                                                                                   \
        DeallocateFunctionInternals_##type(clone);                                                                                                      \
        free(clone);                                                                                                                                    \
        clone = NULL;                                                                                                                                   \
    }                                                                                                                                                   \
                                                                                                                                                        \
    return clone;                                                                                                                                       \
}                                                                                                                                                       \
                                                                                                                                                        \
void CopySamples_##type(Function_##type dest, Function_##type src, unsigned long long destFrom, unsigned long long srcFrom, unsigned long long length)  \
{                                                                                                                                                       \
    for (unsigned long long i = 0; i < length; i++)                                                                                                     \
    {                                                                                                                                                   \
        get(dest, destFrom + i) = get(src, srcFrom + i);                                                                                                \
    }                                                                                                                                                   \
}

unsigned long long NumOfSamples(Function* f)
{
    // The segment count and length are in the same place no matter what type f has, so we just cast it so some type and read it that way.
    Function_FloatComplex* castedF = (Function_FloatComplex*)f;
    return castedF->segmentCount * castedF->segmentLen;
}

FunctionType GetType(Function* f)
{
    return *((FunctionType*)f);
}

void DeallocateFunctionInternals(Function* f)
{
    switch (GetType(f))
    {
        case FloatComplexType:
            DeallocateFunctionInternals_FloatComplex((Function_FloatComplex*)f);
            break;
        case DoubleComplexType:
            DeallocateFunctionInternals_DoubleComplex((Function_DoubleComplex*)f);
            break;
        default:
            break;
    }
}

Function* CreatePartialClone(Function* f, unsigned long long from, unsigned long long to)
{
    switch (GetType(f))
    {
        case FloatComplexType:
            return CreatePartialClone_FloatComplex(f, from, to);
        case DoubleComplexType:
            return CreatePartialClone_DoubleComplex(f, from, to);
        default:
            return NULL;
    }
}

void CopySamples(Function* dest, Function* src, unsigned long long destFrom, unsigned long long srcFrom, unsigned long long length)
{
    switch (GetType(dest))
    {
        case FloatComplexType:
            CopySamples_FloatComplex(*((Function_FloatComplex*)dest), *((Function_FloatComplex*)dest), destFrom, srcFrom, length);
            break;
        case DoubleComplexType:
            CopySamples_DoubleComplex(*((Function_DoubleComplex*)dest), *((Function_DoubleComplex*)dest), destFrom, srcFrom, length);
            break;
        default:
            break;
    }
}

SAMPLED_FUNCTION_C_TYPED_CONTENTS(FloatComplex)
SAMPLED_FUNCTION_C_TYPED_CONTENTS(DoubleComplex)