#include <windows.h>

#define KILOBYTE (1 << 10)
#define MEGABYTE (1 << 20)
#define GIGABYTE (1 << 30)

#define KILOBYTES(k) (k * KILOBYTE)
#define MEGABYTES(k) (k * MEGABYTE)
#define GIGABYTES(k) (k * GIGABYTE)

// Returns a random int in the range [min, max)
int RandRange(int, int);

// A generic bubblesort.
void Bubblesort(void*, int, char (*)(void*, void*), size_t);

// A generic quicksort.
void QuickSort(void*, int, int, char (*)(void*, void*), size_t);

// Takes a number and returns nonzero value iff it's a power of two.
char IsPowerOfTwo(unsigned long long);
  