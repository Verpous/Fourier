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

#define get(f, i) (f.samples[i / f.segmentLen][i % f.segmentLen])
#define NumOfSamples(f) (f.segmentCount * f.segmentCount)
#define GetType(f) (*((FunctionType*)f))

#define SAMPLED_FUNCTION_H_TYPED_CONTENTS(type)                                                                             \
typedef struct Function_##type                                                                                              \
{                                                                                                                           \
    FunctionType funcType;                                                                                                  \
    type** samples;                                                                                                         \
    unsigned long long segmentLen;                                                                                          \
    unsigned long long segmentCount;                                                                                        \
} Function_##type;                                                                                                          \
                                                                                                                            \
/* Initializes a function, including allocating its samples array. Returns zero iff there was a memory allocation error.*/  \
char AllocateFunction_##type(Function_##type*, unsigned long long);

SAMPLED_FUNCTION_H_TYPED_CONTENTS(FloatComplex)
SAMPLED_FUNCTION_H_TYPED_CONTENTS(DoubleComplex)
#endif