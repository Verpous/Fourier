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
#include "FileManager.h"
#include <stdio.h>
#include <stdlib.h>
#include <commctrl.h>
#include <tchar.h>

// Turns any text you give it into a string.
#define Stringify(x) #x 

// Like stringify, but it will stringify macro values instead of names if you give it a macro.
#define XStringify(x) Stringify(x)

// Takes a notification code and returns it as an HMENU that uses the high word so it words the same as system notification codes.
#define NOTIF_CODIFY(x) MAKEWPARAM(0, x)

#define FILE_MENU_NEW 1
#define FILE_MENU_OPEN 2
#define FILE_MENU_SAVE 3
#define FILE_MENU_SAVEAS 4
#define FILE_MENU_EXIT 5

#define EDIT_MENU_UNDO 6
#define EDIT_MENU_REDO 7

#define NEW_FILE_OPTIONS_OK 1
#define NEW_FILE_OPTIONS_CANCEL 2

#define MSG_BOX_CANCEL 2
#define MSG_BOX_NO 7
#define MSG_BOX_YES 6

// File length is measured in seconds.
#define FILE_MIN_LENGTH 1
#define FILE_MAX_LENGTH 3600
#define NEW_FILE_DEFAULT_LENGTH 60
#define LENGTH_TRACKBAR_LINESIZE 1
#define LENGTH_TRACKBAR_PAGESIZE 60

// Frequency is measured in Hertz.
#define NEW_FILE_DEFAULT_FREQUENCY 44100
#define FREQUENCY_TRACKBAR_LINESIZE 50
#define FREQUENCY_TRACKBAR_PAGESIZE 1000

#define NEW_FILE_TRACKBAR_TICKS 11 // How many ticks we want to have in a trackbar.

#define MAIN_WINDOW_CLASS TEXT("MainWindow")
#define NEW_FILE_OPTIONS_CLASS TEXT("NewFileOptions")
#define SELECT_FILE_OPTION_CLASS TEXT("SelectFileOption")

#define BUTTON_CLASS TEXT("Button")
#define EDIT_CLASS TEXT("Edit")
#define STATIC_CLASS TEXT("Static")

#define FILE_FILTER TEXT("Wave files (*.wav;*.wave)\0*.wav;*.wave\0")

static HWND mainWindowHandle = NULL;
static NewFileOptionsWindow* newFileOptionsHandles = NULL;

#pragma region Initialization

