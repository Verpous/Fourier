#ifndef MY_UTILS_INTERNAL_H
#define MY_UTILS_INTERNAL_H

#include "MyUtils.h"

// Chooses a random pivot and places it at its correct position such that everything before it is smaller and everything after it is bigger.
int Partition (void*, int, int, char (*)(void*, void*), size_t);

// Swaps two elements in an array.
void Swap(void*, void*, size_t);

#endif