#ifndef WINDOWS_MANAGER_H
#define WINDOWS_MANAGER_H

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


// Registers classes and creates the main window.
char InitializeWindows(HINSTANCE);

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

// Paints all the GUI for editing the open file onto the main window.
void PaintCurrentFileEditor(HWND);

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

// Prompts the user to choose if he wants to save his progress before it's lost. Returns zero iff the user chose to abort the operation that was about to cause progress to be lost.
char PromptSaveProgress();


// Handler for any messages sent to the new file options dialog.
LRESULT CALLBACK NewFileOptionsProcedure(HWND, UINT, WPARAM, LPARAM);

// Paints the new file options window with all the controls or whatever that make it up.
void PaintNewFileOptionsWindow(HWND);

// Paints controls that go into the new file options window.
void AddNewFileOptionsControls(HWND);

// Paints a trackbar-textbox-units triple with the given parameters.
void AddTrackbarWithTextbox(HWND, HWND*, HWND*, int, int, int, int, int, int, LPCTSTR, LPCTSTR);

// Closes the new file options window and re-enables its parent.
void CloseNewFileOptions(HWND);

// Reads the user's selection for new file options and sets the editor to show this file.
void ApplyNewFileOptions(HWND);

// Processes any WM_COMMAND message the new file window may receive.
void ProcessNewFileOptionsCommand(HWND, WPARAM, LPARAM);

// Sets the length textbox to the number selected in the trackbar.
void SyncTextboxToTrackbar(HWND, HWND);

// Sets the length trackbar to the value written in the textbox.
void SyncTrackbarToTextbox(HWND, HWND);


// Handler for any messages sent to the new file options dialog.
LRESULT CALLBACK SelectFileOptionProcedure(HWND, UINT, WPARAM, LPARAM);

// Paints the select file option window with all the controls or whatever that make it up.
void PaintSelectFileOptionWindow(HWND);

// Closes the select file option window and re-enables its parent.
void CloseSelectFileOption(HWND);

// Processes any WM_COMMAND message sent to the select file option window.
void ProcessSelectFileOptionCommand(HWND, WPARAM, LPARAM);

#endif