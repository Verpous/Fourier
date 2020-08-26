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

#include "WindowManager.h"
#include "SoundEditor.h"
#include <stdio.h> // For printing errors and such.
#include <commctrl.h> // For some trackbar-related things.
#include <tchar.h> // For dealing with unicode and ANSI strings.
#include <shlwapi.h> // For PathStipPath.

// Turns any text you give it into a string.
#define Stringify(x) #x

// Like stringify, but it will stringify macro values instead of names if you give it a macro.
#define XStringify(x) Stringify(x)

// Takes a notification code and returns it as an HMENU that uses the high word so it words the same as system notification codes.
#define NOTIF_CODIFY(x) MAKEWPARAM(0, x)

// The following are notification codes. Codes below 0x8000 are reserved.
#define FILE_MENU_NEW 0x8001
#define FILE_MENU_OPEN 0x8002
#define FILE_MENU_SAVE 0x8003
#define FILE_MENU_SAVEAS 0x8004
#define FILE_MENU_EXIT 0x8005

#define EDIT_MENU_UNDO 0x8006
#define EDIT_MENU_REDO 0x8007

// The following are control identifiers. They don't have to be above 0x8000.
#define FILE_EDITOR_UNDO 1
#define FILE_EDITOR_REDO 2
#define FILE_EDITOR_APPLY 3

#define NEW_FILE_OPTIONS_OK 1
#define NEW_FILE_OPTIONS_CANCEL 2

#define MSG_BOX_OK 1
#define MSG_BOX_CANCEL 2
#define MSG_BOX_YES 6
#define MSG_BOX_NO 7

// File length is measured in seconds.
#define FILE_MIN_LENGTH 1
#define FILE_MAX_LENGTH 3600
#define NEW_FILE_DEFAULT_LENGTH 60
#define LENGTH_TRACKBAR_LINESIZE 1
#define LENGTH_TRACKBAR_PAGESIZE 60

// Frequency is measured in Hertz.
#define FILE_MIN_FREQUENCY 8000
#define FILE_MAX_FREQUENCY 96000
#define NEW_FILE_DEFAULT_FREQUENCY 44100
#define FREQUENCY_TRACKBAR_LINESIZE 50
#define FREQUENCY_TRACKBAR_PAGESIZE 1000

// Smoothing region is also in hertz.
#define MIN_SMOOTHING_REGION 1
#define MAX_SMOOTHING_REGION 1000 // TODO: make unlimited? or dependent on frequency of file? or keep as is?
#define DEFAULT_SMOOTHING_REGION 250
#define SMOOTHING_REGION_TRACKBAR_LINESIZE 5
#define SMOOTHING_REGION_TRACKBAR_PAGESIZE 50

#define NEW_FILE_TRACKBAR_TICKS 11 // How many ticks we want to have in a trackbar.

#define WC_MAINWINDOW TEXT("MainWindow")
#define WC_NEWFILEOPTIONS TEXT("NewFileOptions")
#define WC_SELECTFILEOPTION TEXT("SelectFileOption")

#define FILE_FILTER TEXT("Wave files (*.wav;*.wave)\0*.wav;*.wave\0")
#define TITLE_POSTFIX TEXT(" - Fourier")

static HWND mainWindowHandle = NULL;
static NewFileOptionsWindow* newFileOptionsHandles = NULL;
static FileEditor fileEditor = {0};

#pragma region Initialization

char InitializeWindows(HINSTANCE instanceHandle)
{
    if (!RegisterClasses(instanceHandle))
    {
        return FALSE;
    }

    // Loading common controls that this program uses.
    INITCOMMONCONTROLSEX commonCtrls;
    commonCtrls.dwSize = sizeof(INITCOMMONCONTROLSEX);
    commonCtrls.dwICC = ICC_BAR_CLASSES | ICC_STANDARD_CLASSES | ICC_TAB_CLASSES;
    InitCommonControlsEx(&commonCtrls);
    
    // Creates main window.
    // TODO: not sure about this CLIPSIBLINGS thing, MS says it's needed for tab controls.
    mainWindowHandle = CreateWindow(WC_MAINWINDOW, TEXT("Untitled") TITLE_POSTFIX, WS_OVERLAPPED | WS_MINIMIZEBOX | WS_SYSMENU | WS_VISIBLE | WS_CLIPSIBLINGS, 600, 250, 850, 700, NULL, NULL, NULL, NULL);
    return TRUE;
}

char RegisterClasses(HINSTANCE instanceHandle)
{
    return RegisterMainWindowClass(instanceHandle) && RegisterNewFileOptionsClass(instanceHandle) && RegisterSelectFileOptionClass(instanceHandle);
}

char RegisterMainWindowClass(HINSTANCE instanceHandle)
{
    WNDCLASS mainWindowClass = {0};
    mainWindowClass.hbrBackground = (HBRUSH)COLOR_WINDOW;
    mainWindowClass.hCursor = LoadCursor(instanceHandle, IDC_ARROW);
    mainWindowClass.hInstance = instanceHandle;
    mainWindowClass.lpszClassName = WC_MAINWINDOW;
    mainWindowClass.lpfnWndProc = MainWindowProcedure;

    // Registering this class. If it fails, we'll log it and end the program.
    if (!RegisterClass(&mainWindowClass))
    {
        fprintf(stderr, "RegisterClass of main window failed with error code: 0x%lX", GetLastError());
        return FALSE;
    }

    return TRUE;
}

