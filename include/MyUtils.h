#ifndef MY_UTILS_H
#define MY_UTILS_H

#include <windows.h>	// For some winapi types.
#include <complex.h>	// For dealing with complex numbers.
#include <stdlib.h>		// For memcpy, rand, etc.
#include <limits.h>		// For CHAR_BIT and max unsigned long long.
#include <math.h>		// For functions like log10.

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

// Returns nonzero iff the string represents a float in scientific notation. If the string doesn't contain a float in any representation, the behavior is undefined.
char IsScientificNotation(TCHAR*);


// The following functions are inline and therefore must be defined in the header.

// The C '%' operator is the remainder, so this does modulus.
 __attribute__((always_inline)) inline
int Modulus(int numerator, int denominator)
{
	return ((numerator % denominator) + denominator) % denominator;
}

// The mathematical sign function.
 __attribute__((always_inline)) inline
int Sign(int num)
{
	return num > 0 ? 1 : num == 0 ? 0 : -1;
}

// Returns a random long long out of all the possible long long values.
 __attribute__((always_inline)) inline
long long RandLong()
{
	return (((long long)rand()) << (sizeof(int) * CHAR_BIT)) | rand();
}

// Returns a random int in the range [min, max).
 __attribute__((always_inline)) inline
int RandRangeInt(int min, int max)
{
	return Modulus(rand(), max - min) + min;
}

// Returns a random single-precision float between [min, max].
 __attribute__((always_inline)) inline
float RandRangeFloat(float min, float max)
{
	return min + ((((unsigned int)rand()) / ((float)UINT_MAX)) * (max - min));
}

// Returns a random double-precision float between [min, max].
 __attribute__((always_inline)) inline
double RandRangeDouble(double min, double max)
{
	return min + ((((unsigned long long)RandLong()) / ((double)ULONG_LONG_MAX)) * (max - min));
}

// Clamps a single-precision float between two values.
 __attribute__((always_inline)) inline
float ClampFloat(float val, float min, float max)
{
	return val > max ? max : val < min ? min : val;
}

// Clamps a double-precision float between two values.
 __attribute__((always_inline)) inline
double ClampDouble(double val, double min, double max)
{
	return val > max ? max : val < min ? min : val;
}

// Clamps an integer between two values [inclusive, inclusive].
 __attribute__((always_inline)) inline
long long ClampInt(long long val, long long min, long long max)
{
	return val > max ? max : val < min ? min : val;
}

// Returns the square magnitude of a single-precision float complex.
 __attribute__((always_inline)) inline
float SquareMagnitudeFloatComplex(float complex val)
{
	float realPart = crealf(val);
	float imagPart = cimagf(val);
	return (realPart * realPart) + (imagPart * imagPart);
}

// Returns the square magnitude of a double-precision float complex.
 __attribute__((always_inline)) inline
double SquareMagnitudeDoubleComplex(double complex val)
{
	double realPart = creal(val);
	double imagPart = cimag(val);
	return (realPart * realPart) + (imagPart * imagPart);
}

// Converts a linear single-precision float value to decibel units based on a reference point.
 __attribute__((always_inline)) inline
float LinearToDecibelFloatReal(float val, float reference)
{
	return 10 * log10f(val / reference);
}

// Converts a linear double-precision float value to decibel units based on a reference point.
 __attribute__((always_inline)) inline
double LinearToDecibelDoubleReal(double val, double reference)
{
	return 10 * log10(val / reference);
}

// Returns the division with ceiling of two integers.
 __attribute__((always_inline)) inline
long long DivCeilInt(long long numerator, long long denominator)
{
	lldiv_t divResult = lldiv(numerator, denominator);
	return divResult.quot + (divResult.rem == 0 ? 0 : 1);
}

// Swaps the memory contents of the given addresses.
 __attribute__((always_inline)) inline
void Swap(void* a, void* b, size_t size)
{
	char temp[size];
	memcpy(temp, a, size);
	memcpy(a, b, size);
	memcpy(b, temp, size);
}

#endif