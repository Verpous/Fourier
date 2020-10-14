#ifndef WINDOWS_MANAGER_H
#define WINDOWS_MANAGER_H

#include "WaveReadWriter.h"
#include "SoundEditor.h"
#include <windows.h>

typedef enum
{
	TIME_DOMAIN = 0, // Setting TIME_DOMAIN to 0 makes it the default, works nicely with calloc.
	FREQUENCY_DOMAIN = 1
} FunctionDomain;

// This struct holds handles to important windows in the new file options dialog so we can reference them.
typedef struct
{
	HWND handle;
	HWND parent;
	HWND lengthTrackbar;
	HWND lengthTextbox;
	HWND frequencyTrackbar;
	HWND frequencyTextbox;
	HWND depthOptions[4];
} NewFileOptionsWindow;

// This file holds the selections from the last occurence of the new file options dialog (or default values if it hasn't been opened yet) so they carry over between newly created files within the same instance of the program.
typedef struct
{
	unsigned int length;
	unsigned int frequency;
	unsigned int byteDepth;
} NewFileOptionsSelections;

typedef struct
{
	FileInfo* fileInfo;					// Info about the file that's being edited, such as its format, size, etc.
	Function** channelsData;			// An array of function pointers. This can be either the waveform or the DFT, we swap between them.
	SoundEditorCache* soundEditorCache;	// A cache that the sound editor uses to speed up calculations.
	FunctionDomain* channelsDomain;		// An array that contains the current domains of all the channels (time if it currently has the waveform, frequency if it currently has the DFT).

	HBITMAP* waveformGraphs;			// An array of bitmaps including graphs of all the channels' waveforms. NULL for channels that aren't drawn yet.
	HBITMAP* fourierGraphs;				// An array of bitmaps including graphs of all the channels' fourier transforms. NULL for channels that aren't drawn yet.
	unsigned short* fourierGraphsPeaks;	// An array of decibel values to be used as the highest point for the channels' fourier transform graphs.
	HDC graphingDC;						// The device context used to paint all the waveform and frequency graphs.
	HDC currentFourierDC;				// The device context we use to paint the current fourier graph with selection. graphingDC is caught in this time because we blit the graph from it.
	HBRUSH selectionBrush;				// The brush we use to paint frequency range selection onto the fourier graph.
	HBITMAP currentFourierGraph;		// A buffer we use to double-buffer the painting of fourier graphs in order to fix flickering when selecting a frequency range.

	char isSelecting;					// True iff the user is currently dragging the mouse around to select frequencies.
	double selectionPivot;				// While isSelecting is true, this contains the frequency at which the selection started.

	Modification* modificationStack;	// A stack of all the changes the user applies, for undoing and redoing them. Only NULL when no file is open.
	Modification* currentSaveState;		// The last change that was saved.

	// The following fields are just handles to some important windows of the file editor.
	HWND channelTabs;					// The tab control that lets you switch between channels.
	HWND waveformGraphStatic;			// The static which contains the waveform graph bitmap.
	HWND fourierGraphStatic;			// The static which contains the fourier transform graph bitmap.
	HWND fourierMaxStatic;				// The static which contains the decibel value at the highest point on the fourier transform graph.
	HWND hoverFrequencyStatic;			// The static which contains the frequency that's currently being hovered over.
	HWND minFreqStatic;					// The static which contains the lowest frequency. This is always 0Hz, but the units can vary.
	HWND maxFreqStatic;					// The static which contains the highest frequency, which is the nyquist frequency.
	HWND fromFreqTextbox;				// The textbox for inputting the frequency to edit from.
	HWND toFreqTextbox;					// The textbox for inputting the frequency to edit to.
	HWND changeTypeDropdown;			// The dropdown for selecting whether to apply a multiplicative, additive, or subtractive change.
	HWND changeAmountTextbox;			// The textbox for inputting the amount to multiply, add, or subtract.
	HWND smoothingTrackbar;				// The trackbar inputting the amount of smoothing to apply to the change.
	HWND smoothingTextbox;				// The textbox inputting the amount of smoothing to apply to the change (goes with the trackbar).
	HWND undoButton;					// The button for undoing changes.
	HWND redoButton;					// The button for redoing changes.
} FileEditor;

// Registers classes, initializes OLE, creates the main window, and more.
char InitializeWindows(HINSTANCE);