char RegisterNewFileOptionsClass(HINSTANCE instanceHandle)
{
    WNDCLASS dialog = {0};

    dialog.hbrBackground = (HBRUSH)COLOR_WINDOW;
    dialog.hCursor = LoadCursor(instanceHandle, IDC_CROSS);
    dialog.hInstance = instanceHandle;
    dialog.lpszClassName = WC_NEWFILEOPTIONS;
    dialog.lpfnWndProc = NewFileOptionsProcedure;

    // Registering this class. If it fails, we'll log it and end the program.
    if (!RegisterClass(&dialog))
    {
        fprintf(stderr, "RegisterClass of new file options failed with error code: 0x%lX", GetLastError());
        return FALSE;
    }

    return TRUE;
}

char RegisterSelectFileOptionClass(HINSTANCE instanceHandle)
{
    WNDCLASS dialog = {0};

    dialog.hbrBackground = (HBRUSH)COLOR_WINDOW;
    dialog.hCursor = LoadCursor(instanceHandle, IDC_CROSS);
    dialog.hInstance = instanceHandle;
    dialog.lpszClassName = WC_SELECTFILEOPTION;
    dialog.lpfnWndProc = SelectFileOptionProcedure;

    // Registering this class. If it fails, we'll log it and end the program.
    if (!RegisterClass(&dialog))
    {
        fprintf(stderr, "RegisterClass of select file option failed with error code: 0x%lX", GetLastError());
        return FALSE;
    }

    return TRUE;
}

#pragma endregion // Initialization.

#pragma region MainWindow

LRESULT CALLBACK MainWindowProcedure(HWND windowHandle, UINT msg, WPARAM wparam, LPARAM lparam) 
{
    switch (msg)
    {
        case WM_CREATE:
            PaintMainWindow(windowHandle);
            PopSelectFileOptionDialog(windowHandle);
            return 0;
        case WM_COMMAND:
            ProcessMainWindowCommand(windowHandle, wparam, lparam);
            return 0;
        case WM_HSCROLL:
            // Updating the textbox to match the trackbar.
            if (fileEditor.smoothingRangeTrackbar == (HWND)lparam)
            {
                SyncTextboxToTrackbar(fileEditor.smoothingRangeTrackbar, fileEditor.smoothingRangeTextbox);
            }

            return 0;
        case WM_CLOSE:
            // Prompt the user to save his progress before it is lost.
            if (PromptSaveProgress(windowHandle))
            {
                // Only proceeding if the user didn't choose to abort.
                CloseWaveFile(&(fileEditor.fileInfo));
                DestroyWindow(windowHandle);
            }

            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProc(windowHandle, msg, wparam, lparam);
    }
}

void PaintMainWindow(HWND windowHandle)
{
    AddMainWindowMenus(windowHandle);
}

void AddMainWindowMenus(HWND windowHandle)
{
    HMENU menuHandler = CreateMenu();
    HMENU fileMenuHandler = CreateMenu();
    HMENU editMenuHandler = CreateMenu();

    // Appending file menu options.
    AppendMenu(fileMenuHandler, MF_STRING, NOTIF_CODIFY(FILE_MENU_NEW), TEXT("New\tCtrl+N"));
    AppendMenu(fileMenuHandler, MF_STRING, NOTIF_CODIFY(FILE_MENU_OPEN), TEXT("Open\tCtrl+O"));
    AppendMenu(fileMenuHandler, MF_STRING, NOTIF_CODIFY(FILE_MENU_SAVE), TEXT("Save\tCtrl+S"));
    AppendMenu(fileMenuHandler, MF_STRING, NOTIF_CODIFY(FILE_MENU_SAVEAS), TEXT("Save as\tCtrl+Shift+S"));
    AppendMenu(fileMenuHandler, MF_SEPARATOR, 0, NULL); // Separator between exit and all the other options.
    AppendMenu(fileMenuHandler, MF_STRING, NOTIF_CODIFY(FILE_MENU_EXIT), TEXT("Exit"));

    // Appending edit menu options.
    AppendMenu(editMenuHandler, MF_STRING, NOTIF_CODIFY(EDIT_MENU_UNDO), TEXT("Undo\tCtrl+Z"));
    AppendMenu(editMenuHandler, MF_STRING, NOTIF_CODIFY(EDIT_MENU_REDO), TEXT("Redo\tCtrl+Y"));

    // Adding both menus.
    AppendMenu(menuHandler, MF_POPUP, (UINT_PTR)fileMenuHandler, TEXT("File"));
    AppendMenu(menuHandler, MF_POPUP, (UINT_PTR)editMenuHandler, TEXT("Edit"));

    SetMenu(windowHandle, menuHandler);
}

