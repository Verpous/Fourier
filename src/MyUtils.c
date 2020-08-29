// Fourier - a program for modifying the weights of different frequencies in a wave file.
// Copyright (C) 2020 Aviv Edery.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "MyUtilsInternal.h"
#include <stdlib.h> // For memcpy, rand, etc.
#include <limits.h> // For CHAR_BIT and max unsigned long long.

int RandRange(int min, int max)
{
    return (rand() % (max - min)) + min;
}

long long RandLong()
{
    return (((long long)rand()) << (sizeof(int) * CHAR_BIT)) | rand();
}

double RandRangeDouble(double min, double max)
{
    return min + (((RandLong() + 0.5) / (LONG_LONG_MAX - 0.5)) * (max - min));
}

float RandRangeFloat(float min, float max)
{
    return min + (((rand() + 0.5) / (INT_MAX - 0.5)) * (max - min));
}

float ClampFloat(float val, float min, float max)
{
    return val > max ? max : val < min ? min : val;
}

double ClampDouble(double val, double min, double max)
{
    return val > max ? max : val < min ? min : val;
}

void Swap(void* a, void* b, size_t size) 
{ 
    char temp[size];
    memcpy(temp, a, size);
    memcpy(a, b, size);
    memcpy(b, temp, size);
}

void Bubblesort(void* arr, int length, char (*comparator)(void*, void*), size_t size)
{ 
    int i, j; 

    for (i = 0; i < length - 1; i++)       
    {
        void* cur;
        void* next = arr;

        for (j = 0; j < length - i - 1; j++)
        {  
            cur = next;
            next += size;

            if (comparator(next, cur))
            {
                Swap(cur, next, size); 
            }
        }
    }
} 
  
void Quicksort(void* arr, int low, int high, char (*comparator)(void*, void*), size_t size) 
{ 
    if (low < high) 
    { 
        // After this line, element at this index is placed correctly.
        int partitionIndex = Partition(arr, low, high, comparator, size); 

        // Recursing.
        Quicksort(arr, low, partitionIndex - 1, comparator, size); 
        Quicksort(arr, partitionIndex + 1, high, comparator, size); 
    }
}

int Partition(void* arr, int low, int high, char (*comparator)(void*, void*), size_t size) 
{ 
    int rand = RandRange(low, high + 1);
    void* pivot = arr + (high * size);
    Swap(arr + (rand * size), pivot, size);
    
    int i = (low - 1);
  
    for (int j = low; j <= high - 1; j++) 
    {
        if (comparator(arr + (j * size), pivot))
        { 
            Swap(arr + (i * size), arr + (j * size), size);
            i++;
        } 
    } 

    Swap(arr + ((i + 1) * size), pivot, size); 
    return (i + 1);
}

char IsPowerOfTwo(unsigned long long n)
{
    return (n != 0) && ((n & (n - 1)) == 0);
}

unsigned int CountTrailingZeroes(unsigned long long N)
{
    unsigned int i = 0;
    unsigned long long mask = 1;
    
    while (i < sizeof(N) * CHAR_BIT)
    {
        if ((mask & N) != 0)
        {
            break;
        }

        mask <<= 1;
        i++;
    }

    return i;
}

unsigned int CountLeadingZeroes(unsigned long long N)
{
    unsigned int i = 0;
    unsigned long long mask = 1ULL << (sizeof(N) * CHAR_BIT - 1);
    
    while (i < sizeof(N) * CHAR_BIT)
    {
        if ((mask & N) != 0)
        {
            break;
        }

        mask >>= 1;
        i++;
    }

    return i;
}

unsigned long long NextPowerOfTwo(unsigned long long N)
{
    if (IsPowerOfTwo(N))
    {
        return N;
    }
    else
    {
        // The number isn't a power of two, so we take the number 100...0, and right shift it such that the 1 bit is one left of N's highest set bit.
        unsigned int leadingZeroes = CountLeadingZeroes(N);
        return (1ULL << (sizeof(N) * CHAR_BIT - 1ULL)) >> (leadingZeroes - 1ULL);
    }
}