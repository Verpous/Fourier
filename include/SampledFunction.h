#ifndef SAMPLED_FUNCTION_H
#define SAMPLED_FUNCTION_H

#include <complex.h>
#include <float.h>

typedef enum
{
	DoubleComplexType,
	FloatComplexType,
	DoubleRealType,
	FloatRealType,
} FunctionType;

// This will be used to hide the varying types functions can have from the code using them
typedef void Function;
typedef double complex DoubleComplex;
typedef float complex FloatComplex;
typedef double DoubleReal;
typedef float FloatReal;

// Gets a function f and an index i and returns the i'th sample of f.
#define get(f, i) ((f).samples[(i) / (f).segmentLen][(i) % (f).segmentLen])

// Macros that rename float functions for different precisions.
// First, things that are only available for complex numbers.
#define conj_FloatComplex(x) conjf(x)
#define conj_DoubleComplex(x) conj(x)

#define carg_FloatComplex(x) cargf(x)
#define carg_DoubleComplex(x) carg(x)

#define creal_FloatComplex(x) crealf(x)
#define creal_DoubleComplex(x) creal(x)

#define cimag_FloatComplex(x) cimagf(x)
#define cimag_DoubleComplex(x) cimag(x)

// Now things that are only available for real numbers.
#define floor_FloatReal(x) floorf(x)
#define floor_DoubleReal(x) floor(x)

#define ceil_FloatReal(x) ceilf(x)
#define ceil_DoubleReal(x) ceil(x)

#define lround_FloatReal(x) lroundf(x)
#define lround_DoubleReal(x) lround(x)

#define llround_FloatReal(x) llroundf(x)
#define llround_DoubleReal(x) llround(x)

#define llfloor_FloatReal(x) llroundf(floorf(x))
#define llfloor_DoubleReal(x) llround(floor(x))

#define llceil_FloatReal(x) llroundf(ceilf(x))
#define llceil_DoubleReal(x) llround(ceil(x))

#define sincos_FloatReal(x, s, c) sincosf(x, s, c)
#define sincos_DoubleReal(x, s, c) sincos(x, s, c)

#define log10_FloatReal(x) log10f(x)
#define log10_DoubleReal(x) log10(x)

#define MAX_FloatReal __FLT_MAX__
#define MAX_DoubleReal __DBL_MAX__

// Now things that are available for both.
#define abs_FloatComplex(x) cabsf(x)
#define abs_DoubleComplex(x) cabs(x)
#define abs_FloatReal(x) fabsf(x)
#define abs_DoubleReal(x) fabs(x)

#define sqrt_FloatComplex(x) csqrtf(x)
#define sqrt_DoubleComplex(x) csqrt(x)
#define sqrt_FloatReal(x) sqrtf(x)
#define sqrt_DoubleReal(x) sqrt(x)

#define exp_FloatComplex(x) cexpf(x)
#define exp_DoubleComplex(x) cexp(x)
#define exp_FloatReal(x) expf(x)
#define exp_DoubleReal(x) exp(x)

#define cos_FloatComplex(x) ccosf(x)
#define cos_DoubleComplex(x) ccos(x)
#define cos_FloatReal(x) cosf(x)
#define cos_DoubleReal(x) cos(x)

#define sin_FloatComplex(x) csinf(x)
#define sin_DoubleComplex(x) csin(x)
#define sin_FloatReal(x) sinf(x)
#define sin_DoubleReal(x) sin(x)

#define SAMPLED_FUNCTION_H_TYPED_CONTENTS(type)																																\
typedef struct																																								\
{																																											\
	FunctionType funcType;																																					\
	unsigned long long segmentLen;																																			\
	unsigned long long segmentCount;																																		\
	unsigned long long totalLen;																																			\
	type** samples;																																							\
} Function_##type;																																							\
																																											\
/* Initializes a function, including allocating its samples array. Returns zero iff there was a memory allocation error.*/													\
/* If an error does occur, memory allocated before the error does not get freed. You must call DeallocateFunctionInternals yourself to avoid memory leaks.*/				\
char AllocateFunctionInternals_##type(Function_##type*, unsigned long long);																								\
																																											\
/* Deallocates a function.*/																																				\
void DeallocateFunctionInternals_##type(Function_##type*);																													\
																																											\
/* Clones the source function in the given sample range.*/																													\
Function_##type* CreatePartialClone_##type(Function_##type*, unsigned long long, unsigned long long);																		\
																																											\
/* Copies the samples from the source function to the destination function, from their given starting points.*/																\
void CopySamples_##type(Function_##type, Function_##type, unsigned long long, unsigned long long, unsigned long long);														\
																																											\
/* Returns the biggest sample between two indices [inclusive, exclusive), while skipping over samples according to the given step size.*/									\
type GetMax_##type(Function_##type, unsigned long long, unsigned long long, unsigned long long);																			\
																																											\
/* Returns the smallest sample between startIndex (inclusive) and endIndex (exclusive).*/																					\
type GetMin_##type(Function_##type, unsigned long long, unsigned long long, unsigned long long);

#define SAMPLED_FUNCTION_H_PRECISION_CONTENTS(precision)																													\
/* Creates a real function which reads from the same data as the given complex function, but every complex number is read as two real numbers with adjacent indices.*/		\
/* This is very useful for dealing with real functions in complex interleaved form.*/																						\
/* Do not call DeallocateFunctionInternals on the returned function unless you are also done with the original complex function.*/											\
Function_##precision##Real ReadComplexFunctionAsReal_##precision##Complex(Function_##precision##Complex*);

/* Returns how many samples are used to represent f.*/
unsigned long long NumOfSamples(Function*);

// Returns the function's type.
FunctionType GetType(Function*);

// Deallocates memory allocated by the function (not the function itself).
void DeallocateFunctionInternals(Function*);

// Clones the source function in the given sample range.
Function* CreatePartialClone(Function*, unsigned long long, unsigned long long);

// Copies the samples from the source function to the destination function, from their given starting points.
void CopySamples(Function*, Function*, unsigned long long, unsigned long long, unsigned long long);

SAMPLED_FUNCTION_H_TYPED_CONTENTS(FloatComplex)
SAMPLED_FUNCTION_H_TYPED_CONTENTS(DoubleComplex)
SAMPLED_FUNCTION_H_TYPED_CONTENTS(FloatReal)
SAMPLED_FUNCTION_H_TYPED_CONTENTS(DoubleReal)

SAMPLED_FUNCTION_H_PRECISION_CONTENTS(Float)
SAMPLED_FUNCTION_H_PRECISION_CONTENTS(Double)

#endif