void PaintCurrentFileEditor(HWND windowHandle)
{
    // TODO: this is a temporary fix for cleaning the window before re-painting it. In the future I think I could do something more optimized, like only erase the parts that I have to (maybe only the channel names and graphs even).
    static HWND tabCtrl = NULL;

    if (tabCtrl != NULL)
    {
        DestroyWindow(tabCtrl);
    }
    
    // Updating the window title to the open file's name.
    if (IsFileNew(fileEditor.fileInfo))
    {
        SetWindowText(windowHandle, TEXT("Untitled") TITLE_POSTFIX);
    }
    else
    {
        // Extracting the file name from the full path, and appending " - Fourier".
        // I decided not to impose a length limit because I fear cutting a unicode string in the middle might ruin it. Worst comes to worst, users get a long ass string at the top of the screen.
        unsigned int len = _tcslen(fileEditor.fileInfo->path);
        TCHAR pathCopy[len + _tcslen(TITLE_POSTFIX) + 1]; // Allocating enough for the path name, the postfix, and the null terminator.
        _tcscpy(pathCopy, fileEditor.fileInfo->path);
        PathStripPath(pathCopy);
        _tcscat(pathCopy, TITLE_POSTFIX);
        SetWindowText(windowHandle, pathCopy);
    }

    tabCtrl = CreateWindow(WC_TABCONTROL, TEXT(""), WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | TCS_TABS, 5, 0, 835, 647, windowHandle, NULL, NULL, NULL);

    TCITEM tab;
    tab.mask = TCIF_PARAM | TCIF_TEXT;

    // This fills in the buffer with channel names, and returns how many channels were filled in.
    TCHAR channelNames[MAX_NAMED_CHANNELS][24];
    unsigned int numOfChannels = GetChannelNames(fileEditor.fileInfo, channelNames);

    for (unsigned int i = 0; i < numOfChannels; i++)
    {
        tab.pszText = channelNames[i];
        TabCtrl_InsertItem(tabCtrl, i, &tab);
    }

    // Originally, all the controls below this were children of this one. But apparently that makes this control receive notifications from its children instead of the main window receiving them, so I changed that.
    HWND tabCtrlStatic = CreateWindow(WC_STATIC, TEXT(""), WS_CHILD | WS_VISIBLE | WS_BORDER | SS_WHITEFRAME, 10, 28, 825, 613, windowHandle, NULL, NULL, NULL);

    // Adding GUI for choosing what frequency to modify.
    CreateWindow(WC_STATIC, TEXT("Frequency:"), WS_VISIBLE | WS_CHILD, 50, 430, 80, 22, windowHandle, NULL, NULL, NULL);

    fileEditor.frequencyTextbox = CreateWindow(WC_EDIT, TEXT("0"), WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER | ES_CENTER, 130, 428, 65, 22, windowHandle, NULL, NULL, NULL);
    SendMessage(fileEditor.frequencyTextbox, EM_SETLIMITTEXT, (WPARAM)6, 0); // Setting character limit.
    CreateWindow(WC_STATIC, TEXT("Hz"), WS_VISIBLE | WS_CHILD, 200, 430, 60, 22, windowHandle, NULL, NULL, NULL);

    // Adding GUI for choosing what change to apply.
    CreateWindow(WC_STATIC, TEXT("Change:"), WS_VISIBLE | WS_CHILD, 50, 475, 80, 22, windowHandle, NULL, NULL, NULL);

    // Adding a radio menu for choosing between multiply and add modes.
    fileEditor.multiplyRadio = CreateWindow(WC_BUTTON, TEXT("Multiply"), WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON | BS_VCENTER | WS_GROUP, 110, 470, 80, 30, windowHandle, NULL, NULL, NULL);
    fileEditor.addRadio = CreateWindow(WC_BUTTON, TEXT("Add"), WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON | BS_VCENTER, 190, 470, 50, 30, windowHandle, NULL, NULL, NULL);
    SendMessage(fileEditor.multiplyRadio, BM_SETCHECK, BST_CHECKED, 0); // Setting default selection for this menu.

    fileEditor.changeTextbox = CreateWindow(WC_EDIT, TEXT("1"), WS_VISIBLE | WS_CHILD | WS_BORDER | ES_CENTER, 250, 473, 65, 22, windowHandle, NULL, NULL, NULL);
    SetWindowSubclass(fileEditor.changeTextbox, FloatTextboxWindowProc, 0, 0); // Setting textbox to only accept float numbers.
    SendMessage(fileEditor.changeTextbox, EM_SETLIMITTEXT, (WPARAM)6, 0); // Setting character limit.

    // Adding GUI for choosing how much smoothing to apply.
    CreateWindow(WC_STATIC, TEXT("Smoothing region:"), WS_VISIBLE | WS_CHILD, 50, 520, 130, 22, windowHandle, NULL, NULL, NULL);
    AddTrackbarWithTextbox(windowHandle, &(fileEditor.smoothingRangeTrackbar), &(fileEditor.smoothingRangeTextbox), 170, 520,
        MIN_SMOOTHING_REGION, MAX_SMOOTHING_REGION, DEFAULT_SMOOTHING_REGION, SMOOTHING_REGION_TRACKBAR_LINESIZE, SMOOTHING_REGION_TRACKBAR_PAGESIZE, TEXT(XStringify(DEFAULT_SMOOTHING_REGION)), TEXT("Hz"));
    
    // The trackbar for some reason was rendered behind other stuff. This makes it render above.
    BringWindowToTop(fileEditor.smoothingRangeTrackbar);

    // Adding buttons for ok and cancel.
    // TODO: I'm considering moving these buttons to the center of the screen (horizontally).
    CreateWindow(WC_BUTTON, TEXT("Undo"), WS_VISIBLE | WS_CHILD | BS_CENTER | BS_VCENTER, 595, 598, 70, 35, windowHandle, (HMENU)FILE_EDITOR_UNDO, NULL, NULL);
    CreateWindow(WC_BUTTON, TEXT("Redo"), WS_VISIBLE | WS_CHILD | BS_CENTER | BS_VCENTER, 675, 598, 70, 35, windowHandle, (HMENU)FILE_EDITOR_REDO, NULL, NULL);
    CreateWindow(WC_BUTTON, TEXT("Apply"), WS_VISIBLE | WS_CHILD | BS_CENTER | BS_VCENTER, 755, 598, 70, 35, windowHandle, (HMENU)FILE_EDITOR_APPLY, NULL, NULL);
}

