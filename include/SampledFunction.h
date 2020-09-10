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
    unsigned long long totalLen;                                                                                            \
    type** samples;                                                                                                         \
} Function_##type;                                                                                                          \
                                                                                                                            \
/* Initializes a function, including allocating its samples array. Returns zero iff there was a memory allocation error.*/  \
char AllocateFunctionInternals_##type(Function_##type*, unsigned long long);                                                \
                                                                                                                            \
/* Deallocates a function.*/                                                                                                \
void DeallocateFunctionInternals_##type(Function_##type*);                                                                  \
                                                                                                                            \
/* Clones the source function in the given sample range.*/                                                                  \
Function_##type* CreatePartialClone_##type(Function_##type*, unsigned long long, unsigned long long);                       \
                                                                                                                            \
/* Copies the samples from the source function to the destination function, from their given starting points.*/             \
void CopySamples_##type(Function_##type, Function_##type, unsigned long long, unsigned long long, unsigned long long);

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
#endif