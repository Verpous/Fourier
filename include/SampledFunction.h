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

#define SAMPLED_FUNCTION_H_TYPED_CONTENTS(type)                                                                             \
typedef struct Function_##type                                                                                              \
{                                                                                                                           \
    FunctionType funcType;                                                                                                  \
    unsigned long long segmentLen;                                                                                          \
    unsigned long long segmentCount;                                                                                        \
    type** samples;                                                                                                         \
} Function_##type;                                                                                                          \
                                                                                                                            \
/* Initializes a function, including allocating its samples array. Returns zero iff there was a memory allocation error.*/  \
char AllocateFunction_##type(Function_##type*, unsigned long long);                                                         \
                                                                                                                            \
/* Deallocates a function.*/                                                                                                \
void DeallocateFunction_##type(Function_##type*);                                                                           \
                                                                                                                            \
/* Returns how many samples are used to represent f.*/                                                                      \
unsigned long long NumOfSamples_##type(Function_##type*);
unsigned long long NumOfSamples(Function*);

// Returns the function's type.
FunctionType GetType(Function*);

// Deallocates memory allocated by the function (not the function itself).
void DeallocateFunction(Function*);

SAMPLED_FUNCTION_H_TYPED_CONTENTS(FloatComplex)
SAMPLED_FUNCTION_H_TYPED_CONTENTS(DoubleComplex)
#endif