void PopSelectFileOptionDialog(HWND windowHandle)
{
    CreateWindow(WC_SELECTFILEOPTION, TEXT("Select File Option - Fourier"), WS_OVERLAPPED | WS_VISIBLE | WS_SYSMENU, 700, 450, 330, 120, windowHandle, NULL, NULL, NULL);
    EnableWindow(windowHandle, FALSE);
}

void ProcessMainWindowCommand(HWND windowHandle, WPARAM wparam, LPARAM lparam)
{
    switch (HIWORD(wparam))
    {
        case BN_CLICKED:
            switch (LOWORD(wparam))
            {
                case FILE_EDITOR_REDO:
                    fprintf(stderr, "REDO!\n");
                    break;
                case FILE_EDITOR_UNDO:
                    break;
                case FILE_EDITOR_APPLY:
                    break;
            }

            break;
        case EN_UPDATE: // EN_UPDATE is sent before the screen gets updated, EN_CHANGE gets sent after.
            ;
            HWND focusedWindow = GetFocus();
            HWND controlHandle = (HWND)lparam;

            // Only matching the trackbar to the textbox when it's in fact the textbox that changed,
            // and when the textbox is in focus to avoid the fact that when I programmatically update the textbox due to trackbar movement, it then fires this event.
            if (focusedWindow == fileEditor.smoothingRangeTextbox && controlHandle == fileEditor.smoothingRangeTextbox)
            {
                SyncTrackbarToTextbox(fileEditor.smoothingRangeTrackbar, fileEditor.smoothingRangeTextbox);
            }

            break;
        case FILE_MENU_NEW:
            FileNew(windowHandle);
            break;
        case FILE_MENU_OPEN:
            FileOpen(windowHandle);
            break;
        case FILE_MENU_SAVE:
            FileSave(windowHandle);
            break;
        case FILE_MENU_SAVEAS:
            FileSaveAs(windowHandle);
            break;
        case FILE_MENU_EXIT:
            DestroyWindow(windowHandle);
            break;
        case EDIT_MENU_REDO:
            break;
        case EDIT_MENU_UNDO:
            break;
        default:
            break;
    }
}

void FileNew(HWND windowHandle)
{
    if (newFileOptionsHandles != NULL)
    {
        fprintf(stderr, "Tried to create new file while already in that menu\n");
        return;
    }

    newFileOptionsHandles = calloc(1, sizeof(NewFileOptionsWindow));
    newFileOptionsHandles->handle = CreateWindow(WC_NEWFILEOPTIONS, TEXT("New File Options - Fourier"),
        WS_VISIBLE | WS_OVERLAPPED | WS_SYSMENU, 800, 500, 362, 212, windowHandle, NULL, NULL, NULL);
    newFileOptionsHandles->parent = windowHandle;
    EnableWindow(windowHandle, FALSE); // Disabling the parent because this is a modal window.
}

