#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <wchar.h>
#include <stdlib.h>

LRESULT CALLBACK WindowProcedure(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) 
{
    int val;

    switch (msg)
    {
        case WM_COMMAND:
            return 0;
        case WM_CREATE:
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProc(hWnd, msg, wp, lp);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    LPTSTR className = TEXT("mainWindow");

    WNDCLASS wc = {0};
    wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hInstance = hInstance;
    wc.lpszClassName = className;
    wc.lpfnWndProc = WindowProcedure;

    if (!RegisterClass(&wc))
    {
        // TODO: write to stderr instead of all this shit, modify makefile to redirect stderr to some log.

        // Writing the error number to a string. 17 is enough to store any 16-nibble number with a null terminator.
        TCHAR errorNumBuffer[17];
        _ultot(GetLastError(), errorNumBuffer, 16);

        // Concatenating the error number to our message.
        TCHAR msg[64];
        _tcscpy(msg, TEXT("RegisterClass failed with error code: 0x"));
        _tcscat(msg, errorNumBuffer);

        // Displaying the message.
        MessageBox(NULL, msg, TEXT("RegisterClass error"), MB_OK);
        return -1;
    }

    RegisterDialogClass(hInstance);

    CreateWindow(className, TEXT("Hello there!"), WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 500, 500, NULL, NULL, NULL, NULL);

    MSG msg = {0};

    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}