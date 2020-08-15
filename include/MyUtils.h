#include <windows.h>

// Returns a random int in the range [min, max)
int RandRange(int min, int max);

// A generic bubblesort.
void Bubblesort(void*, int, char (*)(void*, void*), size_t);

// A generic quicksort.
void QuickSort(void*, int, int, char (*)(void*, void*), size_t);

// Chooses a random pivot and places it at its correct position such that everything before it is smaller and everything after it is bigger.
int Partition (void*, int, int, char (*)(void*, void*), size_t);

// Swaps two elements in an array.
void Swap(void*, void*, size_t);
  
  