void FileOpen(HWND windowHandle)
{
    // Giving the user a chance to save his current progress first (if there isn't any the function will take care of that).
    if (!PromptSaveProgress(windowHandle))
    {
        return; // Aborting the file open operation if the user chose it in the prompt.
    }

    // Now proceeding with opening a new file.
    OPENFILENAME ofn = {0};
    TCHAR filename[MAX_PATH];
    filename[0] = TEXT('\0'); // Without this line this doesn't work.

    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = windowHandle;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = FILE_FILTER;
    ofn.Flags = OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn))
    {
        ReadWaveResult result = ReadWaveFile(&(fileEditor.fileInfo), filename);

        if (ResultHasError(result))
        {
            LPTSTR messageText = NULL;

            switch (ResultErrorCode(result))
            {
                case FILE_CANT_OPEN:
                    messageText = TEXT("The file could not be opened. It may be open in another program.");
                    break;
                case FILE_NOT_WAVE:
                    messageText = TEXT("The file is not a WAVE file.");
                    break;
                case FILE_BAD_WAVE:
                    messageText = TEXT("The file is not entirely compliant with the WAVE format specifications.");
                    break;
                case FILE_BAD_FORMAT:
                    messageText = TEXT("The file uses an unsupported audio format.");
                    break;
                case FILE_BAD_BITDEPTH:
                    messageText = TEXT("The file uses an unsupported bit depth.");
                    break;
                case FILE_BAD_FREQUENCY:
                    messageText = TEXT("The file uses an unsupported sample rate.");
                    break;
                case FILE_BAD_SIZE:
                    messageText = TEXT("The file's actual size does not match up with what it should be.");
                    break;
                case FILE_BAD_SAMPLES:
                    messageText = TEXT("The file exceeds the maximum number of samples allowed by this program.");
                    break;
                case FILE_MISC_ERROR:
                default:
                    messageText = TEXT("A miscellaneous error occured.");
                    break;
            }

            MessageBox(windowHandle, messageText, NULL, MB_OK | MB_ICONERROR);
        }
        else // No error.
        {
            if (ResultHasWarning(result))
            {
                if (ResultWarningCode(result) & FILE_CHUNK_WARNING)
                {
                    int choice = MessageBox(windowHandle,
                        TEXT("The file contains some information which is ignored by this program, which may lead to unexpected results."),
                        TEXT("Warning"), MB_OKCANCEL | MB_ICONWARNING);
                    
                    if (choice == MSG_BOX_CANCEL)
                    {
                        CloseWaveFile(&(fileEditor.fileInfo));
                        return;
                    }
                }

                if (ResultWarningCode(result) & FILE_CHAN_WARNING)
                {
                    int choice = MessageBox(windowHandle,
                        TEXT("The file contains more channels than this program supports. You will only be able to edit some of the channels."),
                        TEXT("Warning"), MB_OKCANCEL | MB_ICONWARNING);
                    
                    if (choice == MSG_BOX_CANCEL)
                    {
                        CloseWaveFile(&(fileEditor.fileInfo));
                        return;
                    }
                }
            }
            
            PaintCurrentFileEditor(mainWindowHandle);
        }
    }
    else // GetOpenFileName failed.
    {
        DWORD error = CommDlgExtendedError();
        fprintf(stderr, "GetOpenFileName failed with error code %lX\n", error);
        
        if (error == FNERR_BUFFERTOOSMALL)
        {
            MessageBox(windowHandle, TEXT("Path name exceeds the upper limit of ") TEXT(XStringify(MAX_PATH)) TEXT(" characters"), NULL, MB_OK | MB_ICONERROR);
        }
    }
}

void FileSave(HWND windowHandle)
{
    if (IsFileNew(fileEditor.fileInfo))
    {
        FileSaveAs(windowHandle);
    }
    else
    {
        WriteWaveFile(fileEditor.fileInfo);
    }
}

void FileSaveAs(HWND windowHandle)
{
    OPENFILENAME ofn = {0};
    TCHAR filename[MAX_PATH];
    filename[0] = TEXT('\0');  // Without this line this doesn't work.

    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = windowHandle;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = FILE_FILTER;

    if (GetSaveFileName(&ofn))
    {
        WriteWaveFileAs(fileEditor.fileInfo, filename);
    }
    else
    {
        DWORD error = CommDlgExtendedError();
        fprintf(stderr, "GetSaveFileName failed with error code %lX\n", error);
        
        if (error == FNERR_BUFFERTOOSMALL)
        {
            MessageBox(windowHandle, TEXT("Path name exceeds the upper limit of ") TEXT(XStringify(MAX_PATH)) TEXT(" characters."), NULL, MB_OK | MB_ICONERROR);
        }
    }
}

char PromptSaveProgress(HWND windowHandle)
{
    if (HasUnsavedProgress())
    {
        int choice = MessageBox(windowHandle,
            TEXT("There is unsaved progress that will be lost if you proceed without saving it. Would you like to save?"),
            TEXT("Warning"), MB_ICONWARNING | MB_YESNOCANCEL);

        switch (choice)
        {
            case MSG_BOX_CANCEL:
                return FALSE; // Returning that the program short abort the operation that prompted this.
            case MSG_BOX_NO:
                break; // Proceeding without saving in case of no.
            case MSG_BOX_YES:
                FileSave(windowHandle); // Saving first in case of yes.
                break;
            default:
                break;
        }
    }

    // Returning that the program should continue the operation that prompted this.
    return TRUE;
}

#pragma endregion // MainWindow.

#pragma region NewFileOptions

