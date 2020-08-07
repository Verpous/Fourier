#ifndef WINDOWS_MANAGER_H
#define WINDOWS_MANAGER_H

#include <windows.h>

// Handler for any messages sent to our main window.
LRESULT CALLBACK MainWindowProcedure(HWND, UINT, WPARAM, LPARAM);

void CreateMainWindow(HWND);
void AddMenus(HWND);
void AddControls(HWND);

void ProcessMainWindowCommand(HWND, WPARAM, LPARAM);

#endif