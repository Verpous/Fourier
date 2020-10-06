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
/* Takes a magnitude and a phase angle and returns the complex number with these polar coordinates.*/													\
 __attribute__((always_inline)) inline																													\
precision##Complex PolarToCartesian_##precision##Complex(precision##Real magnitude, precision##Real angle)												\
{																																						\
	precision##Real sine, cosine;																														\
	sincos_##precision##Real(angle, &sine, &cosine);																									\
	return magnitude * (cosine + (I * sine));																											\
}																																						\
																																						\
/* Calculates the k/N'th root of unity. That is, exp(-2 * pi * i * k / N).*/																			\
 __attribute__((always_inline)) inline																													\
precision##Complex RootOfUnity_##precision##Complex(unsigned long long k, precision##Real N)															\
{																																						\
	return PolarToCartesian_##precision##Complex(1.0, (CAST(-2.0 * M_PI, precision##Real) * k) / N);													\
}																																						\
																																						\
SoundEditorCache* InitializeSoundEditor_##precision##Complex(Function_##precision##Complex* f)															\
{																																						\
	/* At first we have to allocate a bunch of stuff.*/																									\
	SoundEditorCache* cache = malloc(sizeof(SoundEditorCache));																							\
																																						\
	if (cache == NULL)																																	\
	{																																					\
		return NULL;																																	\
	}																																					\
																																						\
	cache->twiddleFactors = malloc(sizeof(Function_##precision##Complex));																				\
																																						\
	if (cache->twiddleFactors == NULL)																													\
	{																																					\
		free(cache);																																	\
		return NULL;																																	\
	}																																					\
																																						\
	unsigned long long length = NumOfSamples(f);																										\
	unsigned long long halfLength = length / 2;																											\
																																						\
	if (!AllocateFunctionInternals_##precision##Complex(cache->twiddleFactors, halfLength))																\
	{																																					\
		DeallocateFunctionInternals_##precision##Complex(cache->twiddleFactors);																		\
		free(cache->twiddleFactors);																													\
		free(cache);																																	\
		return NULL;																																	\
	}																																					\
																																						\
	/* Now that everything is allocated, we can proceed with occupying the cache with values.*/															\
	cache->length = length;																																\
	cache->logLength = CountTrailingZeroes(length);																										\
	Function_##precision##Complex twiddleFactors = *CAST(cache->twiddleFactors, Function_##precision##Complex*);										\
																																						\
	/* Converting to real because we'll be using it that way a lot.*/																					\
	precision##Real lengthReal = length;																												\
																																						\
	get(twiddleFactors, 0) = 1.0;																														\
																																						\
	for (unsigned long long i = 1; i < halfLength; i++)																									\
	{																																					\
		get(twiddleFactors, i) = RootOfUnity_##precision##Complex(i, lengthReal);																		\
	}																																					\
																																						\
	return cache;																																		\
}																																						\
																																						\
void BitReverseArr_##precision##Complex(Function_##precision##Complex f, SoundEditorCache* cache)														\
{																																						\
	unsigned long long len = cache->length;																												\
	unsigned int digits = cache->logLength;																												\
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
/* This is an auxiliary function of RealInterleavedFFT. It calculates the postprocessing step for it.*/													\
/* 'sample' is the k'th sample in the function, 'oppositeSample' is the N-k'th sample 'root' is the k/N'th root of unity.*/								\
 __attribute__((always_inline)) inline																													\
precision##Complex ForwardPostprocess_##precision##Complex(precision##Complex sample, precision##Complex oppositeSample, precision##Complex root)		\
{																																						\
	precision##Complex val = I * root;																													\
	precision##Complex coeffA = CAST(1.0, precision##Complex) - val;																					\
	precision##Complex coeffB = CAST(1.0, precision##Complex) + val;																					\
	return CAST(0.5, precision##Complex) * ((sample * coeffA) + (conj_##precision##Complex(oppositeSample) * coeffB));									\
}																																						\
																																						\
/* This is an auxiliary function of RealInterleavedFFT. It symmetrically applies postprocessing to the k'th and N-k'th samples in the function.*/		\
/* 'sample' is the k'th sample in the function, 'oppositeSample' is the N-k'th sample 'root' is the k/N'th root of unity.*/								\
 __attribute__((always_inline)) inline																													\
void ForwardPostprocessSymmetric_##precision##Complex(precision##Complex* sample, precision##Complex* oppositeSample, precision##Complex root)			\
{																																						\
	/* Can't just dereference these when I need them because their values get changed and they both need both.*/										\
	precision##Complex sampleVal = *sample, oppositeSampleVal = *oppositeSample;																		\
																																						\
	/* root is assumed to be equal to RootOfUnity(k, 2N) where k is the index of 'sample' and N is the length of the interleaved function.*/			\
	/* I did some math, and the following expression equals RootOfUnity(N - k, 2N). N - k is assumed to be the index of 'oppositeSample'.*/				\
	precision##Complex oppositeRoot = CAST(-1.0, precision##Complex) / root; 																			\
	*sample = ForwardPostprocess_##precision##Complex(sampleVal, oppositeSampleVal, root);																\
	*oppositeSample = ForwardPostprocess_##precision##Complex(oppositeSampleVal, sampleVal, oppositeRoot); 												\
}																																						\
																																						\
void RealInterleavedFFT_##precision##Complex(Function_##precision##Complex f, SoundEditorCache* cache)													\
{																																						\
	unsigned long long len = cache->length;																												\
	Function_##precision##Complex twiddleFactors = *CAST(cache->twiddleFactors, Function_##precision##Complex*);										\
																																						\
	/* These get used often.*/																															\
	unsigned long long halfLen = len / 2;																												\
																																						\
	FFT_##precision##Complex(f, cache);																													\
																																						\
	/* Applying postprocessing to extract the DFT of the original function from the DFT of the interleaved one.*/										\
	/* The math is according to the document linked in the header for this function.*/																	\
	/* get(f, 0) is a special case because there is no get(f, len - 0).*/																				\
	get(f, 0) = ForwardPostprocess_##precision##Complex(get(f, 0), get(f, 0), 1.0);																		\
																																						\
	/* Note: get(f, len / 2) doesn't need any extra processing. It already has the right value.*/														\
	/* We iterate over half the samples and postprocess 2 samples in each go because each k'th sample shares computations with the len-k'th one.*/		\
	/* This computation uses RootOfUnity(k, 2*len) for each 1<=k<=2*len. But our cache only contains RootOfUnity(k, len) for each 1<=k<=len/2.*/		\
	/* If we had RootOfUnity(k, 2*len) for each 1<=k<=len/2, it's easy to extract RootOfUnity(len - k, 2*len). The math is explained later.*/			\
	/* To overcome the first hurdle, that we have roots for length-N and not length-2N, we split postprocessing into even and odd indices.*/			\
	for (unsigned long long k = 1; k < halfLen; k += 2)																									\
	{																																					\
		/* For odd indices, RootOfUnity(k, 2*len)=csqrt(RootOfUnity(k, len)), and we have the root inside.*/											\
		precision##Complex root = csqrt_##precision##Complex(get(twiddleFactors, k));																	\
		ForwardPostprocessSymmetric_##precision##Complex(&get(f, k), &get(f, len - k), root);															\
	}																																					\
																																						\
	for (unsigned long long k = 2; k < halfLen; k += 2)																									\
	{																																					\
		/* For even indices, RootOfUnity(k, 2*len)=RootOfUnity(k/2, len), which we have cached.*/														\
		precision##Complex root	= get(twiddleFactors, k / 2);																							\
		ForwardPostprocessSymmetric_##precision##Complex(&get(f, k), &get(f, len - k), root);															\
	}																																					\
}																																						\
																																						\
/* This is an auxiliary function of InverseRealInterleavedFFT. It calculates the preprocessing step for it.*/											\
/* 'sample' is the k'th sample in the function, 'oppositeSample' is the N-k'th sample 'root' is the k/N'th root of unity.*/								\
 __attribute__((always_inline)) inline																													\
precision##Complex BackwardPreprocess_##precision##Complex(precision##Complex sample, precision##Complex oppositeSample, precision##Complex root)		\
{																																						\
	precision##Complex val = I * root;																													\
	precision##Complex coeffA = conj_##precision##Complex(CAST(1.0, precision##Complex) - val);															\
	precision##Complex coeffB = conj_##precision##Complex(CAST(1.0, precision##Complex) + val);															\
	return CAST(0.5, precision##Complex) * ((sample * coeffA) + (conj_##precision##Complex(oppositeSample) * coeffB));									\
}																																						\
																																						\
/* This is an auxiliary function of InverseRealInterleavedFFT. It symmetrically applies preprocessing to the k'th and N-k'th samples in the function.*/	\
/* 'sample' is the k'th sample in the function, 'oppositeSample' is the N-k'th sample 'root' is the k/N'th root of unity.*/								\
 __attribute__((always_inline)) inline																													\
void BackwardPreprocessSymmetric_##precision##Complex(precision##Complex* sample, precision##Complex* oppositeSample, precision##Complex root)			\
{																																						\
	/* Can't just dereference these when I need them because their values get changed and they both need both.*/										\
	precision##Complex sampleVal = *sample, oppositeSampleVal = *oppositeSample;																		\
																																						\
	/* root is assumed to be equal to RootOfUnity(k, 2N) where k is the index of 'sample' and N is the length of the interleaved function.*/			\
	/* I did some math, and the following expression equals RootOfUnity(N - k, 2N). N - k is assumed to be the index of 'oppositeSample'.*/				\
	precision##Complex oppositeRoot = CAST(-1.0, precision##Complex) / root; 																			\
	*sample = BackwardPreprocess_##precision##Complex(sampleVal, oppositeSampleVal, root);																\
	*oppositeSample = BackwardPreprocess_##precision##Complex(oppositeSampleVal, sampleVal, oppositeRoot); 												\
}																																						\
void InverseRealInterleavedFFT_##precision##Complex(Function_##precision##Complex f, SoundEditorCache* cache)											\
{																																						\
	unsigned long long len = cache->length;																												\
	Function_##precision##Complex twiddleFactors = *CAST(cache->twiddleFactors, Function_##precision##Complex*);										\
																																						\
	/* These get used often.*/																															\
	unsigned long long halfLen = len / 2;																												\
																																						\
	/* Applying preprocessing to revert back to the DFT of the interleaved function.*/																	\
	/* The math is from the same document as the one I used for the forward transform.*/																\
	/* get(f, 0) is a special case because there is no get(f, len - 0).*/																				\
	get(f, 0) = BackwardPreprocess_##precision##Complex(get(f, 0), get(f, 0), 1.0);  																	\
																																						\
	/* There's a lot of shit to explain about this, but it's identical to what's already explained in RealInterleavedFFT.*/								\
	/* Refer to the comments there for more information.*/																								\
	for (unsigned long long k = 1; k < halfLen; k += 2)																									\
	{																																					\
		precision##Complex root = csqrt_##precision##Complex(get(twiddleFactors, k));																	\
		BackwardPreprocessSymmetric_##precision##Complex(&get(f, k), &get(f, len - k), root);															\
	}																																					\
																																						\
	for (unsigned long long k = 2; k < halfLen; k += 2)																									\
	{																																					\
		precision##Complex root	= get(twiddleFactors, k / 2);																							\
		BackwardPreprocessSymmetric_##precision##Complex(&get(f, k), &get(f, len - k), root);															\
	}																																					\
																																						\
	/* Now applying inverse FFT. The result will be the original interleaved sequence of even and odd reals (with changes we've applied).*/				\
	InverseFFT_##precision##Complex(f, cache);																											\
}																																						\
																																						\
/* Most of the comments in this function refer to a picture of the recursion tree the algorithm follows, which I saw here:*/							\
/* https://www.geeksforgeeks.org/iterative-fast-fourier-transformation-polynomial-multiplication/*/														\
/* The algorithm itself is a modified version of this: https://stackoverflow.com/a/37729648/12553917. */												\
void FFT_##precision##Complex(Function_##precision##Complex f, SoundEditorCache* cache)																	\
{																																						\
	/* Bit-reversing the array sorts it by the order of the leaves in the recursion tree.*/																\
	BitReverseArr_##precision##Complex(f, cache);																										\
																																						\
	unsigned long long len = cache->length;																												\
	unsigned long long halfLen = len / 2;																												\
	unsigned int logLen = cache->logLength;																												\
	Function_##precision##Complex twiddleFactors = *CAST(cache->twiddleFactors, Function_##precision##Complex*);										\
																																						\
	/* The stride is the length of each sub-tree in the current level.*/																				\
	/* We start from 2 because the level with length-1 sub-trees is trivial and assumed to already be contained in the array.*/							\
	unsigned long long stride = 2;																														\
	unsigned long long halfStride = 1;																													\
	unsigned long long lenDivStride = halfLen;																											\
																																						\
	/* Each iteration of this loop climbs another level up the recursion tree.*/																		\
	for (unsigned int j = 0; j < logLen; j++)																											\
	{																																					\
		/* Each iteration of this loop moves to the next element in the current tree.*/																	\
		/* We only need to iterate over half the elements because in each iteration, we calculate two indices together.*/								\
		for (unsigned long long k = 0; k < halfStride; k++)																								\
		{																																				\
			/* Caching repeatedly-used calculations.*/																									\
			unsigned long long kPlusHalfStride = k + halfStride; 																						\
			precision##Complex factor = get(twiddleFactors, (k * lenDivStride) % halfLen);																\
																																						\
			/* Each iteration of this loop moves to the next tree in the current level.*/																\
			for (unsigned long long i = 0; i < len; i += stride)																						\
			{																																			\
				/* i serves as a sort of "base" for the current tree. i + k is the k'th element in the (i / stride)'th tree of this level.*/			\
				precision##Complex evenSum = get(f, i + k);																								\
				precision##Complex oddSum = factor * get(f, kPlusHalfStride + i);											 							\
				get(f, i + k) = evenSum + oddSum;																										\
				get(f, kPlusHalfStride + i) = evenSum - oddSum;																							\
			}																																			\
		}																																				\
																																						\
		/* In the next level, trees will be twice as big.*/																								\
		stride *= 2;																																	\
		halfStride *= 2;																																\
		lenDivStride /= 2;																																\
	}																																					\
}																																						\
																																						\
void InverseFFT_##precision##Complex(Function_##precision##Complex f, SoundEditorCache* cache)															\
{																																						\
	unsigned long long len = cache->length;																												\
																																						\
	/* Conjugating every sample before applying forward FFT.*/																							\
	for (unsigned long long i = 0; i < len; i++)																										\
	{																																					\
		get(f, i) = conj_##precision##Complex(get(f, i));																								\
	}																																					\
																																						\
	/* Applying forward fft.*/																															\
	FFT_##precision##Complex(f, cache);																													\
																																						\
	/* Conjugating again and dividing by the num of samples.*/																							\
	for (unsigned long long i = 0; i < len; i++)																										\
	{																																					\
		get(f, i) = conj_##precision##Complex(get(f, i)) / len;																							\
	}																																					\
}

SOUND_EDITOR_C_PRECISION_CONTENTS(Double)
SOUND_EDITOR_C_PRECISION_CONTENTS(Float)

SoundEditorCache* InitializeSoundEditor(Function* f)
{
	switch (GetType(f))
	{
		case FloatComplexType:
			return InitializeSoundEditor_FloatComplex(f);
		case DoubleComplexType:
			return InitializeSoundEditor_DoubleComplex(f);
		default:
			fprintf(stderr, "Tried to initialize the sound editor to an invalid type.\n");
			return NULL;
	}
}

void DeallocateSoundEditorCache(SoundEditorCache* cache)
{
	if (cache == NULL)
	{
		return;
	}
		
	// DeallocateFunctionInternals as well as free can handle NULL pointers no problem.
	DeallocateFunctionInternals(cache->twiddleFactors);
	free(cache->twiddleFactors);
	free(cache);
}

void RealInterleavedFFT(Function* f, SoundEditorCache* cache)
{
	switch (GetType(f))
	{
		case FloatComplexType:
			RealInterleavedFFT_FloatComplex(*((Function_FloatComplex*)f), cache);
			break;
		case DoubleComplexType:
			RealInterleavedFFT_DoubleComplex(*((Function_DoubleComplex*)f), cache);
			break;
		default:
			fprintf(stderr, "Tried to FFT an invalid type.\n");
			break;
	}
}

void InverseRealInterleavedFFT(Function* f, SoundEditorCache* cache)
{
	switch (GetType(f))
	{
		case FloatComplexType:
			InverseRealInterleavedFFT_FloatComplex(*((Function_FloatComplex*)f), cache);
			break;
		case DoubleComplexType:
			InverseRealInterleavedFFT_DoubleComplex(*((Function_DoubleComplex*)f), cache);
			break;
		default:
			fprintf(stderr, "Tried to IFFT an invalid type.\n");
			break;
	}
}

unsigned long long BitReverse(unsigned int digits, unsigned long long n)
{
	unsigned long long reversed = 0;

	for (int i = 0; i < digits; i++)
	{
		reversed <<= 1;
		reversed |= (n & 1);
		n >>= 1;
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