LRESULT CALLBACK NewFileOptionsProcedure(HWND windowHandle, UINT msg, WPARAM wparam, LPARAM lparam) 
{
    switch (msg)
    {
        case WM_CREATE:
            PaintNewFileOptionsWindow(windowHandle);
            return 0;
        case WM_COMMAND:
            ProcessNewFileOptionsCommand(windowHandle, wparam, lparam);
            return 0;
        case WM_HSCROLL:
            ;
            // In case of trackbar value changing, update the textbox to match it.
            HWND controlHandle = (HWND)lparam;
            if (newFileOptionsHandles->lengthTrackbar == controlHandle)
            {
                SyncTextboxToTrackbar(newFileOptionsHandles->lengthTrackbar, newFileOptionsHandles->lengthTextbox);
            }
            else if (newFileOptionsHandles->frequencyTrackbar == controlHandle)
            {
                SyncTextboxToTrackbar(newFileOptionsHandles->frequencyTrackbar, newFileOptionsHandles->frequencyTextbox);
            }

            return 0;
        case WM_CLOSE:
            CloseNewFileOptions(windowHandle);
            return 0;
        default:
            return DefWindowProc(windowHandle, msg, wparam, lparam);
    }
}

void PaintNewFileOptionsWindow(HWND windowHandle)
{
   // Adding trackbar-textbox-units triple for selecting file length.
    AddTrackbarWithTextbox(windowHandle, &(newFileOptionsHandles->lengthTrackbar), &(newFileOptionsHandles->lengthTextbox), 10, 16,
        FILE_MIN_LENGTH, FILE_MAX_LENGTH, NEW_FILE_DEFAULT_LENGTH, LENGTH_TRACKBAR_LINESIZE, LENGTH_TRACKBAR_PAGESIZE, TEXT(XStringify(NEW_FILE_DEFAULT_LENGTH)), TEXT("seconds"));

    // Adding trackbar-textbox-units triple for selecting file sample rate.
    AddTrackbarWithTextbox(windowHandle, &(newFileOptionsHandles->frequencyTrackbar), &(newFileOptionsHandles->frequencyTextbox), 10, 61,
        FILE_MIN_FREQUENCY, FILE_MAX_FREQUENCY, NEW_FILE_DEFAULT_FREQUENCY, FREQUENCY_TRACKBAR_LINESIZE, FREQUENCY_TRACKBAR_PAGESIZE, TEXT(XStringify(NEW_FILE_DEFAULT_FREQUENCY)), TEXT("Hz"));

    // Adding a radio menu for choosing bit depth. Important that we add these such that the i'th cell indicates i+1 bytes.
    newFileOptionsHandles->depthOptions[0] = CreateWindow(WC_BUTTON, TEXT("8-bit"), WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON | BS_VCENTER | WS_GROUP, 18, 95, 80, 30, windowHandle, NULL, NULL, NULL);
    newFileOptionsHandles->depthOptions[1] = CreateWindow(WC_BUTTON, TEXT("16-bit"), WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON | BS_VCENTER, 98, 95, 80, 30, windowHandle, NULL, NULL, NULL);
    newFileOptionsHandles->depthOptions[2] = CreateWindow(WC_BUTTON, TEXT("24-bit"), WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON | BS_VCENTER, 178, 95, 80, 30, windowHandle, NULL, NULL, NULL);
    newFileOptionsHandles->depthOptions[3] = CreateWindow(WC_BUTTON, TEXT("32-bit"), WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON | BS_VCENTER, 258, 95, 80, 30, windowHandle, NULL, NULL, NULL);
    SendMessage(newFileOptionsHandles->depthOptions[1], BM_SETCHECK, BST_CHECKED, 0); // Setting default selection for this menu.

    // Adding buttons for ok and cancel.
    CreateWindow(WC_BUTTON, TEXT("Cancel"), WS_VISIBLE | WS_CHILD | BS_CENTER | BS_VCENTER, 198, 140, 70, 35, windowHandle, (HMENU)NEW_FILE_OPTIONS_CANCEL, NULL, NULL);
    CreateWindow(WC_BUTTON, TEXT("Ok"), WS_VISIBLE | WS_CHILD | BS_CENTER | BS_VCENTER, 278, 140, 70, 35, windowHandle, (HMENU)NEW_FILE_OPTIONS_OK, NULL, NULL);
}

void CloseNewFileOptions(HWND windowHandle)
{
    // Enabling the parent window and destroying this one.
    EnableWindow(newFileOptionsHandles->parent, TRUE);
    free(newFileOptionsHandles);
    newFileOptionsHandles = NULL;
    DestroyWindow(windowHandle);
}

