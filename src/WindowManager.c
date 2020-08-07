#include "WindowManager.h"

#define FILE_MENU_NEW 1
#define FILE_MENU_OPEN 2
#define FILE_MENU_SAVE 3
#define FILE_MENU_SAVEAS 4
#define FILE_MENU_EXIT 5

#define EDIT_MENU_UNDO 6
#define EDIT_MENU_REDO 7

LRESULT CALLBACK MainWindowProcedure(HWND windowHandle, UINT msg, WPARAM wparam, LPARAM lparam) 
{
    switch (msg)
    {
        case WM_COMMAND:
            ProcessMainWindowCommand(windowHandle, wparam, lparam);
            return 0;
        case WM_CREATE:
            CreateMainWindow(windowHandle);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProc(windowHandle, msg, wparam, lparam);
    }
}

void CreateMainWindow(HWND windowHandle)
{
    AddMenus(windowHandle);
    AddControls(windowHandle);
}

void AddMenus(HWND windowHandle)
{
    HMENU menuHandler = CreateMenu();
    HMENU fileMenuHandler = CreateMenu();
    HMENU editMenuHandler = CreateMenu();

    // Appending file menu options.
    AppendMenu(fileMenuHandler, MF_STRING, FILE_MENU_NEW, TEXT("New\tCtrl+N"));
    AppendMenu(fileMenuHandler, MF_STRING, FILE_MENU_OPEN, TEXT("Open\tCtrl+O"));
    AppendMenu(fileMenuHandler, MF_STRING, FILE_MENU_SAVE, TEXT("Save\tCtrl+S"));
    AppendMenu(fileMenuHandler, MF_STRING, FILE_MENU_SAVEAS, TEXT("Save as\tCtrl+Shift+S"));
    AppendMenu(fileMenuHandler, MF_SEPARATOR, 0, NULL); // Separator between exit and all the other options.
    AppendMenu(fileMenuHandler, MF_STRING, FILE_MENU_EXIT, TEXT("Exit"));

    // Appending edit menu options.
    AppendMenu(editMenuHandler, MF_STRING, EDIT_MENU_UNDO, TEXT("Undo\tCtrl+Z"));
    AppendMenu(editMenuHandler, MF_STRING, EDIT_MENU_REDO, TEXT("Redo\tCtrl+Y"));

    // Adding both menus.
    AppendMenu(menuHandler, MF_POPUP, (UINT_PTR)fileMenuHandler, TEXT("File"));
    AppendMenu(menuHandler, MF_POPUP, (UINT_PTR)editMenuHandler, TEXT("Edit"));

    SetMenu(windowHandle, menuHandler);
}

void AddControls(HWND windowHandle)
{
    // CreateWindow(TEXT("Static"), TEXT("Name: "), WS_VISIBLE | WS_CHILD, 100, 50, 98, 38, hWnd, NULL, NULL, NULL);
    // hName = CreateWindow(TEXT("Edit"), TEXT(""), WS_VISIBLE | WS_CHILD | WS_BORDER, 200, 50, 98, 38, hWnd, NULL, NULL, NULL);

    // CreateWindow(TEXT("Static"), TEXT("Age: "), WS_VISIBLE | WS_CHILD, 100, 90, 98, 38, hWnd, NULL, NULL, NULL);
    // hAge = CreateWindow(TEXT("Edit"), TEXT(""), WS_VISIBLE | WS_CHILD | WS_BORDER, 200, 90, 98, 38, hWnd, NULL, NULL, NULL);

    // HWND hBut = CreateWindow(TEXT("Button"), NULL, WS_VISIBLE | WS_CHILD | BS_BITMAP, 150, 140, 98, 38, hWnd, (HMENU)GENERATE_BUTTON, NULL, NULL);
    // SendMessage(hBut, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hGenerateImage);

    // hOut = CreateWindow(TEXT("Edit"), TEXT(""), WS_VISIBLE | WS_CHILD | WS_BORDER, 100, 200, 300, 38, hWnd, NULL, NULL, NULL);
    // hLogo = CreateWindow(TEXT("Static"), NULL, WS_VISIBLE | WS_CHILD | SS_BITMAP, 350, 60, 100, 100, hWnd, NULL, NULL, NULL);
    // SendMessage(hLogo, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hLogoImage);

    // CreateWindow(TEXT("Button"), TEXT("Open file"), WS_VISIBLE | WS_CHILD, 100, 250, 100, 50, hWnd, (HMENU)OPEN_FILE_BUTTON, NULL, NULL);
    // CreateWindow(TEXT("Button"), TEXT("Save file"), WS_VISIBLE | WS_CHILD, 225, 250, 100, 50, hWnd, (HMENU)SAVE_FILE_BUTTON, NULL, NULL);
    // hEdit = CreateWindow(TEXT("EDIT"), TEXT(""), WS_VISIBLE | WS_CHILD | ES_MULTILINE | WS_BORDER | WS_VSCROLL | WS_HSCROLL, 100, 320, 300, 100, hWnd, NULL, NULL, NULL);
}

void ProcessMainWindowCommand(HWND windowHandle, WPARAM wparam, LPARAM lparam)
{
    switch (wparam)
    {
        case FILE_MENU_NEW:
            break;
        case FILE_MENU_OPEN:
            break;
        case FILE_MENU_SAVE:
            break;
        case FILE_MENU_SAVEAS:
            break;
        case FILE_MENU_EXIT:
            DestroyWindow(windowHandle); // TODO: Check if there's unsaved work, if there is then prompt the user with an "are you sure?" message and give him a chance to still save his work.
            break;
        case EDIT_MENU_REDO:
            break;
        case EDIT_MENU_UNDO:
            break;
        default:
            break;
    }
}