#include <stdio.h>
#include "WindowManager.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    fprintf(stderr, "\n~~~STARTING A RUN~~~\n");

    // Creating the struct which describes our main window.
    LPTSTR mainWindowClassName = TEXT("MainWindow");

    WNDCLASS mainWindowClass = {0};
    mainWindowClass.hbrBackground = (HBRUSH)COLOR_WINDOW;
    mainWindowClass.hCursor = LoadCursor(hInstance, IDC_ARROW);
    mainWindowClass.hInstance = hInstance;
    mainWindowClass.lpszClassName = mainWindowClassName;
    mainWindowClass.lpfnWndProc = MainWindowProcedure;

    // Registering this class. If it fails, we'll log it and end the program.
    if (!RegisterClass(&mainWindowClass))
    {
        fprintf(stderr, "RegisterClass failed with error code: 0x%lX", GetLastError());
        return -1;
    }

    // Now that the class is registered we can Create our main window!
    CreateWindow(mainWindowClassName, TEXT("Untitled - Fourier"), WS_OVERLAPPED | WS_MINIMIZEBOX | WS_SYSMENU | WS_VISIBLE, 600, 250, 850, 700, NULL, NULL, NULL, NULL);

    // Entering our message loop.
    MSG msg = {0};

    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}