// Registers all of our classes.
char RegisterClasses(HINSTANCE);

// Registers the class for the main window of our program.
char RegisterMainWindowClass(HINSTANCE);

// Registers the class for the new file options dialog so it can be created later.
char RegisterNewFileOptionsClass(HINSTANCE);

// Registers the class for the window that pops up when the program starts that asks you to choose between new file and open file.
char RegisterSelectFileOptionClass(HINSTANCE);

// Undoes anything InitializeWindows did that must be undone.
void UninitializeWindows(HINSTANCE);

// Handler for any messages sent to our main window.
LRESULT CALLBACK MainWindowProcedure(HWND, UINT, WPARAM, LPARAM);


// Paints menus, controls, anything that we want to immediately draw when the program starts.
void PaintMainWindow(HWND);

// Paints the menus.
void AddMainWindowMenus(HWND);

// Processes the WM_STARTFILE message for the main window.
LRESULT ProcessStartFile();

// Displays the dialog for selecting a file option.
void PopSelectFileOptionDialog(HWND);

// Checks if the scrolled window is a trackbar that has a textbox associated with it, and syncs them.
LRESULT ProcessHScroll(HWND);

// Checks if the mouse was over the fourier graph as the double click took place, and selects everything if it was.
LRESULT ProcessLMBDoubleClick(LPARAM);

// Checks if the mouse was over the fourier graph as the click took place, and starts selecting if it was.
LRESULT ProcessLMBDown(LPARAM);

// Ends the selection if one was ongoing.
LRESULT ProcessLMBUp(LPARAM lparam);

// Erases the current selection if the mouse was hovering over the graph.
LRESULT ProcessRMBUp(LPARAM lparam);

// If a selection is ongoing, updates it.
LRESULT ProcessMouseMove(LPARAM);

// Carries out WM_NOTIFY messages of any sort.
LRESULT ProcessNotification(WPARAM, LPNMHDR);

// Checks if the control in question is the waveform graph, and updates its background and foreground colors so it's colored properly.
LRESULT ProcessControlColorStatic(HDC, HWND);

// Opens files that are dragged and dropped onto our window.
LRESULT ProcessDropFiles(HDROP);

// Gives the user a chance to save his progress before it is lost if he had any, and then closes the window.
LRESULT PromptSaveAndClose();

// Carries out WM_COMMAND messages of any sort.
LRESULT ProcessMainWindowCommand(HWND, WPARAM, LPARAM);

// Sets the file editor to the currently selected channel in the tab control.
void UpdateEditorToCurrentChannel();

// Initiates the procedure of creating a new file.
void FileNew(HWND);

// Prompts the user to choose a file to open, and reads its data into memory.
void PromptFileOpen(HWND);

// Opens and reads the file with the given path into memory.
void FileOpen(LPCTSTR, HWND);

// Saves the current file, or saves-as if the file is new and not saved anywhere yet.
void FileSave(HWND);

// Prompts the user to select a save location for the edited file, and saves it there.
void FileSaveAs(HWND);

// Undoes the last change.
void Undo(HWND);

// Redoes the last undone change.
void Redo(HWND);

// Applies the modification from the contents of the input controls.
void ApplyModificationFromInput(HWND);

// Prompts the user to choose if he wants to save his progress before it's lost. Returns zero iff the user chose to abort the operation that was about to cause progress to be lost.
char PromptSaveProgress();

// Using the currently open file, sets it up for editing. Returns zero iff it encountered an error.
void InitializeFileEditor();

// Paints all the GUI for editing the open file onto the main window.
void PaintFileEditor();

// Paints the parts of the file editor which only need to be painted once, when it is first created.
void PaintFileEditorPermanents();

// Paints the parts of the file editor that need to be repainted every time a new file is opened.
void PaintFileEditorTemporaries();

// Sets the given channel to the given domain.
void SetChannelDomain(unsigned short, FunctionDomain);

// Sets all channels to the given domain.
void SetAllChannelsDomain(FunctionDomain);

// Paints the bitmap containing the waveform of the given channel (but doesn't display it).
void PlotChannelWaveform(unsigned short);

// Paints the bitmap containing the fourier transform of the given channel (but doesn't display it).
void PlotChannelFourier(unsigned short);

// Paints the bitmaps containing the waveform and fourier transform of the given channel (but doesn't display them).
void PlotChannelGraphs(unsigned short);

