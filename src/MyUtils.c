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

#include "MyUtils.h"
#include <stdlib.h>

int RandRange(int min, int max)
{
    return (rand() % (max - min)) + min;
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