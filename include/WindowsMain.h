#ifndef WINDOWS_MANAGER_H
#define WINDOWS_MANAGER_H

#include "WaveReadWriter.h"
#include "SoundEditor.h"
#include <windows.h> // Do I need to explain why this is included?

typedef struct NewFileOptionsWindow
{
    HWND handle;
    HWND parent;
    HWND lengthTrackbar;
    HWND lengthTextbox;
    HWND frequencyTrackbar;
    HWND frequencyTextbox;
    HWND depthOptions[4];
} NewFileOptionsWindow;

typedef struct FileEditor
{   
    FileInfo* fileInfo;
    Function** channelsData; // An array of function pointers. This can be either the waveform or the DFT, we swap between them.
    Modification* modificationStack; // A stack of all the changes the user applies, for undoing and redoing them.
    Modification* currentSaveState; // The last change that was saved.

    HWND channelTabs;
    HWND fromFreqTextbox;
    HWND toFreqTextbox;
    HWND changeTypeDropdown;
    HWND changeAmountTextbox;
    HWND smoothingTrackbar;
    HWND smoothingTextbox;
    HWND undoButton;
    HWND redoButton;
} FileEditor;


// Registers classes and creates the main window.
char InitializeWindows(HINSTANCE);

// Registers all of our classes.
char RegisterClasses(HINSTANCE);

// Registers the class for the main window of our program.
char RegisterMainWindowClass(HINSTANCE);

// Registers the class for the new file options dialog so it can be created later.
char RegisterNewFileOptionsClass(HINSTANCE);

// Registers the class for the window that pops up when the program starts that asks you to choose between new file and open file.
char RegisterSelectFileOptionClass(HINSTANCE);


// Handler for any messages sent to our main window.
LRESULT CALLBACK MainWindowProcedure(HWND, UINT, WPARAM, LPARAM);

// Paints menus, controls, anything that we want to immediately draw when the program starts.
void PaintMainWindow(HWND);

// Paints the menus.
void AddMainWindowMenus(HWND);

// Paints controls that we want to have when the program starts.
void AddMainWindowControls(HWND);

// Displays the dialog for selecting a file option.
void PopSelectFileOptionDialog(HWND);

// Carries out WM_COMMAND messages of any sort.
void ProcessMainWindowCommand(HWND, WPARAM, LPARAM);

// Initiates the procedure of creating a new file.
void FileNew(HWND);

// Prompts the user to choose a file to open, and reads its data into memory.
void FileOpen(HWND);

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

// Resets values inside the permanent parts of the file editor for when new files are opened.
void ResetFileEditorPermanents();

// Deallocates memory allocated for file editing and erases the editor from the window.
void CloseFileEditor();

// Frees memory that was reserved for storing the PCM/fourier functions of all the channels.
void DeallocateChannelsData();

// Sets the title of the main window to display the current open file's name.
void UpdateWindowTitle();

// Grays and ungrays the undo and redo buttons according to the current program state.
void UpdateUndoRedoState();

// Returns nonzero iff the file editor is open, meaning a file has been created or opened at least once.
char IsEditorOpen();


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

// Procedure for textboxes that only accept float numbers.
LRESULT CALLBACK FloatTextboxWindowProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);

// Returns zero iff there are no unsaved changes.
char HasUnsavedChanges();

#endif