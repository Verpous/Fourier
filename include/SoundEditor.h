#ifndef SOUND_EDITOR_H
#define SOUND_EDITOR_H

#include "SampledFunction.h"

typedef enum
{
	MULTIPLY,
	ADD,
} ChangeType;

typedef struct Modification
{
	Function* oldFunc;				// The values that were in place before the modification. We only store samples that were changed.
	unsigned long long startSample; // The sample from which the modification was applied.
	ChangeType changeType;			// Whether the modification was additive or multiplicative.
	double changeAmount;			// How much was changed.
	double smoothing;				// How smoothed the change was. 0 for totally square, 1 for totally curved.
	unsigned short channel;			// The channel that the change was applied to.
	struct Modification* prev;		// The modification before this one.
	struct Modification* next;		// The modification after this one.
} Modification;

// In the future this could be expanded into a struct containing more than just the twiddle factors as a function.
typedef struct
{
	Function* twiddleFactors; 		// An array of all the twiddle factors up to the N/2'th one when N is the length of the complex interleaved functions.
	unsigned long long length;		// The length of complex interleaved functions this cache is for. This is assumed to be a power of two and at least 8.
	unsigned long long logLength;	// The log2 of the length field above.
} SoundEditorCache;

// Creates a cache of things the sound editor wants to reuse as long as it's dealing with real interleaved functions of the same length and type as the one given.
// Returns NULL in case of failure, which is probably caused by there not being enough memory available.
SoundEditorCache* InitializeSoundEditor(Function*);

// Deallocates the given cache.
void DeallocateSoundEditorCache(SoundEditorCache*);

// Applies an FFT to the function assuming that it's a real function in complex interleaved form, as described in this document:
// https://www.ti.com/lit/an/spra291/spra291.pdf?ts=1597858546752&ref_url=https%253A%252F%252Fwww.google.co.il%252F.
// Basically, f is treated as if it's a complex sequence where the real parts correspond to even indices of a real sequence g, and the imaginary parts correspond to odds.
void RealInterleavedFFT(Function*, SoundEditorCache*);

// Applies an IFFT to the function assuming it's a real function in complex interleaved form.
void InverseRealInterleavedFFT(Function*, SoundEditorCache*);

// Applies a modification to the function in the given channel and stores the modification in the modifications stack. Returns zero iff there was a memory allocation error.
char ApplyModification(unsigned long long, unsigned long long, ChangeType, double, double, unsigned short, Function**, Modification**);

// Given the channel functions and the modification stack, redoes the change that was last undone. Returns nonzero value iff there were changes to redo.
char RedoLastModification(Function**, Modification**);

// Given the channel functions and modification stack, undoes the change at the top of the stack. Returns nonzero value iff there were changes to undo.
char UndoLastModification(Function**, Modification**);

// Returns nonzero value iff there are changes that can be redone.
char CanRedo(Modification*);

// Returns nonzero value iff there are changes that can be undone.
char CanUndo(Modification*);

// Returns the channel that would be affected by the redo button at this time. Assumes that CanRedo is true, do not call this otherwise.
unsigned short GetRedoChannel(Modification*);

// Returns the channel that would be affected by the undo button at this time. Assumes that CanUndo is true, do not call this otherwise.
unsigned short GetUndoChannel(Modification*);

// Returns nonzero iff the given modification is on the list of modifications to undo in the modification stack.*/
char IsUndoable(Modification*, Modification*);

// Returns nonzero iff the given modification is on the list of modifications to redo in the modification stack.*/
char IsRedoable(Modification*, Modification*);

// Allocates and initializes a new modification stack.
void InitializeModificationStack(Modification**);

// Deallocates memory associated with the given modification stack.
void DeallocateModificationStack(Modification*);

#endif