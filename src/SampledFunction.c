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
#include <math.h> // for min, log, etc.

// Important to use a power of two here.
#define MAX_SEGMENT_LEN MEGAS(16)

#define SAMPLED_FUNCTION_C_TYPED_CONTENTS(type)																												\
char AllocateFunctionInternals_##type(Function_##type* f, unsigned long long length)																		\
{																																							\
	f->funcType = type##Type;																																\
																																							\
	/* Choosing the segment length and count such that we can hold at least 'length' many samples.*/														\
	f->totalLen = length;																																	\
	f->segmentLen = min(length, MAX_SEGMENT_LEN);																											\
	f->segmentCount = DivCeilInt(length, f->segmentLen);																									\
																																							\
	if ((f->samples = calloc(f->segmentCount, sizeof(type*))) == NULL)																						\
	{																																						\
		return FALSE;																																		\
	}																																						\
																																							\
	for (unsigned long i = 0; i < f->segmentCount - 1; i++)																									\
	{																																						\
		if ((f->samples[i] = malloc(sizeof(type) * f->segmentLen)) == NULL)																					\
		{																																					\
			return FALSE;																																	\
		}																																					\
	}																																						\
																																							\
	/* The last segment may be shorter than segmentLen. Its size is however many samples are left to allocate for.*/										\
	if ((f->samples[f->segmentCount - 1] = malloc(sizeof(type) * (f->totalLen - ((f->segmentCount - 1) * f->segmentLen)))) == NULL)							\
	{																																						\
		return FALSE;																																		\
	}																																						\
																																							\
	return TRUE;																																			\
}																																							\
																																							\
void DeallocateFunctionInternals_##type(Function_##type* f)																									\
{																																							\
	if (f->samples != NULL)																																	\
	{																																						\
		for (unsigned long i = 0; i < f->segmentCount; i++)																									\
		{																																					\
			/* It's possible that some of these are NULL, but that's ok because free doesn't throw errors on NULL pointers.*/								\
			free(f->samples[i]);																															\
		}																																					\
																																							\
		free(f->samples);																																	\
	}																																						\
}																																							\
																																							\
Function_##type* CreatePartialClone_##type(Function_##type* f, unsigned long long from, unsigned long long to)												\
{																																							\
	Function_##type* clone = malloc(sizeof(Function_##type));																								\
																																							\
	if (AllocateFunctionInternals_##type(clone, to - from + 1)) /* +1 because we're editing [from, to] (inclusive).*/										\
	{																																						\
		CopySamples_##type(*clone, *f, 0, from, to - from + 1);																								\
	}																																						\
	else																																					\
	{																																						\
		DeallocateFunctionInternals_##type(clone);																											\
		free(clone);																																		\
		clone = NULL;																																		\
	}																																						\
																																							\
	return clone;																																			\
}																																							\
																																							\
/* TODO: potential optimization - use memcpy to work on large chunks at a time instead of one by one using get which is slow.*/								\
void CopySamples_##type(Function_##type dest, Function_##type src, unsigned long long destFrom, unsigned long long srcFrom, unsigned long long length)		\
{																																							\
	for (unsigned long long i = 0; i < length; i++)																											\
	{																																						\
		get(dest, destFrom + i) = get(src, srcFrom + i);																									\
	}																																						\
}

// TODO: potential optimization - use internal knowledge about how functions work to read from them for min and max more efficiently.*/
#define SAMPLED_FUNCTION_C_PRECISION_CONTENTS(precision)																									\
precision##Real GetMax_##precision##Real(Function_##precision##Real f, unsigned long long startIndex, unsigned long long endIndex, unsigned long long s)							\
{																																							\
	precision##Real max = -INFINITY;																														\
																																							\
	for (unsigned long long i = startIndex; i < endIndex; i+=s)																								\
	{																																						\
		precision##Real sample = get(f, i);																													\
																																							\
		if (sample > max)																																	\
		{																																					\
			max = sample;																																	\
		}																																					\
	}																																						\
																																							\
	return max;																																				\
}																																							\
																																							\
precision##Real GetMin_##precision##Real(Function_##precision##Real f, unsigned long long startIndex, unsigned long long endIndex, unsigned long long s)							\
{																																							\
	precision##Real min = INFINITY;																															\
																																							\
	for (unsigned long long i = startIndex; i < endIndex; i+=s)																								\
	{																																						\
		precision##Real sample = get(f, i);																													\
																																							\
		if (sample < min)																																	\
		{																																					\
			min = sample;																																	\
		}																																					\
	}																																						\
																																							\
	return min;																																				\
}																																							\
																																							\
precision##Complex GetMax_##precision##Complex(Function_##precision##Complex f, unsigned long long startIndex, unsigned long long endIndex, unsigned long long s)					\
{																																							\
	precision##Real maxSqrMagnitude = -INFINITY;																											\
	precision##Complex maxSample = NAN;																														\
																																							\
	for (unsigned long long i = startIndex; i < endIndex; i+=s)																								\
	{																																						\
		precision##Complex sample = get(f, i);																												\
		precision##Real sqrMag = SquareMagnitude##precision##Complex(sample);																				\
																																							\
		if (sqrMag > maxSqrMagnitude)																														\
		{																																					\
			maxSample = sample;																																\
			maxSqrMagnitude = sqrMag;																														\
		}																																					\
	}																																						\
																																							\
	return maxSample;																																		\
}																																							\
																																							\
precision##Complex GetMin_##precision##Complex(Function_##precision##Complex f, unsigned long long startIndex, unsigned long long endIndex, unsigned long long s)					\
{																																							\
	precision##Real minSqrMagnitude = INFINITY;																												\
	precision##Complex minSample = NAN;																														\
																																							\
	for (unsigned long long i = startIndex; i < endIndex; i+=s)																								\
	{																																						\
		precision##Complex sample = get(f, i);																												\
		precision##Real sqrMag = SquareMagnitude##precision##Complex(sample);																				\
																																							\
		if (sqrMag < minSqrMagnitude)																														\
		{																																					\
			minSample = sample;																																\
			minSqrMagnitude = sqrMag;																														\
		}																																					\
	}																																						\
																																							\
	return minSample;																																		\
}																																							\
																																							\
Function_##precision##Real ReadComplexFunctionAsReal_##precision##Complex(Function_##precision##Complex* f)													\
{																																							\
	Function_##precision##Real fReal;																														\
	fReal.funcType = precision##Real##Type;																													\
	fReal.segmentLen = 2 * f->segmentLen;																													\
	fReal.segmentCount = f->segmentCount;																													\
	fReal.totalLen = 2 * f->totalLen;																														\
	fReal.samples = ((precision##Real**)f->samples);																										\
	return fReal;																																			\
}

unsigned long long NumOfSamples(Function* f)
{
	// The total length is in the same place no matter what type f has, so we just cast it so some type and read it that way.
	// This function is sort of here for legacy reasons. It used to take a calculation to get this value but now it's just stored inside the struct.
	return ((Function_FloatComplex*)f)->totalLen;
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
SAMPLED_FUNCTION_C_TYPED_CONTENTS(FloatReal)
SAMPLED_FUNCTION_C_TYPED_CONTENTS(DoubleReal)

SAMPLED_FUNCTION_C_PRECISION_CONTENTS(Float)
SAMPLED_FUNCTION_C_PRECISION_CONTENTS(Double)