#ifndef MY_UTILS_H
#define MY_UTILS_H

#include <windows.h> // For some winapi types.

#define KILO (1 << 10)
#define MEGA (1 << 20)
#define GIGA (1 << 30)

#define KILOS(k) ((k) * KILO)
#define MEGAS(k) ((k) * MEGA)
#define GIGAS(k) ((k) * GIGA)

// Casts x to type, sometimes it's prettier and easier to type this way.
#define CAST(x, type) ((type)(x))

// Turns any text you give it into a string.
#define Stringify(x) #x

// Like Stringify, but it will stringify macro values instead of names if you give it a macro.
#define XStringify(x) Stringify(x)

// Like Stringify, but the string is unicode if we're targeting unicode.
#define TStringify(x) TEXT(Stringify(x))

// Like XStringify, but the string is unicode if we're targeting unicode.
#define TXStringify(x) TEXT(XStringify(x))

// Macros that rename float functions for different precisions.
#define cexp_DoubleComplex(x) cexp(x)
#define cexp_FloatComplex(x) cexpf(x)

#define conj_DoubleComplex(x) conj(x)
#define conj_FloatComplex(x) conjf(x)

#define carg_DoubleComplex(x) carg(x)
#define carg_FloatComplex(x) cargf(x)

#define cabs_DoubleComplex(x) cabs(x)
#define cabs_FloatComplex(x) cabsf(x)

#define cos_DoubleReal(x) cos(x)
#define cos_FloatReal(x) cosf(x)

#define ceil_DoubleReal(x) ceil(x)
#define ceil_FloatReal(x) ceilf(x)

#define floor_DoubleReal(x) floor(x)
#define floor_FloatReal(x) floorf(x)

#define abs_DoubleReal(f) fabs(f)
#define abs_FloatReal(f) fabsf(f)

#define lround_DoubleReal(f) lround(f)
#define lround_FloatReal(f) lroundf(f)

// Returns a random int in the range [min, max).
int RandRange(int, int);

// Returns a random long long out of all the possible long long values.
long long RandLong();

// Returns a random single-precision float between [min, max].
float RandRangeFloat(float, float);

// Returns a random double-precision float between [min, max].
double RandRangeDouble(double, double);

// Clamps a single-precision float between two values.
float ClampFloat(float, float, float);

// Clamps a double-precision float between two values.
double ClampDouble(double, double, double);

// Clamps an integer between two values.
long long ClampInt(long long, long long, long long);

// Returns the division with ceiling of two integers.
long long DivCeilInt(long long, long long);

// A generic bubblesort.
void Bubblesort(void*, int, char (*)(void*, void*), size_t);

// A generic quicksort.
void QuickSort(void*, int, int, char (*)(void*, void*), size_t);

// Takes a number and returns nonzero value iff it's a power of two.
char IsPowerOfTwo(unsigned long long);

// Counts how many least significant zero-bits there are before the first set bit.
unsigned int CountTrailingZeroes(unsigned long long);

// Counts how many most significant zero-bits there are before the first 1-bit.
unsigned int CountLeadingZeroes(unsigned long long);

// Returns the smallest power of two that's greater/equal to the given number.
unsigned long long NextPowerOfTwo(unsigned long long);

// Returns TRUE iff a file with the given path (path includes the filename) exists.
char FileExists(LPCTSTR path);

#endif