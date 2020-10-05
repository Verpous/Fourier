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
#include <math.h>	 // For min.
#include <limits.h>	 // For CHAR_BIT.
#include <stdio.h>	 // For fprintf.

#define SOUND_EDITOR_C_PRECISION_CONTENTS(precision)																									\
 __attribute__((always_inline)) inline																													\
precision##Complex PolarToCartesian_##precision##Complex(precision##Real magnitude, precision##Real angle)												\
{																																						\
	precision##Real sine, cosine;																														\
	sincos_##precision##Real(angle, &sine, &cosine);																									\
	return magnitude * (cosine + (I * sine));																											\
}																																						\
																																						\
 __attribute__((always_inline)) inline																													\
precision##Complex RootOfUnity_##precision##Complex(unsigned long long k, precision##Real N)															\
{																																						\
	return PolarToCartesian_##precision##Complex(1.0, (CAST(-2.0 * M_PI, precision##Real) * k) / N);													\
}																																						\
																																						\
void BitReverseArr_##precision##Complex(Function_##precision##Complex f)																				\
{																																						\
	unsigned long long len = NumOfSamples(&f);																											\
	unsigned int digits = CountTrailingZeroes(len);																										\
																																						\
	for (unsigned long long i = 0; i < len; i++)																										\
	{																																					\
		unsigned long long reversed = BitReverse(digits, i);																							\
																																						\
		/* Avoiding reversing the same thing twice.*/																									\
		if (reversed > i)																																\
		{																																				\
			precision##Complex temp = get(f, i);																										\
			get(f, i) = get(f, reversed);																												\
			get(f, reversed) = temp;																													\
		}																																				\
	}																																					\
}																																						\
																																						\