void ApplyNewFileOptions(HWND windowHandle)
{
    // Getting file length. If it's not within the legal range we pop an error message (although I think that shouldn't be possible).
    // Even though there's both a trackbar and a textbox, we read from the trackbar because they're supposed to be in sync anyway and the trackbar gives us an int directly + enforces the range limits.
    unsigned int length = (unsigned int)SendMessage(newFileOptionsHandles->lengthTrackbar, TBM_GETPOS, 0, 0);

    if (!(FILE_MIN_LENGTH <= length && length <= FILE_MAX_LENGTH))
    {
        MessageBox(windowHandle, TEXT("Invalid file length."), NULL, MB_ICONERROR | MB_OK);
        return;
    }

    // Getting the sample rate same as we did with file length.
    unsigned int frequency = (unsigned int)SendMessage(newFileOptionsHandles->frequencyTrackbar, TBM_GETPOS, 0, 0);
    
    if (!(FILE_MIN_FREQUENCY <= frequency && frequency <= FILE_MAX_FREQUENCY))
    {
        MessageBox(windowHandle, TEXT("Invalid sample rate."), NULL, MB_ICONERROR | MB_OK);
        return;
    }

    // Getting the bit depth (as byte depth because it works better with the algorithm). The i'th cell in the array indicates i+1 byte depth. We assume the first cell is selected and only check the rest.
    unsigned int byteDepth = 1;

    for (unsigned int i = 1; i < 4; i++)
    {
        if (SendMessage(newFileOptionsHandles->depthOptions[i], BM_GETCHECK, 0, 0) == BST_CHECKED)
        {
            byteDepth = i + 1;
            break; // Only one radio button can be checked so we can exit now.
        }
    }

    // Giving the user a chance to save if there is unsaved progress.
    if (PromptSaveProgress(windowHandle))
    {
        // Storing this because the pointer to it will be deallocated by CloseNewFileOptions.
        HWND parent = newFileOptionsHandles->parent;

        // Proceeding with creating a new file only if the user didn't choose to abort.
        CreateNewFile(&(fileEditor.fileInfo), length, frequency, byteDepth);
        PaintCurrentFileEditor(mainWindowHandle);
        CloseNewFileOptions(windowHandle);

        // This is a bandage fix but I can't come up with a better way to have the select file option menu close when you create a new file.
        if (parent != mainWindowHandle)
        {
            CloseSelectFileOption(parent);
        }
    }
}

void ProcessNewFileOptionsCommand(HWND windowHandle, WPARAM wparam, LPARAM lparam)
{
    switch (HIWORD(wparam))
    {
        case BN_CLICKED:
            switch (LOWORD(wparam))
            {
                case NEW_FILE_OPTIONS_CANCEL:
                    CloseNewFileOptions(windowHandle);
                    break;
                case NEW_FILE_OPTIONS_OK:
                    ApplyNewFileOptions(windowHandle);
                    break;
                default:
                    break;    
            }
            
            break;
        case EN_UPDATE: // EN_UPDATE is sent before the screen gets updated, EN_CHANGE gets sent after.
            ;
            HWND focusedWindow = GetFocus();
            HWND controlHandle = (HWND)lparam;

            // Only matching the trackbar to the textbox when it's in fact the textbox that changed,
            // and when the textbox is in focus to avoid the fact that when I programmatically update the textbox due to trackbar movement, it then fires this event.
            if (focusedWindow == newFileOptionsHandles->lengthTextbox && controlHandle == newFileOptionsHandles->lengthTextbox)
            {
                SyncTrackbarToTextbox(newFileOptionsHandles->lengthTrackbar, newFileOptionsHandles->lengthTextbox);
            }
            else if (focusedWindow == newFileOptionsHandles->frequencyTextbox && controlHandle == newFileOptionsHandles->frequencyTextbox)
            {
                SyncTrackbarToTextbox(newFileOptionsHandles->frequencyTrackbar, newFileOptionsHandles->frequencyTextbox);
            }

            break;
        default:
            break;
    }   
}

#pragma endregion // NewFileOptions.

#pragma region SelectFileOption

LRESULT CALLBACK SelectFileOptionProcedure(HWND windowHandle, UINT msg, WPARAM wparam, LPARAM lparam) 
{
    switch (msg)
    {
        case WM_CREATE:
            PaintSelectFileOptionWindow(windowHandle);
            return 0;
        case WM_COMMAND:
            ProcessSelectFileOptionCommand(windowHandle, wparam, lparam);
            return 0;
        case WM_CLOSE:
            CloseSelectFileOption(windowHandle);
            return 0;
        default:
            return DefWindowProc(windowHandle, msg, wparam, lparam);
    }
}

void PaintSelectFileOptionWindow(HWND windowHandle)
{
    // Adding text with the prompt description.
    CreateWindow(WC_STATIC, TEXT("Create a new file or open an existing one?"), WS_VISIBLE | WS_CHILD | SS_CENTER, 0, 15, 330, 30, windowHandle, NULL, NULL, NULL);

    // Adding buttons for new file and open file.
    CreateWindow(WC_BUTTON, TEXT("New file"), WS_VISIBLE | WS_CHILD | BS_CENTER | BS_VCENTER, 85, 50, 70, 35, windowHandle, (HMENU)FILE_MENU_NEW, NULL, NULL);
    CreateWindow(WC_BUTTON, TEXT("Open file"), WS_VISIBLE | WS_CHILD | BS_CENTER | BS_VCENTER, 175, 50, 70, 35, windowHandle, (HMENU)FILE_MENU_OPEN, NULL, NULL);
}

void CloseSelectFileOption(HWND windowHandle)
{
    // Enabling the parent window and destroying this one.
    HWND parent = GetWindow(windowHandle, GW_OWNER);
    EnableWindow(parent, TRUE);
    SetActiveWindow(parent); // Sometimes the parent window wasn't given the focus again when you close this window.
    DestroyWindow(windowHandle);
}