// Displays the given channel's waveform graph. The graph has to be painted before calling this.
void DisplayChannelWaveform(unsigned short);

// Displays the given channel's fourier transform graph. The graph has to be painted before calling this.
void DisplayChannelFourier(unsigned short);

// Displays the given channel's waveform fourier transform graphs. The graphs have to be painted before calling this.
void DisplayChannelGraphs(unsigned short);

// Plots and displays both the waveform and fourier transform graphs of a given channel.
void PlotAndDisplayChannelGraphs(unsigned short);

// Returns the amount of samples to skip in each step when plotting graphs.
unsigned long long GetPlottingStepSize();

// Paints the selection, sets the textboxes to contain it, and sets the new values in fileEditor.
void UpdateSelection();

// Deallocates memory allocated for file editing and erases the editor from the window.
void CloseFileEditor();

// Frees memory that was reserved for storing the PCM/fourier functions of all the channels.
void DeallocateChannelsData();

// Frees memory held by bitmaps for the various channels' waveform and fourier graphs.
void DeallocateChannelsGraphs();

// Sets the title of the main window to display the current open file's name.
void UpdateWindowTitle();

// Grays and ungrays the undo and redo buttons according to the current program state.
void UpdateUndoRedoState();

// Returns nonzero iff the file editor is open, meaning a file has been created or opened at least once.
char IsEditorOpen();

// Returns zero iff there are no unsaved changes.
char HasUnsavedChanges();


// Handler for any messages sent to the new file options dialog.
LRESULT CALLBACK NewFileOptionsProcedure(HWND, UINT, WPARAM, LPARAM);

// Paints the new file options window with all the controls or whatever that make it up.
void PaintNewFileOptionsWindow(HWND);

// Paints controls that go into the new file options window.
void AddNewFileOptionsControls(HWND);

// Closes the new file options window and re-enables its parent.
void CloseNewFileOptions(HWND);

// Reads the user's selection for new file options and sets the editor to show this file.
void ApplyNewFileOptions(HWND);

// Processes any WM_COMMAND message the new file window may receive.
void ProcessNewFileOptionsCommand(HWND, WPARAM, LPARAM);


// Handler for any messages sent to the new file options dialog.
LRESULT CALLBACK SelectFileOptionProcedure(HWND, UINT, WPARAM, LPARAM);

// Paints the select file option window with all the controls or whatever that make it up.
void PaintSelectFileOptionWindow(HWND);

// Closes the select file option window and re-enables its parent.
void CloseSelectFileOption(HWND);

// Processes any WM_COMMAND message sent to the select file option window.
void ProcessSelectFileOptionCommand(HWND, WPARAM, LPARAM);


// Paints a trackbar-textbox-units triple with the given parameters.
void AddTrackbarWithTextbox(HWND, HWND*, HWND*, int, int, int, int, int, int, int, LPCTSTR, LPCTSTR, char);

// Sets the textbox to display the number selected in the trackbar.
void SyncTextboxToTrackbar(HWND, HWND);

// Sets the trackbar position to the value written in the textbox.
void SyncTrackbarToTextbox(HWND, HWND);

// Sets the textbox to display the number selected in the trackbar, converted to the [0,1] range.
void SyncTextboxToTrackbarFloat(HWND, HWND);

// Sets the trackbar position to the value written in the textbox, converted from [0,1] to the trackbar's range.
void SyncTrackbarToTextboxFloat(HWND, HWND);

// Takes a textbox handle and returns the number inside parsed to double. Returns NaN if the number couldn't be parsed.
double GetTextboxDouble(HWND);

// Takes a textbox handle and sets its text to the given double. If the number is NaN, the textbox is emptied. The float is truncated to 3 decimal digits if the truncate argument is set.
void SetTextboxDouble(HWND, double, char);

// Procedure for textboxes that only accept float numbers.
LRESULT CALLBACK FloatTextboxWindowProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);

// Procedure for the fourier graph static.
LRESULT CALLBACK FourierGraphWindowProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);

// Returns nonzero iff the point is within the bounds of the given window.
char IsInWindow(POINT, HWND);

// Takes the whole command line as a single string and returns argv, and also sets the int* it gets to argc.
LPTSTR* SplitCommandLine(LPTSTR, int*);

// Returns nonzero iff the given string ends in .wav or .wave.
char HasWaveExtension(LPTSTR);

#endif