void RealInterleavedFFT_##precision##Complex(Function_##precision##Complex f)																			\
{																																						\
	unsigned long long len = NumOfSamples(&f);																											\
																																						\
	/* This number gets used a lot and as a float. So we convert it once to save on conversions later.*/												\
	precision##Real twoLenReal = 2 * len;																												\
																																						\
	FFT_##precision##Complex(f);																														\
																																						\
	/* Applying postprocessing to extract the DFT of the original function from the DFT of the interleaved one.*/										\
	/* The math is according to the document linked in the header for this function.*/																	\
	/* get(f, 0) is a special case because there is no get(f, len - 0).*/																				\
	get(f, 0) = CAST(0.5, precision##Complex) *																											\
				((get(f, 0) * CAST(1.0 - I, precision##Complex)) + (conj_##precision##Complex(get(f, 0)) * CAST(1.0 + I, precision##Complex)));			\
																																						\
	/* Note: get(f, len / 2) doesn't need any extra processing. It already has the right value.*/														\
	for (unsigned long long k = 1; k < len / 2; k++)																									\
	{																																					\
		precision##Complex fOfK = get(f, k);																											\
		precision##Complex fOfLenMinusK = get(f, len - k);																								\
		precision##Complex rootK = RootOfUnity_##precision##Complex(k, twoLenReal);																		\
		precision##Complex rootLenMinusK = CAST(-1.0, precision##Complex) / rootK; /* I did some math, and this equals RootOfUnity(len - k, 2 * len)*/	\
		precision##Complex val1 = I * rootK;																											\
		precision##Complex val2 = I * rootLenMinusK;																									\
		precision##Complex coeffA1 = CAST(0.5, precision##Complex) * (CAST(1.0, precision##Complex) - val1);											\
		precision##Complex coeffB1 = CAST(0.5, precision##Complex) * (CAST(1.0, precision##Complex) + val1);											\
		precision##Complex coeffA2 = CAST(0.5, precision##Complex) * (CAST(1.0, precision##Complex) - val2);											\
		precision##Complex coeffB2 = CAST(0.5, precision##Complex) * (CAST(1.0, precision##Complex) + val2);											\
		get(f, k) = (fOfK * coeffA1) + (conj_##precision##Complex(fOfLenMinusK) * coeffB1);																\
		get(f, len - k) = (fOfLenMinusK * coeffA2) + (conj_##precision##Complex(fOfK) * coeffB2);														\
	}																																					\
}																																						\
																																						\
void InverseRealInterleavedFFT_##precision##Complex(Function_##precision##Complex f)																	\
{																																						\
	unsigned long long len = NumOfSamples(&f);																											\
																																						\
	/* This number gets used a lot and as a float. So we convert it once to save on conversions later.*/												\
	precision##Complex twoLenReal = 2 * len;																											\
																																						\
	/* Applying preprocessing to revert back to the DFT of the interleaved function.*/																	\
	/* The math is from the same document as the one I used for the forward transform.*/																\
	/* get(f, 0) is a special case because there is no get(f, len - 0).*/																				\
	get(f, 0) = CAST(0.5, precision##Complex) *																											\
				((get(f, 0) * CAST(1.0 + I, precision##Complex)) + (conj_##precision##Complex(get(f, 0)) * CAST(1.0 - I, precision##Complex)));			\
																																						\
	/* Note: get(f, len / 2) doesn't need any extra processing. It already has the right value.*/														\
	for (unsigned long long k = 1; k < len / 2; k++)																									\
	{																																					\
		precision##Complex fOfK = get(f, k);																											\
		precision##Complex fOfLenMinusK = get(f, len - k);																								\
		precision##Complex rootK = RootOfUnity_##precision##Complex(k, twoLenReal);																		\
		precision##Complex rootLenMinusK = CAST(-1.0, precision##Complex) / rootK; /* I did some math, and this equals RootOfUnity(len - k, 2 * len)*/	\
		precision##Complex val1 = I * rootK;																											\
		precision##Complex val2 = I * rootLenMinusK;																									\
		precision##Complex coeffA1 = conj_##precision##Complex(CAST(0.5, precision##Complex) * (CAST(1.0, precision##Complex) - val1));					\
		precision##Complex coeffB1 = conj_##precision##Complex(CAST(0.5, precision##Complex) * (CAST(1.0, precision##Complex) + val1));					\
		precision##Complex coeffA2 = conj_##precision##Complex(CAST(0.5, precision##Complex) * (CAST(1.0, precision##Complex) - val2));					\
		precision##Complex coeffB2 = conj_##precision##Complex(CAST(0.5, precision##Complex) * (CAST(1.0, precision##Complex) + val2));					\
		get(f, k) = (fOfK * coeffA1) + (conj_##precision##Complex(fOfLenMinusK) * coeffB1);																\
		get(f, len - k) = (fOfLenMinusK * coeffA2) + (conj_##precision##Complex(fOfK) * coeffB2);														\
	}																																					\
																																						\
	/* Now applying inverse FFT. The result will be the original interleaved sequence of even and odd reals (with changes we've applied).*/				\
	InverseFFT_##precision##Complex(f);																													\
}																																						\
																																						\
/* Most of the comments in this function refer to a picture of the recursion tree the algorithm follows, which I saw here:*/							\
/* https://www.geeksforgeeks.org/iterative-fast-fourier-transformation-polynomial-multiplication/*/														\
/* The algorithm itself is a modified version of this: https://stackoverflow.com/a/37729648/12553917. */												\
void FFT_##precision##Complex(Function_##precision##Complex f)																							\
{																																						\
	/* Bit-reversing the array sorts it by the order of the leaves in the recursion tree.*/																\
	BitReverseArr_##precision##Complex(f);																												\
																																						\
	/* The stride is the length of each sub-tree in the current level.*/																				\
	/*We start from 2 because the level with length-1 sub-trees is trivial and assumed to already be contained in the array.*/							\
	unsigned long long stride = 2, halfStride = 1;																										\
																																						\
	/* We keep the version of stride that's converted to a float cached because we use it a lot.*/														\
	precision##Complex strideReal = stride;																												\
																																						\
	unsigned long long len = NumOfSamples(&f);																											\
	unsigned int logLen = CountTrailingZeroes(len);																										\
																																						\
	/* Each iteration of this loop climbs another level up the recursion tree.*/																		\
	for (unsigned int j = 0; j < logLen; j++)																											\
	{																																					\
		/* Each iteration of this loop moves to the next tree in the current level.*/																	\
		for (unsigned long long i = 0; i < len; i += stride)																							\
		{																																				\
			unsigned long long iPlusHalfStride = i + halfStride; /* Caching repeatedly-used calculation.*/												\
																																						\
			/* Each iteration of this loop moves to the next element in the current tree.*/																\
			/* We only need to iterate over half the elements because in each iteration, we calculate two indices together.*/							\
			for (unsigned long long k = 0; k < halfStride; k++)																							\
			{																																			\
				/* i serves as a sort of "base" for the current tree. i + k is the k'th element in the (i / stride)'th tree of this level.*/			\
				precision##Complex evenSum = get(f, i + k);																								\
				precision##Complex oddSum = RootOfUnity_##precision##Complex(k, strideReal) * get(f, iPlusHalfStride + k); /* TODO: cache roots.*/		\
				get(f, i + k) = evenSum + oddSum;																										\
				get(f, iPlusHalfStride + k) = evenSum - oddSum;																							\
			}																																			\
		}																																				\
																																						\
		/* In the next level, trees will be twice as big.*/																								\
		stride *= 2;																																	\
		halfStride *= 2;																																\
		strideReal = stride;																															\
	}																																					\
}																																						\
																																						\
void InverseFFT_##precision##Complex(Function_##precision##Complex f)																					\
{																																						\
	unsigned long long len = NumOfSamples(&f);																											\
																																						\
	/* Conjugating every sample before applying forward FFT.*/																							\
	for (unsigned long long i = 0; i < len; i++)																										\
	{																																					\
		get(f, i) = conj_##precision##Complex(get(f, i));																								\
	}																																					\
																																						\
	/* Applying forward fft.*/																															\
	FFT_##precision##Complex(f);																														\
																																						\
	/* Conjugating again and dividing by the num of samples.*/																							\
	for (unsigned long long i = 0; i < len; i++)																										\
	{																																					\
		get(f, i) = conj_##precision##Complex(get(f, i)) / len;																							\
	}																																					\
}

SOUND_EDITOR_C_PRECISION_CONTENTS(Double)
SOUND_EDITOR_C_PRECISION_CONTENTS(Float)

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

unsigned long long BitReverse(unsigned int digits, unsigned long long n)
{
	int i;
	unsigned long long reversed = 0, mask = 1;

	// First iterating on the left half of the number.
	for (i = 0; (2 * i) + 1 < digits; i++, mask <<= 1)
	{
		unsigned long long bit = mask & n;
		bit <<= digits - (2 * i) - 1;
		reversed |= bit;
	}

	// Now iterating on the right half.
	for (; i < digits; i++, mask <<= 1)
	{
		unsigned long long bit = mask & n;
		bit >>= (2 * i) + 1 - digits;
		reversed |= bit;
	}

	return reversed;
}

char ApplyModification(unsigned long long fromSample, unsigned long long toSample, ChangeType changeType, double changeAmount, double smoothing, unsigned short channel, Function **channelsData, Modification **modificationStack)
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
	#define APPLY_MODIFICATION_INTERNAL_TYPED(precision)																											\
	Function_##precision##Complex channelData = *(((Function_##precision##Complex**)channelsData)[modification->channel]);											\
	unsigned long long length = NumOfSamples(modification->oldFunc);																								\
	unsigned long long startSample = modification->startSample;																										\
	unsigned long long endSample = startSample + length - 1;																										\
	precision##Real smoothing = modification->smoothing, changeAmount = modification->changeAmount;																	\
																																									\
	/* This value appears in a few different places.*/																												\
	precision##Real tukeyWidth = (smoothing * length) / CAST(2.0, precision##Real);																					\
																																									\
	/* This gets reused in a lot of calculations.*/																													\
	precision##Real piDivWidth = CAST(M_PI, precision##Real) / tukeyWidth;																							\
																																									\
	/* The graph plateaus at this sample. Need to have this rounded up.*/																							\
	/* By clamping this the way that we do, we prevent double-dipping (applying the change to the same sample twice) when symmetrically applying a change.*/		\
	unsigned long long plateauStart = ClampInt(llceil_##precision##Real(tukeyWidth), 0, length / 2);																\
																																									\
	if (modification->changeType == MULTIPLY)																														\
	{																																								\
		unsigned long long i;																																		\
		precision##Real changeAmountMinusOne = changeAmount - 1.0; /* This number is often-used so we cache it.*/													\
																																									\
		/* The window function is piecewise so we'll apply it in two parts.*/																						\
		/* In the first part, 0 <= n < tukeyWidth, w[n] and w[length - 1 - n] are equal to 0.5 - 0.5 * cos(piDivWidth * n)).*/										\
		/* We want the multiplication to apply the changeAmount at its peak and 1 at the edges. So the real multiplier is 1 + (changeAmount - 1) * w[n].*/			\
		for (i = 0; i < plateauStart; i++)																															\
		{																																							\
			precision##Real multiplier = CAST(1.0, precision##Real) + (changeAmountMinusOne *																		\
				(CAST(0.5, precision##Real) - (CAST(0.5, precision##Real) * cos_##precision##Real(piDivWidth * i))));												\
			get(channelData, startSample + i) *= multiplier;																										\
			get(channelData, endSample - i) *= multiplier; /* Applying the change symmetrically.*/																	\
		}																																							\
																																									\
		/* This is the highest sample index that wasn't covered by the previous loop. We want to apply the next part for all remaining samples up to this one.*/	\
		unsigned long long plateauEnd = length - 1 - i;																												\
																																									\
		/* In the second part, tukeyWidth <= n <= (length - 1) / 2, w[n] and w[length - 1 - n] are equal to 1 so we apply the full changeAmount.*/					\
		/* We don't really bother with the math for what indices to apply it to. We just apply it to all indices that weren't affected by the previous part.*/		\
		for (; i <= plateauEnd; i++)																																\
		{																																							\
			get(channelData, startSample + i) *= changeAmount;																										\
		}																																							\
	}																																								\
	else /* Additive change. This can be either add or subtract.*/																									\
	{																																								\
		unsigned long long i;																																		\
																																									\
		/* The window function is piecewise so we'll apply it in two parts.*/																						\
		/* In the first part, 0 <= n < tukeyWindow, w[n] and w[length - 1 - n] are equal to 0.5 * 0.5 - 0.5 * cos(piDivWidth * n)).*/								\
		for (i = 0; i < plateauStart; i++)																															\
		{																																							\
			/* Calculating the magnitude we want to add.*/																											\
			precision##Real addition = changeAmount * (CAST(0.5, precision##Real) - (CAST(0.5, precision##Real) * cos_##precision##Real(piDivWidth * i)));			\
																																									\
			/* Getting the samples we want to add to.*/																												\
			precision##Real val1 = get(channelData, startSample + i);																								\
			precision##Real val2 = get(channelData, endSample - i);																									\
																																									\
			/* Calculating the new magnitudes of the samples, which are the old ones + the addition clamped at 0.*/													\
			precision##Real magnitude1 = Clamp##precision(cabs_##precision##Complex(val1) + addition, 0.0, INFINITY);												\
			precision##Real magnitude2 = Clamp##precision(cabs_##precision##Complex(val2) + addition, 0.0, INFINITY);												\
																																									\
			/* Setting the samples to have the same phase as before but with the new magnitudes.*/																	\
			get(channelData, startSample + i) = PolarToCartesian_##precision##Complex(magnitude1, carg_##precision##Complex(val1));									\
			get(channelData, endSample - i) = PolarToCartesian_##precision##Complex(magnitude2, carg_##precision##Complex(val2));	 								\
		}																																							\
																																									\
		/* This is the highest sample index that wasn't covered by the previous loop. We want to apply the next part for all remaining samples up to this one.*/	\
		unsigned long long plateauEnd = length - 1 - i;																												\
																																									\
		/* In the second part, tukeyWindow <= n <= (length - 1) / 2, w[n] and w[length - 1 - n] are equal to 1 so we add the full changeAmount.*/					\
		/* We don't really bother with the math for what indices to apply it to. We just apply it to all indices that weren't affected by the previous part.*/		\
		for (; i <= plateauEnd; i++)																																\
		{																																							\
			precision##Real val = get(channelData, startSample + i);																								\
			precision##Real magnitude = Clamp##precision(cabs_##precision##Complex(val) + changeAmount, 0.0, INFINITY);												\
			get(channelData, startSample + i) = PolarToCartesian_##precision##Complex(magnitude, carg_##precision##Complex(val));									\
		}																																							\
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

char RedoLastModification(Function** channelsData, Modification** modificationStack)
{
	Modification* current = *modificationStack;

	if (CanRedo(current))
	{
		Modification* next = current->next;

		// Applying the modification again.
		ApplyModificationInternal(channelsData, next);

		// Assigning this modification as our current position on the stack.
		*modificationStack = next;
		return TRUE;
	}

	return FALSE;
}

char UndoLastModification(Function** channelsData, Modification** modificationStack)
{
	Modification* current = *modificationStack;

	if (CanUndo(current))
	{
		Modification* prev = current->prev;

		// Restoring the backed up values back into the channel data.
		CopySamples(channelsData[current->channel], current->oldFunc, current->startSample, 0, NumOfSamples(current->oldFunc));

		// Assigning the last modification as our current position on the stack.
		*modificationStack = prev;
		return TRUE;
	}

	return FALSE;
}

char CanRedo(Modification* modificationStack)
{
	return modificationStack != NULL && modificationStack->next != NULL;
}

char CanUndo(Modification* modificationStack)
{
	return modificationStack != NULL && modificationStack->prev != NULL;
}

unsigned short GetRedoChannel(Modification* modificationStack)
{
	return modificationStack->next->channel;
}

unsigned short GetUndoChannel(Modification* modificationStack)
{
	return modificationStack->channel;
}

void InitializeModificationStack(Modification** modificationStack)
{
	// The stack is initialized with an empty modification that represents the point before any changes were made.
	*modificationStack = calloc(1, sizeof(Modification));
}

void DeallocateModificationStack(Modification* modificationStack)
{
	if (modificationStack != NULL)
	{
		DeallocateModificationsNextwards(modificationStack->next);
		DeallocateModificationsPrevwards(modificationStack);
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