void ProcessSelectFileOptionCommand(HWND windowHandle, WPARAM wparam, LPARAM lparam)
{
    switch (HIWORD(wparam))
    {
        case BN_CLICKED:
            switch (LOWORD(wparam))
            {
                case FILE_MENU_NEW:
                    FileNew(windowHandle);
                    break;
                case FILE_MENU_OPEN:
                    FileOpen(windowHandle);

                    // If the FileOpen operation was a success, closing this menu.
                    if (IsFileOpen(fileEditor.fileInfo))
                    {
                        CloseSelectFileOption(windowHandle);
                    }

                    break;
                default:
                    break;    
            }
            
            break;
        default:
            break;
    }   
}

#pragma endregion // SelectFileOption.

#pragma region Misc

void AddTrackbarWithTextbox(HWND windowHandle, HWND* trackbar, HWND* textbox, int xPos, int yPos, int minValue, int maxValue, int defaultValue, int linesize, int pagesize, LPCTSTR defaultValueStr, LPCTSTR units)
{
    // Calculating tick length given the interval length and how many ticks we want to have. Rounding it up instead of down because I would rather have too few ticks than too many.
    div_t divResult = div(maxValue - minValue, NEW_FILE_TRACKBAR_TICKS);
    WPARAM tickLength = divResult.quot + (divResult.rem ? 1 : 0);

    // Adding trackbar.
    *trackbar = CreateWindow(TRACKBAR_CLASS, NULL, WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS, xPos, yPos - 6, 200, 30, windowHandle, 0, NULL, NULL);
    SendMessage(*trackbar, TBM_SETRANGEMIN, (WPARAM)FALSE, (LPARAM)minValue); // Configuring min range of trackbar.
    SendMessage(*trackbar, TBM_SETRANGEMAX, (WPARAM)TRUE, (LPARAM)maxValue); // Configuring max range of trackbar.
    SendMessage(*trackbar, TBM_SETPOS, (WPARAM)TRUE, (LPARAM)defaultValue); // Configuring default selection.
    SendMessage(*trackbar, TBM_SETLINESIZE, 0, (LPARAM)linesize); // Configuring sensitivity for arrow key inputs.
    SendMessage(*trackbar, TBM_SETPAGESIZE, 0, (LPARAM)pagesize); // Configuring sensitivity for PGUP/PGDOWN inputs.
    SendMessage(*trackbar, TBM_SETTICFREQ, tickLength, 0); // Configuring how many ticks are on it.

    // Adding textbox.
    *textbox = CreateWindow(WC_EDIT, defaultValueStr, WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER | ES_CENTER, xPos + 210, yPos - 2, 65, 22, windowHandle, NULL, NULL, NULL);
    SendMessage(*textbox, EM_SETLIMITTEXT, (WPARAM)6, 0); // Setting character limit.
    CreateWindow(WC_STATIC, units, WS_VISIBLE | WS_CHILD, xPos + 280, yPos, 60, 22, windowHandle, NULL, NULL, NULL);
}

void SyncTextboxToTrackbar(HWND trackbar, HWND textbox)
{
    int length = (int)SendMessage(trackbar, TBM_GETPOS, 0, 0);
    TCHAR lengthStr[16];
    _itot(length, lengthStr, 10);
    SendMessage(textbox, WM_SETTEXT, 0, (LPARAM)lengthStr);
}

void SyncTrackbarToTextbox(HWND trackbar, HWND textbox)
{
    TCHAR buffer[16];
    unsigned long int length = SendMessage(textbox, WM_GETTEXT, 16, (LPARAM)buffer);
    int min = SendMessage(trackbar, TBM_GETRANGEMIN, 0, 0);
    int max = SendMessage(trackbar, TBM_GETRANGEMAX, 0, 0);
    length = length > max ? max : (length < min ? min : length); // Clamping length between min and max.
    SendMessage(trackbar, TBM_SETPOS, (WPARAM)TRUE, (LPARAM)length);
}

LRESULT CALLBACK FloatTextboxWindowProc(HWND windowHandle, UINT msg, WPARAM wparam, LPARAM lparam, UINT_PTR subclassId, DWORD_PTR refData)
{
    switch (msg)
    {
        case WM_CHAR:
            // If the character isn't a digit or a dot, rejecting it.
            if (!(('0' <= wparam && wparam <= '9') ||  wparam == '.' ||
                wparam == '\r' || wparam == '\b' || wparam == '\x03' || wparam == '\x16' || wparam == '\x18')) // ASCII codes for carriage return, backspace, ctrl+c, ctrl+v, ctrl+x.
            {
                return 0;
            }
            else if (wparam == '.') // If the digit is a dot, we want to check if there already is one.
            {
                TCHAR buffer[16];
                SendMessage(windowHandle, WM_GETTEXT, 16, (LPARAM)buffer);

                // _tcschr finds the first occurence of a character and returns NULL if it wasn't found. Rejecting this input if it found a dot.
                if (_tcschr(buffer, TEXT('.')) != NULL)
                {
                    return 0;
                }
            }

        default:
            return DefSubclassProc(windowHandle, msg, wparam, lparam);
    }
}

#pragma endregion // Misc.