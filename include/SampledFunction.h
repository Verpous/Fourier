#ifndef SAMPLED_FUNCTION_H
#define SAMPLED_FUNCTION_H

#include <complex.h>

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

#define SAMPLED_FUNCTION_H_TYPED_CONTENTS(type)																																\
typedef struct Function_##type																																				\
{																																											\
	FunctionType funcType;																																					\
	unsigned long long segmentLen;																																			\
	unsigned long long segmentCount;																																		\
	unsigned long long totalLen;																																			\
	type** samples;																																							\
} Function_##type;																																							\
																																											\
/* Initializes a function, including allocating its samples array. Returns zero iff there was a memory allocation error.*/													\
char AllocateFunctionInternals_##type(Function_##type*, unsigned long long);																								\
																																											\
/* Deallocates a function.*/																																				\
void DeallocateFunctionInternals_##type(Function_##type*);																													\
																																											\
/* Clones the source function in the given sample range.*/																													\
Function_##type *CreatePartialClone_##type(Function_##type*, unsigned long long, unsigned long long);																		\
																																											\
/* Copies the samples from the source function to the destination function, from their given starting points.*/																\
void CopySamples_##type(Function_##type, Function_##type, unsigned long long, unsigned long long, unsigned long long);														\
																																											\
/* Returns the biggest sample between startIndex (inclusive) and endIndex (exclusive).*/																					\
type GetMax_##type(Function_##type, unsigned long long, unsigned long long);																								\
																																											\
/* Returns the smallest sample between startIndex (inclusive) and endIndex (exclusive).*/																					\
type GetMin_##type(Function_##type, unsigned long long, unsigned long long);

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