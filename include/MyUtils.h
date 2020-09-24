#ifndef MY_UTILS_H
#define MY_UTILS_H

#include <windows.h>	// For some winapi types.
#include <complex.h>	// For dealing with complex numbers.

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

// TODO: declare any of these that make sense as inline.

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

// Returns the square magnitude of a single-precision float complex.
float SquareMagnitudeFloatComplex(float complex);

// Returns the square magnitude of a double-precision float complex.
double SquareMagnitudeDoubleComplex(double complex);

// Converts a linear single-precision float value to decibel units based on a reference point.
float LinearToDecibelFloatReal(float, float);

// Converts a linear double-precision float value to decibel units based on a reference point.
double LinearToDecibelDoubleReal(double, double);

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