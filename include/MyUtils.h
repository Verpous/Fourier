#include <windows.h>

#define KILO (1 << 10)
#define MEGA (1 << 20)
#define GIGA (1 << 30)

#define KILOS(k) ((k) * KILO)
#define MEGAS(k) ((k) * MEGA)
#define GIGAS(k) ((k) * GIGA)

#define CAST(f, type) ((type)(f))

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