char InitializeWindows(HINSTANCE instanceHandle)
{
    if (!RegisterClasses(instanceHandle))
    {
        return FALSE;
    }
    
    // Creates main window.
    mainWindowHandle = CreateWindow(MAIN_WINDOW_CLASS, TEXT("Untitled - Fourier"), WS_OVERLAPPED | WS_MINIMIZEBOX | WS_SYSMENU | WS_VISIBLE, 600, 250, 850, 700, NULL, NULL, NULL, NULL);
    SetWindowPos(mainWindowHandle, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE); // This line fixes a bug where when child windows that spawned child windows are closed, the main window gets minimized.
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
    mainWindowClass.lpszClassName = MAIN_WINDOW_CLASS;
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
    dialog.lpszClassName = NEW_FILE_OPTIONS_CLASS;
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
    dialog.lpszClassName = SELECT_FILE_OPTION_CLASS;
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
    AddMainWindowControls(windowHandle);
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

void AddMainWindowControls(HWND windowHandle)
{
}

void PopSelectFileOptionDialog(HWND windowHandle)
{
    CreateWindow(SELECT_FILE_OPTION_CLASS, TEXT("Select File Option - Fourier"), WS_OVERLAPPED | WS_VISIBLE | WS_SYSMENU, 700, 450, 330, 120, windowHandle, NULL, NULL, NULL);
    EnableWindow(windowHandle, FALSE);
}

void ProcessMainWindowCommand(HWND windowHandle, WPARAM wparam, LPARAM lparam)
{
    switch (HIWORD(wparam))
    {
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
    newFileOptionsHandles->handle = CreateWindow(NEW_FILE_OPTIONS_CLASS, TEXT("New File Options - Fourier"),
        WS_VISIBLE | WS_OVERLAPPED | WS_SYSMENU, 800, 500, 362, 212, windowHandle, NULL, NULL, NULL);
    newFileOptionsHandles->parent = windowHandle;
    EnableWindow(windowHandle, FALSE); // Disabling the parent because this is a modal window.
}

void FileOpen(HWND windowHandle)
{
    // Giving the user a chance to save if there is unsaved progress.
    if (HasUnsavedProgress())
    {
        int decision = MessageBox(windowHandle, TEXT("There is unsaved progress that will be lost if you proceed without saving it. Would you like to save?"), TEXT("Warning"), MB_ICONWARNING | MB_YESNOCANCEL);

        switch (decision)
        {
            case MSG_BOX_CANCEL:
                return; // Quitting the whole function in case of cancel.
            case MSG_BOX_NO:
                break; // Proceeding without saving in case of no.
            case MSG_BOX_YES:
                FileSave(windowHandle); // Saving first in case of yes.
                break;
            default:
                break;
        }
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
        ReadWaveFile(filename);
    }
    else
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
    if (IsFileNew())
    {
        FileSaveAs(windowHandle);
    }
    else
    {
        WriteWaveFile();
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
        WriteNewWaveFile(filename);
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
    AddTrackbarWithTextbox(windowHandle, &(newFileOptionsHandles->lengthTrackbar), &(newFileOptionsHandles->lengthTextbox),
        10, FILE_MIN_LENGTH, FILE_MAX_LENGTH, NEW_FILE_DEFAULT_LENGTH, LENGTH_TRACKBAR_LINESIZE, LENGTH_TRACKBAR_PAGESIZE, TEXT(XStringify(NEW_FILE_DEFAULT_LENGTH)), TEXT("seconds"));

    // Adding trackbar-textbox-units triple for selecting file sample rate.
    AddTrackbarWithTextbox(windowHandle, &(newFileOptionsHandles->frequencyTrackbar), &(newFileOptionsHandles->frequencyTextbox),
        55, FILE_MIN_FREQUENCY, FILE_MAX_FREQUENCY, NEW_FILE_DEFAULT_FREQUENCY, FREQUENCY_TRACKBAR_LINESIZE, FREQUENCY_TRACKBAR_PAGESIZE, TEXT(XStringify(NEW_FILE_DEFAULT_FREQUENCY)), TEXT("Hz"));

    // Adding a radio menu for choosing bit depth. Important that we add these such that the i'th cell indicates i+1 bytes.
    newFileOptionsHandles->depthOptions[0] = CreateWindow(BUTTON_CLASS, TEXT("8-bit"), WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON | BS_VCENTER | WS_GROUP, 18, 95, 80, 30, windowHandle, NULL, NULL, NULL);
    newFileOptionsHandles->depthOptions[1] = CreateWindow(BUTTON_CLASS, TEXT("16-bit"), WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON | BS_VCENTER, 98, 95, 80, 30, windowHandle, NULL, NULL, NULL);
    newFileOptionsHandles->depthOptions[2] = CreateWindow(BUTTON_CLASS, TEXT("24-bit"), WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON | BS_VCENTER, 178, 95, 80, 30, windowHandle, NULL, NULL, NULL);
    newFileOptionsHandles->depthOptions[3] = CreateWindow(BUTTON_CLASS, TEXT("32-bit"), WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON | BS_VCENTER, 258, 95, 80, 30, windowHandle, NULL, NULL, NULL);
    SendMessage(newFileOptionsHandles->depthOptions[1], BM_SETCHECK, BST_CHECKED, 0); // Setting default selection for this menu.

    // Adding buttons for ok and cancel.
    CreateWindow(BUTTON_CLASS, TEXT("Cancel"), WS_VISIBLE | WS_CHILD | BS_CENTER | BS_VCENTER, 198, 140, 70, 35, windowHandle, (HMENU)NEW_FILE_OPTIONS_CANCEL, NULL, NULL);
    CreateWindow(BUTTON_CLASS, TEXT("Ok"), WS_VISIBLE | WS_CHILD | BS_CENTER | BS_VCENTER, 278, 140, 70, 35, windowHandle, (HMENU)NEW_FILE_OPTIONS_OK, NULL, NULL);
}

void AddTrackbarWithTextbox(HWND windowHandle, HWND* trackbar, HWND* textbox, int yPos, int minValue, int maxValue, int defaultValue, int linesize, int pagesize, LPCTSTR defaultValueStr, LPCTSTR units)
{
    // Calculating tick length given the interval length and how many ticks we want to have. Rounding it up instead of down because I would rather have too few ticks than too many.
    div_t divResult = div(maxValue - minValue, NEW_FILE_TRACKBAR_TICKS);
    WPARAM tickLength = divResult.quot + (divResult.rem ? 1 : 0);

    // Adding trackbar.
    *trackbar = CreateWindow(TRACKBAR_CLASS, NULL, WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS, 10, yPos, 200, 30, windowHandle, 0, NULL, NULL);
    SendMessage(*trackbar, TBM_SETRANGEMIN, (WPARAM)FALSE, (LPARAM)minValue); // Configuring min range of trackbar.
    SendMessage(*trackbar, TBM_SETRANGEMAX, (WPARAM)TRUE, (LPARAM)maxValue); // Configuring max range of trackbar.
    SendMessage(*trackbar, TBM_SETPOS, (WPARAM)TRUE, (LPARAM)defaultValue); // Configuring default selection.
    SendMessage(*trackbar, TBM_SETLINESIZE, 0, (LPARAM)linesize); // Configuring sensitivity for arrow key inputs.
    SendMessage(*trackbar, TBM_SETPAGESIZE, 0, (LPARAM)pagesize); // Configuring sensitivity for PGUP/PGDOWN inputs.
    SendMessage(*trackbar, TBM_SETTICFREQ, tickLength, 0); // Configuring how many ticks are on it.

    // Adding textbox.
    *textbox = CreateWindow(EDIT_CLASS, defaultValueStr, WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER | ES_CENTER, 220, yPos + 4, 65, 22, windowHandle, NULL, NULL, NULL);
    SendMessage(*textbox, EM_SETLIMITTEXT, (WPARAM)6, 0); // Setting character limit.
    CreateWindow(STATIC_CLASS, units, WS_VISIBLE | WS_CHILD, 290, yPos + 6, 60, 22, windowHandle, NULL, NULL, NULL);
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
    int length = (int)SendMessage(newFileOptionsHandles->lengthTrackbar, TBM_GETPOS, 0, 0);

    if (!(FILE_MIN_LENGTH <= length && length <= FILE_MAX_LENGTH))
    {
        MessageBox(windowHandle, TEXT("Invalid file length."), NULL, MB_ICONERROR | MB_OK);
        return;
    }

    // Getting the sample rate same as we did with file length.
    int frequency = (int)SendMessage(newFileOptionsHandles->frequencyTrackbar, TBM_GETPOS, 0, 0);
    
    if (!(FILE_MIN_FREQUENCY <= frequency && frequency <= FILE_MAX_FREQUENCY))
    {
        MessageBox(windowHandle, TEXT("Invalid sample rate."), NULL, MB_ICONERROR | MB_OK);
        return;
    }

    // Getting the bit depth (as byte depth because it works better with the algorithm). The i'th cell in the array indicates i+1 byte depth. We assume the first cell is selected and only check the rest.
    int byteDepth = 1;

    for (int i = 1; i < 4; i++)
    {
        if (SendMessage(newFileOptionsHandles->depthOptions[i], BM_GETCHECK, 0, 0) == BST_CHECKED)
        {
            byteDepth = i + 1;
            break; // Only one radio button can be checked so we can exit now.
        }
    }

    // TODO: pass this data to the FileManager or SoundEditor for actually creating the new file. Also delete this next line, it's only to temporarily make a warning go away.
    if (byteDepth) {}
    CloseNewFileOptions(windowHandle);
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
        case EN_CHANGE:
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
    SendMessage(textbox, WM_GETTEXT, 16, (LPARAM)buffer);
    unsigned long int length = _tcstoul(buffer, NULL, 10);
    int min = SendMessage(trackbar, TBM_GETRANGEMIN, 0, 0);
    int max = SendMessage(trackbar, TBM_GETRANGEMAX, 0, 0);
    length = length > max ? max : (length < min ? min : length); // Clamping length between min and max.
    SendMessage(trackbar, TBM_SETPOS, (WPARAM)TRUE, (LPARAM)length);
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
        case WM_ENABLE:
            if (wparam)
            {
                // TODO: Check if a file has been created or opened probably using something in FileManager or SoundEditor that doesn't exist yet, and only close this window if that condition is met.
                // TODO: This line causes the program to crash when it occurs after closing the open file dialog. I imagine it will do so even when it's made conditional. Maybe a small delay will fix it.
                CloseSelectFileOption(windowHandle);
            }

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
    CreateWindow(STATIC_CLASS, TEXT("Create a new file or open an existing one?"), WS_VISIBLE | WS_CHILD | SS_CENTER, 0, 15, 330, 30, windowHandle, NULL, NULL, NULL);

    // Adding buttons for new file and open file.
    CreateWindow(BUTTON_CLASS, TEXT("New file"), WS_VISIBLE | WS_CHILD | BS_CENTER | BS_VCENTER, 85, 50, 70, 35, windowHandle, (HMENU)FILE_MENU_NEW, NULL, NULL);
    CreateWindow(BUTTON_CLASS, TEXT("Open file"), WS_VISIBLE | WS_CHILD | BS_CENTER | BS_VCENTER, 175, 50, 70, 35, windowHandle, (HMENU)FILE_MENU_OPEN, NULL, NULL);
}

void CloseSelectFileOption(HWND windowHandle)
{
    // Enabling the parent window and destroying this one.
    EnableWindow(GetWindow(windowHandle, GW_OWNER), TRUE);
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
