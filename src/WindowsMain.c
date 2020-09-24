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

#include "WindowsMain.h"
#include "MyUtils.h"
#include "Resource.h"
#include <stdio.h>		// For printing errors and such.
#include <commctrl.h>	// For some trackbar-related things.
#include <tchar.h>		// For dealing with unicode and ANSI strings.
#include <shlwapi.h> 	// For PathStipPath.
#include <gdiplus.h>	// For drawing graphs.
#include <time.h>		// To seed rand.

// Takes a notification code and returns it as an HMENU that uses the high word so it works the same as system notification codes.
#define NOTIF_CODIFY(x) MAKEWPARAM(0, x)

// This is the value in the HIWORD of wparam when windows sends a message about a shortcut from the accelerator table being pressed.
#define ACCELERATOR_SHORTCUT_PRESSED 1

// This needs to be above 0x8000 and different than the values in Resources.h.
#define PROGRAM_EXIT 0x8008

// This message tells the main window to pop the select file option dialog.
#define WM_SELECTFILE WM_USER

// These don't need to be above 0x8000.
#define NEW_FILE_OPTIONS_OK 1
#define NEW_FILE_OPTIONS_CANCEL 2

// File length is measured in seconds.
#define FILE_MIN_LENGTH 1
#define FILE_MAX_LENGTH 3600
#define NEW_FILE_DEFAULT_LENGTH 10 // TODO: change this something like 30 or 60 when the program is finished.
#define LENGTH_TRACKBAR_LINESIZE 1
#define LENGTH_TRACKBAR_PAGESIZE 60

// Frequency is measured in Hertz.
#define FILE_MIN_FREQUENCY 8000
#define FILE_MAX_FREQUENCY 96000
#define NEW_FILE_DEFAULT_FREQUENCY 44100
#define FREQUENCY_TRACKBAR_LINESIZE 50
#define FREQUENCY_TRACKBAR_PAGESIZE 1000

// Smoothing is unitless, and is in fact in the [0,1] range, so MAX_SMOOTHING isn't actually the max smoothing it's just how precise can you get between [0,1].
#define MIN_SMOOTHING 0
#define MAX_SMOOTHING 1000
#define DEFAULT_SMOOTHING 500
#define SMOOTHING_TRACKBAR_LINESIZE 1
#define SMOOTHING_TRACKBAR_PAGESIZE 10

// How many ticks we want to have in a trackbar.
#define TRACKBAR_TICKS 11

// Dimensions of windows. Everything is in pixels.
#define MAIN_WINDOW_WIDTH 1152 // Used to be 850
#define MAIN_WINDOW_HEIGHT 864 // Used to be 700
#define NEW_FILE_OPTIONS_WIDTH 380
#define NEW_FILE_OPTIONS_HEIGHT 220
#define SELECT_FILE_OPTION_WIDTH 330
#define SELECT_FILE_OPTION_HEIGHT 120

// Dimensions of controls. Everything is in pixels.
#define INPUT_TEXTBOX_WIDTH 90
#define INPUT_TEXTBOX_HEIGHT 22
#define STATIC_TEXT_HEIGHT 16
#define STATIC_UNITS_WIDTH 50
#define TRACKBAR_WIDTH (INPUT_TEXTBOX_WIDTH + UNITS_AFTER_TEXTBOX_SPACING + STATIC_UNITS_WIDTH + CONTROL_DESCRIPTION_WIDTH) // This value ensures that the smoothing trackbar has the right alignment with other controls.
#define TRACKBAR_HEIGHT 30
#define BUTTON_WIDTH 70
#define BUTTON_HEIGHT 35
#define RADIO_WIDTH 94
#define GRAPH_WIDTH (MAIN_WINDOW_WIDTH - 130) // We want the graph to take up as much as it can of the screen while still looking good.
#define GRAPH_HEIGHT 235 // This should be an odd number because we divide (GRAPH_HEIGHT - 1) by 2 and so there's as many pixels above 0 as below.
#define CONTROL_DESCRIPTION_WIDTH 80

// Some positions that are used as baselines from which all other positions of their respective menus are derived.
// File editor.
#define WAVEFORM_GRAPH_Y_POS 70

// New file options.
#define CHOOSE_FILE_LENGTH_X_POS 10
#define CHOOSE_FILE_LENGTH_Y_POS 15

// Some pixel distances and spacings.
#define INPUTS_Y_SPACING 50
#define UNITS_AFTER_TEXTBOX_SPACING 5
#define GENERIC_SPACING 10 // We use this to distance some things.

// Decibel is a unit which requires a reference point to measure against. This is that for the logarithm scale of fourier graphs.
#define FOURIER_DECIBEL_REFERENCE 5.0

// How many characters are allowed in input controls for numbers (which is all of them).
#define INPUT_TEXTBOX_CHARACTER_LIMIT 8

// WindowClass names.
#define WC_MAINWINDOW TEXT("MainWindow")
#define WC_NEWFILEOPTIONS TEXT("NewFileOptions")
#define WC_SELECTFILEOPTION TEXT("SelectFileOption")

#define FILE_FILTER TEXT("Wave files (*.wav;*.wave)\0*.wav;*.wave\0")
#define TITLE_POSTFIX TEXT(" - Fourier")

static HWND mainWindowHandle = NULL;
static NewFileOptionsWindow* newFileOptionsHandles = NULL;
static FileEditor fileEditor = {0};

#pragma region Initialization

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	fprintf(stderr, "\n~~~STARTING A RUN~~~\n");
	srand(time(NULL)); // Seeding the RNG. We use it for dithering.

	if (!InitializeWindows(hInstance))
	{
		return -1;
	}

	HACCEL acceleratorHandle = LoadAccelerators(hInstance, MAKEINTRESOURCE(ACCELERATOR_TABLE_ID));
	MSG msg = {0};

	// Entering our message loop.
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(mainWindowHandle, acceleratorHandle, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return 0;
}

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
	// WS_CLIPSIBLINGS makes child windows not draw over each other.
	// TODO: get screen dimensions and spawn window in position such that it's at the center of the screen? If I do, I should also adjust popselectfileoption to spawn in the center.
	mainWindowHandle = CreateWindow(WC_MAINWINDOW, TEXT("Untitled") TITLE_POSTFIX, WS_OVERLAPPED | WS_MINIMIZEBOX | WS_SYSMENU | WS_VISIBLE | WS_CLIPSIBLINGS,
		600, 250, MAIN_WINDOW_WIDTH, MAIN_WINDOW_HEIGHT, NULL, NULL, NULL, NULL);
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
	mainWindowClass.hIcon = LoadIcon(instanceHandle, MAKEINTRESOURCE(PROGRAM_ICON_ID));

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
			PostMessage(windowHandle, WM_SELECTFILE, 0, 0); // This is a hack to resolve a bug that I couldn't make the modal dialog take focus when the main window was just created, so I post a message that does it with a delay.
			return 0;
		case WM_SELECTFILE:
			PopSelectFileOptionDialog(windowHandle);
			return 0;
		case WM_COMMAND:
			ProcessMainWindowCommand(windowHandle, wparam, lparam);
			return 0;
		case WM_HSCROLL:
			// Updating the textbox to match the trackbar.
			if (fileEditor.smoothingTrackbar == (HWND)lparam)
			{
				SyncTextboxToTrackbarFloat(fileEditor.smoothingTrackbar, fileEditor.smoothingTextbox);
			}

			return 0;
		case WM_NOTIFY:
			if (((LPNMHDR)lparam)->code == TCN_SELCHANGE)
			{
				unsigned short currentChannel = TabCtrl_GetCurSel(fileEditor.channelTabs);

				// If the waveform of this channel hasn't been painted yet, then neither has the fourier transform. If it has been painted, then so has the fourier transform.
				if (fileEditor.waveformGraphs[currentChannel] == NULL)
				{
					PlotChannelGraphs(currentChannel);
				}
				
				DisplayChannelGraphs(currentChannel);
			}

			return 0;
		case WM_CLOSE:
			// Prompt the user to save his progress before it is lost.
			if (PromptSaveProgress(windowHandle))
			{
				// Only proceeding if the user didn't choose to abort.
				CloseFileEditor();

				// This aspect of the file editor is only allocated once on the first file open which means it only needs to be deleted once, here.
				if (fileEditor.graphingDC != NULL)
				{
					DeleteDC(fileEditor.graphingDC);
				}

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
	AppendMenu(fileMenuHandler, MF_STRING, NOTIF_CODIFY(FILE_ACTION_NEW), TEXT("New\tCtrl+N"));
	AppendMenu(fileMenuHandler, MF_STRING, NOTIF_CODIFY(FILE_ACTION_OPEN), TEXT("Open\tCtrl+O"));
	AppendMenu(fileMenuHandler, MF_STRING | MF_GRAYED, NOTIF_CODIFY(FILE_ACTION_SAVE), TEXT("Save\tCtrl+S"));
	AppendMenu(fileMenuHandler, MF_STRING | MF_GRAYED, NOTIF_CODIFY(FILE_ACTION_SAVEAS), TEXT("Save as\tCtrl+Shift+S"));
	AppendMenu(fileMenuHandler, MF_SEPARATOR, 0, NULL); // Separator between exit and all the other options.
	AppendMenu(fileMenuHandler, MF_STRING, NOTIF_CODIFY(PROGRAM_EXIT), TEXT("Exit"));

	// Appending edit menu options. Initially, undo and redo are grayed out.
	AppendMenu(editMenuHandler, MF_STRING | MF_GRAYED, NOTIF_CODIFY(EDIT_ACTION_UNDO), TEXT("Undo\tCtrl+Z"));
	AppendMenu(editMenuHandler, MF_STRING | MF_GRAYED, NOTIF_CODIFY(EDIT_ACTION_REDO), TEXT("Redo\tCtrl+Y"));
	AppendMenu(editMenuHandler, MF_STRING | MF_GRAYED, NOTIF_CODIFY(EDIT_ACTION_APPLY), TEXT("Apply\tCtrl+E"));

	// Adding both menus.
	AppendMenu(menuHandler, MF_POPUP, (UINT_PTR)fileMenuHandler, TEXT("File"));
	AppendMenu(menuHandler, MF_POPUP, (UINT_PTR)editMenuHandler, TEXT("Edit"));

	SetMenu(windowHandle, menuHandler);
}

void PopSelectFileOptionDialog(HWND windowHandle)
{
	CreateWindow(WC_SELECTFILEOPTION, TEXT("Select File Option - Fourier"), WS_OVERLAPPED | WS_VISIBLE | WS_SYSMENU,
		575 + ((MAIN_WINDOW_WIDTH - SELECT_FILE_OPTION_WIDTH) / 2), 200 + ((MAIN_WINDOW_HEIGHT - SELECT_FILE_OPTION_HEIGHT) / 2), SELECT_FILE_OPTION_WIDTH, SELECT_FILE_OPTION_HEIGHT, windowHandle, NULL, NULL, NULL);
	EnableWindow(windowHandle, FALSE);
}

void ProcessMainWindowCommand(HWND windowHandle, WPARAM wparam, LPARAM lparam)
{
	switch (HIWORD(wparam))
	{
		case BN_CLICKED:

			// The buttons use the same identifiers as their corresponding menu options so we can forward this to be handled as a menu item press.
			switch (LOWORD(wparam))
			{
				case EDIT_ACTION_REDO:
				case EDIT_ACTION_UNDO:
				case EDIT_ACTION_APPLY:
					ProcessMainWindowCommand(windowHandle, NOTIF_CODIFY(LOWORD(wparam)), lparam);
					break;
			}

		break;
		case ACCELERATOR_SHORTCUT_PRESSED:
		
			// Generating a menu button pressed event same as for BN_CLICKED, but only when this is the active window.
			if (GetActiveWindow() == windowHandle)
			{
				ProcessMainWindowCommand(windowHandle, NOTIF_CODIFY(LOWORD(wparam)), lparam);
			}

			break;
		case EN_UPDATE: // EN_UPDATE is sent before the screen gets updated, EN_CHANGE gets sent after.
			;
			HWND focusedWindow = GetFocus();
			HWND controlHandle = (HWND)lparam;

			// Only matching the trackbar to the textbox when it's in fact the textbox that changed,
			// and when the textbox is in focus to avoid the fact that when I programmatically update the textbox due to trackbar movement, it then fires this event.
			if (focusedWindow == fileEditor.smoothingTextbox && controlHandle == fileEditor.smoothingTextbox)
			{
				SyncTrackbarToTextboxFloat(fileEditor.smoothingTrackbar, fileEditor.smoothingTextbox);
			}

			break;
		case FILE_ACTION_NEW:
			FileNew(windowHandle);
			break;
		case FILE_ACTION_OPEN:
			FileOpen(windowHandle);
			break;
		case FILE_ACTION_SAVE:
			FileSave(windowHandle);
			break;
		case FILE_ACTION_SAVEAS:
			FileSaveAs(windowHandle);
			break;
		case EDIT_ACTION_REDO:
			Redo(windowHandle);
			break;
		case EDIT_ACTION_UNDO:
			Undo(windowHandle);
			break;
		case EDIT_ACTION_APPLY:
			ApplyModificationFromInput(windowHandle);
			break;
		case PROGRAM_EXIT:
			DestroyWindow(windowHandle);
			break;
		default:
			break;
	}
}

void FileNew(HWND windowHandle)
{
	if (newFileOptionsHandles != NULL)
	{
		fprintf(stderr, "Tried to create new file while already in that menu.\n");
		return;
	}

	newFileOptionsHandles = calloc(1, sizeof(NewFileOptionsWindow));
	newFileOptionsHandles->handle = CreateWindow(WC_NEWFILEOPTIONS, TEXT("New File Options - Fourier"),
												 WS_VISIBLE | WS_OVERLAPPED | WS_SYSMENU, 950, 550, NEW_FILE_OPTIONS_WIDTH, NEW_FILE_OPTIONS_HEIGHT, windowHandle, NULL, NULL, NULL);
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
		FileInfo* fileInfo;
		ReadWaveResult result = ReadWaveFile(&fileInfo, filename);

		if (!ResultHasError(result))
		{
			if (ResultHasWarning(result))
			{
				if (ResultWarningCode(result) & FILE_CHUNK_WARNING)
				{
					int choice = MessageBox(windowHandle,
											TEXT("The file contains some information which is ignored by this program, which may lead to unexpected results."),
											TEXT("Warning"), MB_OKCANCEL | MB_ICONWARNING);

					if (choice == IDCANCEL)
					{
						CloseWaveFile(fileInfo);
						return;
					}
				}

				if (ResultWarningCode(result) & FILE_CHAN_WARNING)
				{
					int choice = MessageBox(windowHandle,
											TEXT("The file contains more channels than this program supports. You will only be able to edit some of the channels."),
											TEXT("Warning"), MB_OKCANCEL | MB_ICONWARNING);

					if (choice == IDCANCEL)
					{
						CloseWaveFile(fileInfo);
						return;
					}
				}
			}

			InitializeFileEditor(windowHandle, fileInfo);
		}
		else // There's an error.
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
					messageText = TEXT("The file has an audio length of 0 seconds.");
					break;
				case FILE_MISC_ERROR:
				default:
					messageText = TEXT("A miscellaneous error occured.");
					break;
			}

			MessageBox(windowHandle, messageText, NULL, MB_OK | MB_ICONERROR);
		}
	}
	else // GetOpenFileName failed.
	{
		DWORD error = CommDlgExtendedError();
		fprintf(stderr, "GetOpenFileName failed with error code %lX\n", error);

		if (error == FNERR_BUFFERTOOSMALL)
		{
			MessageBox(windowHandle, TEXT("Path name exceeds the upper limit of ") TXStringify(MAX_PATH) TEXT(" characters"), NULL, MB_OK | MB_ICONERROR);
		}
	}
}

void FileSave(HWND windowHandle)
{
	// If the file editor isn't open yet, no file has been opened for saving.
	if (!IsEditorOpen())
	{
		return;
	}

	if (IsFileNew(fileEditor.fileInfo))
	{
		FileSaveAs(windowHandle);
	}
	else if (HasUnsavedChanges())
	{
		SetAllChannelsDomain(TIME_DOMAIN);

		while (TRUE)
		{
			if (WriteWaveFile(fileEditor.fileInfo->file, fileEditor.fileInfo, fileEditor.channelsData))
			{
				fileEditor.currentSaveState = fileEditor.modificationStack;
				UpdateWindowTitle();
				break;
			}
			else
			{
				int choice = MessageBox(windowHandle, TEXT("There is insufficient memory for saving this file."), NULL, MB_RETRYCANCEL | MB_ICONERROR);

				if (choice == IDCANCEL)
				{
					break;
				}
			}
		}
	}
}

void FileSaveAs(HWND windowHandle)
{
	// If the file editor isn't open yet, no file has been opened for saving.
	if (!IsEditorOpen())
	{
		return;
	}

	OPENFILENAME ofn = {0};
	TCHAR filename[MAX_PATH];

	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = windowHandle;
	ofn.lpstrFile = filename;
	ofn.nMaxFile = MAX_PATH - 4; // We subtract 4 from the max_path so we have spare room to append .wav in if need be.
	ofn.lpstrFilter = FILE_FILTER;

	// The rest of the code is in a loop because if the operation fails I give the user an option to choose to retry.
	// This is important for saving files because otherwise, if there's an error when the user chose save by a prompt to save before data is lost, the data will be lost.
	while (TRUE)
	{
		filename[0] = TEXT('\0'); // Without this line this doesn't work.

		if (GetSaveFileName(&ofn))
		{
			// Appending .wav to the end of the path if it isn't there.
			size_t pathLen = _tcslen(filename);

			if ((pathLen < 4 || _tcscmp(&(filename[pathLen - 4]), TEXT(".wav")) != 0) &&
				(pathLen < 5 || _tcscmp(&(filename[pathLen - 5]), TEXT(".wave")) != 0))
			{
				_tcscat_s(filename, MAX_PATH, TEXT(".wav"));
			}

			// Checking if the file already exists and popping a warning if it does.
			if (FileExists(filename))
			{
				int choice = MessageBox(windowHandle, TEXT("The existing file will be overwritten by this operation. Proceed anyway?"), TEXT("Warning"), MB_YESNOCANCEL | MB_ICONWARNING);

				// Using an if statement instead of switch statement because I need to be able to use break here and not have it only break from the switch.
				if (choice == IDCANCEL)
				{
					break;
				}
				else if (choice == IDNO)
				{
					continue;
				}
			}

			// On iterations after the first this call won't be expensive. It's better to have this here than before the loop because it's a big computations which can be pointless if the operation is cancelled before it's useful.
			SetAllChannelsDomain(TIME_DOMAIN);

			if (WriteWaveFileAs(fileEditor.fileInfo, filename, fileEditor.channelsData))
			{
				fileEditor.currentSaveState = fileEditor.modificationStack;
				UpdateWindowTitle();
				break;
			}

			int choice = MessageBox(windowHandle, TEXT("There was a problem with creating this file."), NULL, MB_RETRYCANCEL | MB_ICONERROR);

			if (choice == IDCANCEL)
			{
				break;
			}
		}
		else
		{
			DWORD error = CommDlgExtendedError();

			// This happens when the dialog failed because the user clicked cancel.
			if (!error)
			{
				break;
			}

			fprintf(stderr, "GetSaveFileName failed with error code %lX\n", error);

			if (error == FNERR_BUFFERTOOSMALL)
			{
				LPCTSTR msg = error == FNERR_BUFFERTOOSMALL ? TEXT("Path name exceeds the upper limit of ") TXStringify(MAX_PATH - 4) TEXT(" characters.") : TEXT("There was an error in retrieving the file name.");
				int choice = MessageBox(windowHandle, msg, NULL, MB_RETRYCANCEL | MB_ICONERROR);

				if (choice == IDCANCEL)
				{
					break;
				}
			}
		}
	}
}

void Undo(HWND windowHandle)
{
	if (UndoLastModification(fileEditor.channelsData, &(fileEditor.modificationStack)))
	{
		UpdateWindowTitle();
		UpdateUndoRedoState();
		PlotAndDisplayChannelGraphs(TabCtrl_GetCurSel(fileEditor.channelTabs));
	}
}

void Redo(HWND windowHandle)
{
	if (RedoLastModification(fileEditor.channelsData, &(fileEditor.modificationStack)))
	{
		UpdateWindowTitle();
		UpdateUndoRedoState();
		PlotAndDisplayChannelGraphs(TabCtrl_GetCurSel(fileEditor.channelTabs));
	}
}

void ApplyModificationFromInput(HWND windowHandle)
{
	// If the editor isn't open, there's no input controls to read from yet.
	if (!IsEditorOpen())
	{
		return;
	}
	
	TCHAR buffer[16];
	TCHAR* endChar;

	// First reading the from frequency.
	LRESULT length = SendMessage(fileEditor.fromFreqTextbox, WM_GETTEXT, 16, (LPARAM)buffer);
	double fromFreq = _tcstod(buffer, &endChar);

	// This condition is met when the float couldn't be parsed from the string.
	if (length == 0 || endChar != &(buffer[length]))
	{
		MessageBox(windowHandle, TEXT("Invalid input in 'From' textbox."), NULL, MB_OK | MB_ICONERROR);
		return;
	}

	// Now reading to frequency.
	length = SendMessage(fileEditor.toFreqTextbox, WM_GETTEXT, 16, (LPARAM)buffer);
	double toFreq = _tcstod(buffer, &endChar);

	if (length == 0 || endChar != &(buffer[length]))
	{
		MessageBox(windowHandle, TEXT("Invalid input in 'To' textbox."), NULL, MB_OK | MB_ICONERROR);
		return;
	}

	// Reading the change type.
	LRESULT changeSelection = SendMessage(fileEditor.changeTypeDropdown, CB_GETCURSEL, 0, 0);
	ChangeType changeType = changeSelection == 0 ? MULTIPLY : ADD;

	// Reading the change amount. If the user selected the "subtract" option, I multiply the amount by -1.
	length = SendMessage(fileEditor.changeAmountTextbox, WM_GETTEXT, 16, (LPARAM)buffer);
	double changeAmount = _tcstod(buffer, &endChar) * (changeSelection == 2 ? -1.0 : 1.0);

	if (length == 0 || endChar != &(buffer[length]))
	{
		MessageBox(windowHandle, TEXT("Invalid input in 'Amount' textbox."), NULL, MB_OK | MB_ICONERROR);
		return;
	}

	// Reading the smoothing amount.
	double smoothing = (((double)SendMessage(fileEditor.smoothingTrackbar, TBM_GETPOS, 0, 0)) - MIN_SMOOTHING) / (MAX_SMOOTHING - MIN_SMOOTHING);

	// Reading the current channel.
	unsigned short currentChannel = TabCtrl_GetCurSel(fileEditor.channelTabs);

	// Converting fromFreq and toFreq from real frequencies to integer sample numbers.
	// The total samples is twice what it says because this is a real FFT so we only store half the samples, the other half is conjugate symmetric.
	// TODO: make sure these get floored properly or rounded in a different way if it's determined to be preferable.
	unsigned long long totalSamples = 2 * NumOfSamples(fileEditor.channelsData[currentChannel]);
	unsigned long long fromFreqInt = (fromFreq * totalSamples) / fileEditor.fileInfo->format.contents.Format.nSamplesPerSec;
	unsigned long long toFreqInt = (toFreq * totalSamples) / fileEditor.fileInfo->format.contents.Format.nSamplesPerSec;

	// TODO: possibly also check that the samples are close enough together.
	if (!(0 <= fromFreqInt && fromFreqInt < totalSamples / 2) || !(0 <= toFreqInt && toFreqInt < totalSamples / 2))
	{
		MessageBox(windowHandle, TEXT("Frequencies to modify are out of range"), NULL, MB_OK | MB_ICONERROR);
		return;
	}

	if (fromFreqInt >= toFreqInt)
	{
		MessageBox(windowHandle, TEXT("'From' frequency must be smaller than 'To' frequency."), NULL, MB_OK | MB_ICONERROR);
		return;
	}

	SetChannelDomain(currentChannel, FREQUENCY_DOMAIN);

	if (ApplyModification(fromFreqInt, toFreqInt, changeType, changeAmount, smoothing, currentChannel, fileEditor.channelsData, &(fileEditor.modificationStack)))
	{
		UpdateWindowTitle();
		UpdateUndoRedoState();
		PlotAndDisplayChannelGraphs(currentChannel);
	}
	else
	{
		MessageBox(windowHandle, TEXT("There is insufficient memory for applying this change."), NULL, MB_OK | MB_ICONERROR);
	}
}

char PromptSaveProgress(HWND windowHandle)
{
	if (HasUnsavedChanges())
	{
		int choice = MessageBox(windowHandle,
								TEXT("There is unsaved progress that will be lost if you proceed without saving it. Would you like to save?"),
								TEXT("Warning"), MB_ICONWARNING | MB_YESNOCANCEL);

		switch (choice)
		{
			case IDCANCEL:
				return FALSE; // Returning that the program short abort the operation that prompted this.
			case IDNO:
				break; // Proceeding without saving in case of no.
			case IDYES:
				FileSave(windowHandle); // Saving first in case of yes.
				break;
			default:
				break;
		}
	}

	// Returning that the program should continue the operation that prompted this.
	return TRUE;
}

void InitializeFileEditor(HWND windowHandle, FileInfo* fileInfo)
{
	// Closing the file that was open until now.
	CloseFileEditor();
	fileEditor.fileInfo = fileInfo;

	if (LoadPCMInterleaved(fileInfo, &(fileEditor.channelsData)))
	{
		InitializeModificationStack(&(fileEditor.modificationStack));

		unsigned short relevantChannels = GetRelevantChannelsCount(fileEditor.fileInfo);
		fileEditor.channelsDomain = calloc(relevantChannels, sizeof(FunctionDomain));
		fileEditor.waveformGraphs = calloc(relevantChannels, sizeof(HBITMAP));
		fileEditor.fourierGraphs = calloc(relevantChannels, sizeof(HBITMAP));
		fileEditor.graphingDC = CreateCompatibleDC(NULL); // TODO: free this with DeleteDC (and also free the callocs above this) in CloseFileEditor, but only if they're allocated.
		fileEditor.currentSaveState = fileEditor.modificationStack;

		// There's no reason to delete and recreate this between different files. The fact that we only allocate it once means we have to be careful to deallocate it exactly once, though, meaning not in CloseFileEditor.
		if (fileEditor.graphingDC == NULL)
		{
			fileEditor.graphingDC = CreateCompatibleDC(NULL);
		}

		PaintFileEditor();
	}
	else // Deallocating functions if it failed.
	{
		MessageBox(windowHandle, TEXT("There is insufficient memory for opening this file."), NULL, MB_ICONERROR | MB_OK);
		CloseFileEditor();
	}
}

void PaintFileEditor()
{
	if (IsEditorOpen())
	{
		ResetFileEditorPermanents();
	}
	else
	{
		PaintFileEditorPermanents();
	}

	PaintFileEditorTemporaries();
}

void PaintFileEditorPermanents()
{
	fileEditor.channelTabs = CreateWindow(WC_TABCONTROL, TEXT(""), WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | TCS_TABS, 5, 0, MAIN_WINDOW_WIDTH - 15, MAIN_WINDOW_HEIGHT - 53, mainWindowHandle, NULL, NULL, NULL);

	// Originally, all the controls below this were children of this one. But apparently that makes this control receive notifications from its children instead of the main window receiving them, so I changed that.
	CreateWindow(WC_STATIC, TEXT(""), WS_CHILD | WS_VISIBLE | WS_BORDER | SS_WHITEFRAME, 10, 28, MAIN_WINDOW_WIDTH - 25, MAIN_WINDOW_HEIGHT - 87, mainWindowHandle, NULL, NULL, NULL);

	// Adding static controls that for graphing, including the one which will contain the bitmap of the graph later.
	// First, waveforms.
	unsigned int graphXPos = (MAIN_WINDOW_WIDTH - GRAPH_WIDTH) / 2; // This makes it so there's even spacing on both sides of the graph.
	unsigned int unitsXPos = graphXPos - STATIC_UNITS_WIDTH - 2;
	unsigned int waveformUnitsYBasePos = WAVEFORM_GRAPH_Y_POS - (STATIC_TEXT_HEIGHT / 2);

	CreateWindow(WC_STATIC, TEXT("Waveform:"), WS_CHILD | WS_VISIBLE,
		graphXPos, WAVEFORM_GRAPH_Y_POS - STATIC_TEXT_HEIGHT - 8, 200, STATIC_TEXT_HEIGHT, mainWindowHandle, NULL, NULL, NULL);

	CreateWindow(WC_STATIC, TEXT("1"), WS_CHILD | WS_VISIBLE | SS_RIGHT,
		unitsXPos, waveformUnitsYBasePos, STATIC_UNITS_WIDTH, STATIC_TEXT_HEIGHT, mainWindowHandle, NULL, NULL, NULL);

	CreateWindow(WC_STATIC, TEXT("0"), WS_CHILD | WS_VISIBLE | SS_RIGHT,
		unitsXPos, waveformUnitsYBasePos + (GRAPH_HEIGHT / 2), STATIC_UNITS_WIDTH, STATIC_TEXT_HEIGHT, mainWindowHandle, NULL, NULL, NULL);

	CreateWindow(WC_STATIC, TEXT("-1"), WS_CHILD | WS_VISIBLE | SS_RIGHT,
		unitsXPos, waveformUnitsYBasePos + GRAPH_HEIGHT, STATIC_UNITS_WIDTH, STATIC_TEXT_HEIGHT, mainWindowHandle, NULL, NULL, NULL);

	fileEditor.waveformGraphStatic = CreateWindow(WC_STATIC, NULL, WS_CHILD | WS_VISIBLE | WS_BORDER | SS_BITMAP,
		graphXPos, WAVEFORM_GRAPH_Y_POS, GRAPH_WIDTH, GRAPH_HEIGHT, mainWindowHandle, NULL, NULL, NULL);

	// Now doing fourier transforms.
	unsigned int fourierGraphYPos = WAVEFORM_GRAPH_Y_POS + GRAPH_HEIGHT + INPUTS_Y_SPACING;
	unsigned int fourierDecibelUnitsBaseYPos = fourierGraphYPos - (STATIC_TEXT_HEIGHT / 2);
	unsigned int fourierFrequencyUnitsYPos = fourierGraphYPos + GRAPH_HEIGHT + (STATIC_TEXT_HEIGHT / 2);

	CreateWindow(WC_STATIC, TEXT("Frequency spectrum:"), WS_CHILD | WS_VISIBLE,
		graphXPos, fourierGraphYPos - STATIC_TEXT_HEIGHT - 8, 200, STATIC_TEXT_HEIGHT, mainWindowHandle, NULL, NULL, NULL);

	CreateWindow(WC_STATIC, TEXT("0dB"), WS_CHILD | WS_VISIBLE | SS_RIGHT,
		unitsXPos, fourierDecibelUnitsBaseYPos + GRAPH_HEIGHT, STATIC_UNITS_WIDTH, STATIC_TEXT_HEIGHT, mainWindowHandle, NULL, NULL, NULL);

	fileEditor.fourierMaxStatic = CreateWindow(WC_STATIC, TEXT("90dB"), WS_CHILD | WS_VISIBLE | SS_RIGHT,
		unitsXPos,fourierDecibelUnitsBaseYPos, STATIC_UNITS_WIDTH, 27, mainWindowHandle, NULL, NULL, NULL);

	fileEditor.minFreqStatic = CreateWindow(WC_STATIC, TEXT("0KHz"), WS_CHILD | WS_VISIBLE | SS_CENTER,
		graphXPos - (STATIC_UNITS_WIDTH / 2), fourierFrequencyUnitsYPos, STATIC_UNITS_WIDTH, STATIC_UNITS_WIDTH, mainWindowHandle, NULL, NULL, NULL); // TODO: May want to only allow editing starting from 1Hz because 0 is special. Maybe it should say 0 here anyway though.

	fileEditor.maxFreqStatic = CreateWindow(WC_STATIC, TEXT("20KHz"), WS_CHILD | WS_VISIBLE | SS_CENTER,
		graphXPos - (STATIC_UNITS_WIDTH / 2) + GRAPH_WIDTH, fourierFrequencyUnitsYPos, STATIC_UNITS_WIDTH, STATIC_TEXT_HEIGHT, mainWindowHandle, NULL, NULL, NULL); // TODO: change units to Hz when sample rate is below a threshold.

	fileEditor.fourierGraphStatic = CreateWindow(WC_STATIC, NULL, WS_CHILD | WS_VISIBLE | WS_BORDER | SS_BITMAP, graphXPos, fourierGraphYPos, GRAPH_WIDTH, GRAPH_HEIGHT, mainWindowHandle, NULL, NULL, NULL);

	// Adding GUI for choosing what frequency to modify.
	unsigned int chooseFrequenciesYPos = fourierGraphYPos + GRAPH_HEIGHT + INPUTS_Y_SPACING;

	CreateWindow(WC_STATIC, TEXT("From:"), WS_VISIBLE | WS_CHILD,
		graphXPos, chooseFrequenciesYPos, CONTROL_DESCRIPTION_WIDTH, STATIC_TEXT_HEIGHT, mainWindowHandle, NULL, NULL, NULL);

	fileEditor.fromFreqTextbox = CreateWindow(WC_EDIT, TEXT(""), WS_VISIBLE | WS_CHILD | WS_BORDER | ES_CENTER,
		graphXPos + CONTROL_DESCRIPTION_WIDTH, chooseFrequenciesYPos - 2, INPUT_TEXTBOX_WIDTH, INPUT_TEXTBOX_HEIGHT, mainWindowHandle, NULL, NULL, NULL);
	SetWindowSubclass(fileEditor.fromFreqTextbox, FloatTextboxWindowProc, 0, 0); // Setting textbox to only accept float numbers.
	SendMessage(fileEditor.fromFreqTextbox, EM_SETLIMITTEXT, (WPARAM)INPUT_TEXTBOX_CHARACTER_LIMIT, 0); // Setting character limit.

	CreateWindow(WC_STATIC, TEXT("Hz"), WS_VISIBLE | WS_CHILD,
		graphXPos + CONTROL_DESCRIPTION_WIDTH + INPUT_TEXTBOX_WIDTH + UNITS_AFTER_TEXTBOX_SPACING, chooseFrequenciesYPos, STATIC_UNITS_WIDTH, STATIC_TEXT_HEIGHT, mainWindowHandle, NULL, NULL, NULL);

	unsigned int toFrequencyBaseXPos =  graphXPos + CONTROL_DESCRIPTION_WIDTH + INPUT_TEXTBOX_WIDTH + UNITS_AFTER_TEXTBOX_SPACING + STATIC_UNITS_WIDTH + GENERIC_SPACING;

	CreateWindow(WC_STATIC, TEXT("To:"), WS_VISIBLE | WS_CHILD,
		toFrequencyBaseXPos, chooseFrequenciesYPos, CONTROL_DESCRIPTION_WIDTH, STATIC_TEXT_HEIGHT, mainWindowHandle, NULL, NULL, NULL);

	fileEditor.toFreqTextbox = CreateWindow(WC_EDIT, TEXT(""), WS_VISIBLE | WS_CHILD | WS_BORDER | ES_CENTER,
		toFrequencyBaseXPos + CONTROL_DESCRIPTION_WIDTH, chooseFrequenciesYPos - 2, INPUT_TEXTBOX_WIDTH, INPUT_TEXTBOX_HEIGHT, mainWindowHandle, NULL, NULL, NULL);
	SetWindowSubclass(fileEditor.toFreqTextbox, FloatTextboxWindowProc, 0, 0); // Setting textbox to only accept float numbers.
	SendMessage(fileEditor.toFreqTextbox, EM_SETLIMITTEXT, (WPARAM)INPUT_TEXTBOX_CHARACTER_LIMIT, 0); // Setting character limit.

	CreateWindow(WC_STATIC, TEXT("Hz"), WS_VISIBLE | WS_CHILD, toFrequencyBaseXPos + CONTROL_DESCRIPTION_WIDTH + INPUT_TEXTBOX_WIDTH + UNITS_AFTER_TEXTBOX_SPACING,
		chooseFrequenciesYPos, STATIC_UNITS_WIDTH, STATIC_TEXT_HEIGHT, mainWindowHandle, NULL, NULL, NULL);

	// Adding GUI for choosing what change to apply.
	// Adding a dropdown for choosing the change type.
	unsigned int chooseChangeYPos = chooseFrequenciesYPos + INPUTS_Y_SPACING;

	CreateWindow(WC_STATIC, TEXT("Change:"), WS_VISIBLE | WS_CHILD,
		graphXPos, chooseChangeYPos, CONTROL_DESCRIPTION_WIDTH, STATIC_TEXT_HEIGHT, mainWindowHandle, NULL, NULL, NULL);

	fileEditor.changeTypeDropdown = CreateWindow(WC_COMBOBOX, TEXT("Multiply"), CBS_DROPDOWNLIST | WS_VISIBLE | WS_CHILD | CBS_HASSTRINGS,
		graphXPos + CONTROL_DESCRIPTION_WIDTH, chooseChangeYPos - 5, INPUT_TEXTBOX_WIDTH, 100, mainWindowHandle, NULL, NULL, NULL);
	SendMessage(fileEditor.changeTypeDropdown, CB_ADDSTRING, 0, (LPARAM)TEXT("Multiply"));
	SendMessage(fileEditor.changeTypeDropdown, CB_ADDSTRING, 0, (LPARAM)TEXT("Add"));
	SendMessage(fileEditor.changeTypeDropdown, CB_ADDSTRING, 0, (LPARAM)TEXT("Subtract"));
	SendMessage(fileEditor.changeTypeDropdown, CB_SETCURSEL, 0, 0); // Setting "multiply" as the default selection.

	// Adding a textbox for choosing the change amount.
	CreateWindow(WC_STATIC, TEXT("Amount:"), WS_VISIBLE | WS_CHILD,
		toFrequencyBaseXPos, chooseChangeYPos, CONTROL_DESCRIPTION_WIDTH, STATIC_TEXT_HEIGHT, mainWindowHandle, NULL, NULL, NULL);

	fileEditor.changeAmountTextbox = CreateWindow(WC_EDIT, TEXT("0.000"), WS_VISIBLE | WS_CHILD | WS_BORDER | ES_CENTER, 
		toFrequencyBaseXPos + CONTROL_DESCRIPTION_WIDTH, chooseChangeYPos - 2, INPUT_TEXTBOX_WIDTH, INPUT_TEXTBOX_HEIGHT, mainWindowHandle, NULL, NULL, NULL);
	SetWindowSubclass(fileEditor.changeAmountTextbox, FloatTextboxWindowProc, 0, 0); // Setting textbox to only accept float numbers.
	SendMessage(fileEditor.changeAmountTextbox, EM_SETLIMITTEXT, (WPARAM)INPUT_TEXTBOX_CHARACTER_LIMIT, 0); // Setting character limit.

	// Adding GUI for choosing how much smoothing to apply.
	unsigned int chooseSmoothingYPos = chooseChangeYPos + INPUTS_Y_SPACING;

	CreateWindow(WC_STATIC, TEXT("Smoothing:"), WS_VISIBLE | WS_CHILD,
		graphXPos, chooseSmoothingYPos, CONTROL_DESCRIPTION_WIDTH, STATIC_TEXT_HEIGHT, mainWindowHandle, NULL, NULL, NULL);

	AddTrackbarWithTextbox(mainWindowHandle, &(fileEditor.smoothingTrackbar), &(fileEditor.smoothingTextbox), graphXPos + CONTROL_DESCRIPTION_WIDTH, chooseSmoothingYPos,
						   MIN_SMOOTHING, MAX_SMOOTHING, DEFAULT_SMOOTHING, SMOOTHING_TRACKBAR_LINESIZE, SMOOTHING_TRACKBAR_PAGESIZE, TEXT("0.") TXStringify(DEFAULT_SMOOTHING), NULL, FALSE); // TODO: change default to 1 (I think)

	// Adding buttons for undo, redo, apply.
	fileEditor.undoButton = CreateWindow(WC_BUTTON, TEXT("Undo"), WS_VISIBLE | WS_CHILD | BS_CENTER | BS_VCENTER | WS_DISABLED,
		graphXPos + GRAPH_WIDTH - BUTTON_WIDTH, chooseFrequenciesYPos, BUTTON_WIDTH, BUTTON_HEIGHT, mainWindowHandle, (HMENU)EDIT_ACTION_UNDO, NULL, NULL);

	fileEditor.redoButton = CreateWindow(WC_BUTTON, TEXT("Redo"), WS_VISIBLE | WS_CHILD | BS_CENTER | BS_VCENTER | WS_DISABLED,
		graphXPos + GRAPH_WIDTH - BUTTON_WIDTH, chooseFrequenciesYPos + BUTTON_HEIGHT + GENERIC_SPACING, BUTTON_WIDTH, BUTTON_HEIGHT, mainWindowHandle, (HMENU)EDIT_ACTION_REDO, NULL, NULL);

	CreateWindow(WC_BUTTON, TEXT("Apply"), WS_VISIBLE | WS_CHILD | BS_CENTER | BS_VCENTER,
		graphXPos + GRAPH_WIDTH - BUTTON_WIDTH, chooseFrequenciesYPos + 2 * (BUTTON_HEIGHT + GENERIC_SPACING), BUTTON_WIDTH, BUTTON_HEIGHT, mainWindowHandle, (HMENU)EDIT_ACTION_APPLY, NULL, NULL);

	// Un-graying menu options that will always be usable from now on.
	HMENU mainMenu = GetMenu(mainWindowHandle);
	EnableMenuItem(mainMenu, NOTIF_CODIFY(FILE_ACTION_SAVE), MF_BYCOMMAND | MF_ENABLED);
	EnableMenuItem(mainMenu, NOTIF_CODIFY(FILE_ACTION_SAVEAS), MF_BYCOMMAND | MF_ENABLED);
	EnableMenuItem(mainMenu, NOTIF_CODIFY(EDIT_ACTION_APPLY), MF_BYCOMMAND | MF_ENABLED);
}

void PaintFileEditorTemporaries()
{
	// Filling the tab control with the names of the channels this file has instead of the old one.
	TabCtrl_DeleteAllItems(fileEditor.channelTabs);

	TCITEM tab;
	tab.mask = TCIF_PARAM | TCIF_TEXT;

	// This fills in the buffer with channel names, and returns how many channels were filled in.
	TCHAR channelNames[MAX_NAMED_CHANNELS][CHANNEL_NAME_BUFFER_LEN];
	unsigned short numOfChannels = GetChannelNames(fileEditor.fileInfo, channelNames);

	for (unsigned short i = 0; i < numOfChannels; i++)
	{
		tab.pszText = channelNames[i];
		TabCtrl_InsertItem(fileEditor.channelTabs, i, &tab);
	}

	SetWindowPos(fileEditor.channelTabs, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE); // Editing the tab control brings it to front. This fixes that.
	UpdateWindowTitle();
	PlotAndDisplayChannelGraphs(0);
}

void ResetFileEditorPermanents()
{
	// Resetting all the input controls to their default values.
	SendMessage(fileEditor.fromFreqTextbox, WM_SETTEXT, 0, (LPARAM)TEXT(""));
	SendMessage(fileEditor.toFreqTextbox, WM_SETTEXT, 0, (LPARAM)TEXT(""));
	SendMessage(fileEditor.changeTypeDropdown, CB_SETCURSEL, 0, 0);
	SendMessage(fileEditor.changeAmountTextbox, WM_SETTEXT, 0, (LPARAM)TEXT("0.000"));
	SendMessage(fileEditor.smoothingTextbox, WM_SETTEXT, 0, (LPARAM)(TEXT("0.") TXStringify(DEFAULT_SMOOTHING)));
	SendMessage(fileEditor.smoothingTrackbar, TBM_SETPOS, (WPARAM)TRUE, (LPARAM)DEFAULT_SMOOTHING); // This should fire a sync operation which will set the textbox accordingly.
}

void SetChannelDomain(unsigned short channel, FunctionDomain domain)
{
	if (fileEditor.channelsDomain[channel] != domain)
	{
		if (domain == FREQUENCY_DOMAIN)
		{
			RealInterleavedFFT(fileEditor.channelsData[channel]);
		}
		else
		{
			InverseRealInterleavedFFT(fileEditor.channelsData[channel]);
		}

		fileEditor.channelsDomain[channel] = domain;
	}
}

void SetAllChannelsDomain(FunctionDomain domain)
{
	unsigned short relevantChannels = GetRelevantChannelsCount(fileEditor.fileInfo);

	for (unsigned short i = 0; i < relevantChannels; i++)
	{
		SetChannelDomain(i, domain);
	}
}

void PlotChannelWaveform(unsigned short channel)
{
	// TODO: this is an old naive plotting algorithm. It might still come in handy for plotting waveforms with very few samples.
	//Function_##precision##Complex func = *((Function_##precision##Complex*)fileEditor.channelsData[channel]);													\
	precision##Real lengthReal = length;																														\
	precision##Real halfHeight = GRAPH_HEIGHT / 2;																												\
																																								\
	for (unsigned long long i = 0; i < length; i++)																												\
	{																																							\
		/* Extracting the real sample from this complex function.*/																								\
		precision##Real sample = *(CAST(&get(func, i / 2), precision##Real*) + (i % 2));																		\
																																								\
		/* Setting the pixel that corresponds to this sample's position on the graph.*/																			\
		unsigned int xCoord = i * (GRAPH_WIDTH / lengthReal); /* TODO: consider different rounding techniques. Also clamp the pixels*/							\
		unsigned int yCoord = halfHeight - (halfHeight * sample);																								\
		SetPixel(fileEditor.graphingDC, xCoord, yCoord, WHITE_COLOR);																							\
	}
	
	// This macro contains the contents of this function that depend on the float precision. It needs to be declared at the start before it is used.
	// TODO: potentially use a different plotting algorithm when working with a small number of samples. Maybe make it look similar to audacity's high zoom appearance.
	#define PLOT_CHANNEL_WAVEFORM_TYPED(precision)																												\
	Function_##precision##Real func = ReadComplexFunctionAsReal_##precision##Complex(fileEditor.channelsData[channel]);											\
	precision##Real halfHeight = (GRAPH_HEIGHT - 1) / 2.0;																										\
																																								\
	/* For every x pixel, we find the min and max values from the set of samples that map to this pixel, and draw a line between them.*/						\
	for (unsigned int i = 0; i < GRAPH_WIDTH; i++)																												\
	{																																							\
		/* Calculating the indices of the first (inclusive) and last (exclusive) samples which map to this x pixel.*/											\
		unsigned long long startSample = ceil_DoubleReal(stepSize * i);																							\
		unsigned long long endSample = ClampInt(ceil_DoubleReal(stepSize * (i + 1)), 0, length);																\
																																								\
		/* Finding the min and max values of all samples in the range [startSample, endSample).*/																\
		precision##Real min = GetMin_##precision##Real(func, startSample, endSample, 3);																			\
		precision##Real max = GetMax_##precision##Real(func, startSample, endSample, 3);																			\
																																								\
		/* Calculating the y pixel that the min and max samples map to, and drawing a line between them.*/														\
		unsigned int minYCoord = ClampInt(halfHeight - (halfHeight * min), 0, GRAPH_HEIGHT - 1);																\
		unsigned int maxYCoord = ClampInt(halfHeight - (halfHeight * max), 0, GRAPH_HEIGHT - 1);																\
		MoveToEx(fileEditor.graphingDC, i, maxYCoord, NULL);																									\
		LineTo(fileEditor.graphingDC, i, minYCoord + 1); /* LineTo doesn't color the last pixel which is why we add +1.*/										\
	}

	SetChannelDomain(channel, TIME_DOMAIN);

	if (fileEditor.waveformGraphs[channel] == NULL)
	{
		fileEditor.waveformGraphs[channel] = CreateBitmap(GRAPH_WIDTH, GRAPH_HEIGHT, 1, 1, NULL);
	}
	
	// SelectObject returns a handle to the object that was selected before the call. Windows says that you must restore the old selected object after you're done painting.
	HBITMAP oldSelectedObj = SelectObject(fileEditor.graphingDC, fileEditor.waveformGraphs[channel]);

	// Filling the bitmap with white color.
	RECT bitmapDimensions;
	bitmapDimensions.left = 0;
	bitmapDimensions.top = 0;
	bitmapDimensions.right = GRAPH_WIDTH;
	bitmapDimensions.bottom = GRAPH_HEIGHT;
	FillRect(fileEditor.graphingDC, &bitmapDimensions, GetStockObject(WHITE_BRUSH));

	// For every sample in the function that isn't zero-padding, setting the pixel it corresponds to. Length is doubled because we'll be reading a complex sequence as if it's a real sequence of twice the length.
	unsigned long long length = fileEditor.fileInfo->sampleLength;
	DoubleReal stepSize = length / ((DoubleReal)GRAPH_WIDTH); // Always double precision because it's used for sample indices, not sample values.

	// From this point on we need to know if the function uses single or double precision. Either way it's gonna be in complex interleaved form, more on that in the documentation of RealInterleavedFFT.
	if (GetType(fileEditor.channelsData[channel]) == FloatComplexType)
	{
		PLOT_CHANNEL_WAVEFORM_TYPED(Float)
	}
	else
	{
		PLOT_CHANNEL_WAVEFORM_TYPED(Double)
	}

	SelectObject(fileEditor.graphingDC, oldSelectedObj);
}

void PlotChannelFourier(unsigned short channel)
{	
	// TODO: Investigate different step sizes for min and max further and performances and determine the best course of action.
	// This macro contains the contents of this function that depend on the float precision. It needs to be declared at the start before it is used.
	#define PLOT_CHANNEL_FOURIER_TYPED(precision)																												\
	Function_##precision##Complex func = *((Function_##precision##Complex*)fileEditor.channelsData[channel]);													\
																																								\
	/* We'll be plotting the graph such that the highest pixel represents the global maximum point.*/															\
	precision##Real globalMax = cabs_##precision##Complex(GetMax_##precision##Complex(func, 0, length, 3)); 													\
																																								\
	/* Clamping the value so it isn't smaller than the decibel reference, which is the smallest number that we plot while globalMax is the highest.*/			\
	globalMax = Clamp##precision(globalMax, FOURIER_DECIBEL_REFERENCE, MAX_##precision##Real);																	\
																																								\
	/* Converting to logarithmic scale, and add a little so the graph peak isn't exactly on the last pixel.*/													\
	globalMax = LinearToDecibel##precision##Real(globalMax, FOURIER_DECIBEL_REFERENCE) + CAST(1.5, precision##Real); 											\
																																								\
	/* This is the slope of the linear function which maps decibels to y pixels.*/																				\
	precision##Real yMappingSlope = GRAPH_HEIGHT / globalMax; 																									\
																																								\
	/* For every x pixel, we find the min and max values from the set of samples that map to this pixel, and draw a line between them.*/						\
	for (unsigned int i = 0; i < GRAPH_WIDTH; i++)																												\
	{																																							\
		/* Calculating the indices of the first (inclusive) and last (exclusive) samples which map to this x pixel.*/											\
		unsigned long long startSample = ceil_DoubleReal(stepSize * i);																							\
		unsigned long long endSample = ClampInt(ceil_DoubleReal(stepSize * (i + 1)), 0, length);																\
																																								\
		/* Finding the min and max values of all samples in the range [startSample, endSample).*/																\
		precision##Real max = cabs_##precision##Complex(GetMax_##precision##Complex(func, startSample, endSample, 3));											\
																																								\
		/* Converting to logarithmic scale. Had a bug with the integer conversion when this returns -INF, so I only proceed if max is above the reference.*/	\
		max = max < FOURIER_DECIBEL_REFERENCE ? 0.0 : LinearToDecibel##precision##Real(max, FOURIER_DECIBEL_REFERENCE); 										\
																																								\
		/* Calculating the y pixel that the min and max samples map to, and drawing a line between them.*/														\
		unsigned int yCoord = ClampInt((GRAPH_HEIGHT - 1) - (yMappingSlope * max), -1, GRAPH_HEIGHT - 1);														\
		MoveToEx(fileEditor.graphingDC, i, GRAPH_HEIGHT - 1, NULL);																								\
		LineTo(fileEditor.graphingDC, i, yCoord);																												\
	}

	SetChannelDomain(channel, FREQUENCY_DOMAIN);

	if (fileEditor.fourierGraphs[channel] == NULL)
	{
		fileEditor.fourierGraphs[channel] = CreateBitmap(GRAPH_WIDTH, GRAPH_HEIGHT, 1, 1, NULL);
	}

	// SelectObject returns a handle to the object that was selected before the call. Windows says that you must restore the old selected object after you're done painting.
	HBITMAP oldSelectedObj = SelectObject(fileEditor.graphingDC, fileEditor.fourierGraphs[channel]);

	// Filling the bitmap with white color.
	RECT bitmapDimensions;
	bitmapDimensions.left = 0;
	bitmapDimensions.top = 0;
	bitmapDimensions.right = GRAPH_WIDTH;
	bitmapDimensions.bottom = GRAPH_HEIGHT;
	FillRect(fileEditor.graphingDC, &bitmapDimensions, GetStockObject(WHITE_BRUSH));

	// For every sample in the function that isn't zero-padding, setting the pixel it corresponds to. Length is doubled because we'll be reading a complex sequence as if it's a real sequence of twice the length.
	unsigned long long length = NumOfSamples(fileEditor.channelsData[channel]);
	DoubleReal stepSize = length / ((DoubleReal)GRAPH_WIDTH); // Always double precision because it's used for sample indices, not sample values.

	// From this point on we need to know if the function uses single or double precision. Either way it's gonna be in complex interleaved form, more on that in the documentation of RealInterleavedFFT.
	if (GetType(fileEditor.channelsData[channel]) == FloatComplexType)
	{
		PLOT_CHANNEL_FOURIER_TYPED(Float)
	}
	else
	{
		PLOT_CHANNEL_FOURIER_TYPED(Double)
	}

	SelectObject(fileEditor.graphingDC, oldSelectedObj);
}

void PlotChannelGraphs(unsigned short channel)
{
	// We check what the function's current domain is so we know which order is most efficient for doing this.
	if (fileEditor.channelsDomain[channel] == TIME_DOMAIN)
	{
		PlotChannelWaveform(channel);
		PlotChannelFourier(channel);
	}
	else
	{
		PlotChannelFourier(channel);
		PlotChannelWaveform(channel);
	}
}

void DisplayChannelWaveform(unsigned short channel)
{
	if (fileEditor.waveformGraphs[channel] == NULL)
	{
		fprintf(stderr, "Tried to display the waveform of channel %hu but it hasn't been plotted yet.\n", channel);
		return;
	}

	// TODO: this commented code successfully paints the graphs with color, but it uses beginpaint outside of the WM_PAINT message handler which is forbidden. I need to figure out how to make this work without beginpaint.
	// PAINTSTRUCT ps;
	// HDC tmpDC;
	// HBITMAP oldBitmap;

	// BeginPaint(mainWindowHandle, &ps);
	// tmpDC = CreateCompatibleDC(ps.hdc);
	// oldBitmap = (HBITMAP) SelectObject(tmpDC, fileEditor.waveformGraphs[channel]);
	// SetBkColor(ps.hdc, RGB(255, 128, 0));
	// SetTextColor(ps.hdc, RGB(0, 128, 255));
	// BitBlt(ps.hdc, 50, 60, GRAPH_WIDTH, GRAPH_HEIGHT,
	// tmpDC, 0, 0, SRCCOPY);
	// SelectObject(tmpDC, oldBitmap);
	// DeleteDC(tmpDC);
	// EndPaint(mainWindowHandle, &ps);
	
    SendMessage(fileEditor.waveformGraphStatic, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)fileEditor.waveformGraphs[channel]);
}

void DisplayChannelFourier(unsigned short channel)
{
	if (fileEditor.fourierGraphs[channel] == NULL)
	{
		fprintf(stderr, "Tried to display the fourier transform of channel %hu but it hasn't been plotted yet.\n", channel);
		return;
	}

	SendMessage(fileEditor.fourierGraphStatic, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)fileEditor.fourierGraphs[channel]);
}

void DisplayChannelGraphs(unsigned short channel)
{
	DisplayChannelWaveform(channel);
	DisplayChannelFourier(channel);
}

void PlotAndDisplayChannelGraphs(unsigned short channel)
{
	PlotChannelGraphs(channel);
	DisplayChannelGraphs(channel);
}

void CloseFileEditor()
{
	DeallocateChannelsData();
	DeallocateChannelsGraphs();
	DeallocateModificationStack(fileEditor.modificationStack);
	CloseWaveFile(fileEditor.fileInfo);
	free(fileEditor.channelsDomain);

	fileEditor.fileInfo = NULL;
	fileEditor.channelsData = NULL;
	fileEditor.channelsDomain = NULL;
	fileEditor.waveformGraphs = NULL;
	fileEditor.fourierGraphs = NULL;
	fileEditor.modificationStack = NULL;
	fileEditor.currentSaveState = NULL;

	UpdateUndoRedoState();
}

void DeallocateChannelsData()
{
	if (fileEditor.channelsData != NULL)
	{
		unsigned short relevantChannels = GetRelevantChannelsCount(fileEditor.fileInfo);

		for (unsigned short i = 0; i < relevantChannels; i++)
		{
			DeallocateFunctionInternals(fileEditor.channelsData[i]);
			free(fileEditor.channelsData[i]);
		}

		free(fileEditor.channelsData);
	}
}

void DeallocateChannelsGraphs()
{
	// Dodging a segfault with GetRelevantChannelsCount.
	if (fileEditor.fileInfo == NULL)
	{
		return;
	}

	unsigned short relevantChannels = GetRelevantChannelsCount(fileEditor.fileInfo);

	if (fileEditor.waveformGraphs != NULL)
	{
		for (unsigned short i = 0; i < relevantChannels; i++)
		{
			if (fileEditor.waveformGraphs[i] != NULL)
			{
				DeleteObject(fileEditor.waveformGraphs[i]);
			}
		}

		free(fileEditor.waveformGraphs);
	}

	if (fileEditor.fourierGraphs != NULL)
	{
		for (unsigned short i = 0; i < relevantChannels; i++)
		{
			if (fileEditor.fourierGraphs[i] != NULL)
			{
				DeleteObject(fileEditor.fourierGraphs[i]);
			}
		}

		free(fileEditor.fourierGraphs);
	}
}

void UpdateWindowTitle()
{
	if (fileEditor.fileInfo == NULL || IsFileNew(fileEditor.fileInfo))
	{
		// When there are unsaved changes, an * is appended to indicate it.
		SetWindowText(mainWindowHandle, HasUnsavedChanges() ? TEXT("Untitled*") TITLE_POSTFIX : TEXT("Untitled") TITLE_POSTFIX);
	}
	else
	{
		// Extracting the file name from the full path, and appending " - Fourier".
		// I decided not to impose a length limit because I fear cutting a unicode string in the middle might ruin it. Worst comes to worst, users get a long ass string at the top of the screen.
		unsigned int pathLen = _tcslen(fileEditor.fileInfo->path);
		unsigned int bufLen = pathLen + _tcslen(TITLE_POSTFIX) + 2; // Buffer must be large enough to hold the path name, an optional asterisk, the postfix, and the null terminator.
		TCHAR pathCopy[bufLen];
		_tcscpy_s(pathCopy, pathLen + 1, fileEditor.fileInfo->path);
		PathStripPath(pathCopy);

		// Appending an * to indicate unsaved changes.
		if (HasUnsavedChanges())
		{
			_tcscat_s(pathCopy, pathLen + 2, TEXT("*"));
		}

		_tcscat_s(pathCopy, bufLen, TITLE_POSTFIX);
		SetWindowText(mainWindowHandle, pathCopy);
	}
}

void UpdateUndoRedoState()
{
	char enableRedo = CanRedo(fileEditor.modificationStack);
	char enableUndo = CanUndo(fileEditor.modificationStack);

	HMENU menu = GetMenu(mainWindowHandle);
	EnableMenuItem(menu, NOTIF_CODIFY(EDIT_ACTION_REDO), enableRedo ? MF_ENABLED : MF_GRAYED);
	EnableMenuItem(menu, NOTIF_CODIFY(EDIT_ACTION_UNDO), enableUndo ? MF_ENABLED : MF_GRAYED);

	if (IsEditorOpen())
	{
		EnableWindow(fileEditor.redoButton, enableRedo);
		EnableWindow(fileEditor.undoButton, enableUndo);
	}
}

char IsEditorOpen()
{
	// We could've checked any other permanent editor control in here, but the tabs seems appropriate.
	return fileEditor.channelTabs != NULL;
}

char HasUnsavedChanges()
{
	// TODO: if the current save state was freed, it's possible something else was allocated in its stead (however unlikely) which would cause this to not fire even though we want it to.
	return fileEditor.modificationStack != NULL && fileEditor.modificationStack != fileEditor.currentSaveState;
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
		case WM_HSCROLL:;
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
	AddTrackbarWithTextbox(windowHandle, &(newFileOptionsHandles->lengthTrackbar), &(newFileOptionsHandles->lengthTextbox), CHOOSE_FILE_LENGTH_X_POS, CHOOSE_FILE_LENGTH_Y_POS,
						   FILE_MIN_LENGTH, FILE_MAX_LENGTH, NEW_FILE_DEFAULT_LENGTH, LENGTH_TRACKBAR_LINESIZE, LENGTH_TRACKBAR_PAGESIZE, TXStringify(NEW_FILE_DEFAULT_LENGTH), TEXT("sec"), TRUE);

	// Adding trackbar-textbox-units triple for selecting file sample rate.
	AddTrackbarWithTextbox(windowHandle, &(newFileOptionsHandles->frequencyTrackbar), &(newFileOptionsHandles->frequencyTextbox), CHOOSE_FILE_LENGTH_X_POS, CHOOSE_FILE_LENGTH_Y_POS + INPUTS_Y_SPACING,
						   FILE_MIN_FREQUENCY, FILE_MAX_FREQUENCY, NEW_FILE_DEFAULT_FREQUENCY, FREQUENCY_TRACKBAR_LINESIZE, FREQUENCY_TRACKBAR_PAGESIZE, TXStringify(NEW_FILE_DEFAULT_FREQUENCY), TEXT("Hz"), TRUE);

	// Adding a radio menu for choosing bit depth. Important that we add these such that the i'th cell indicates i+1 bytes.
	unsigned int baseRadiosXPos = CHOOSE_FILE_LENGTH_X_POS + 8;
	unsigned int radiosYPos = CHOOSE_FILE_LENGTH_Y_POS + (2 * INPUTS_Y_SPACING);

	newFileOptionsHandles->depthOptions[0] = CreateWindow(WC_BUTTON, TEXT("8-bit"), WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON | BS_VCENTER | WS_GROUP,
		baseRadiosXPos, radiosYPos, RADIO_WIDTH, STATIC_TEXT_HEIGHT, windowHandle, NULL, NULL, NULL);

	newFileOptionsHandles->depthOptions[1] = CreateWindow(WC_BUTTON, TEXT("16-bit"), WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON | BS_VCENTER,
		baseRadiosXPos + RADIO_WIDTH, radiosYPos, RADIO_WIDTH, STATIC_TEXT_HEIGHT, windowHandle, NULL, NULL, NULL);

	newFileOptionsHandles->depthOptions[2] = CreateWindow(WC_BUTTON, TEXT("24-bit"), WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON | BS_VCENTER,
		baseRadiosXPos + (2 * RADIO_WIDTH), radiosYPos, RADIO_WIDTH, STATIC_TEXT_HEIGHT, windowHandle, NULL, NULL, NULL);

	newFileOptionsHandles->depthOptions[3] = CreateWindow(WC_BUTTON, TEXT("32-bit"), WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON | BS_VCENTER,
		baseRadiosXPos + (3 * RADIO_WIDTH), radiosYPos, RADIO_WIDTH, STATIC_TEXT_HEIGHT, windowHandle, NULL, NULL, NULL);

	SendMessage(newFileOptionsHandles->depthOptions[1], BM_SETCHECK, BST_CHECKED, 0); // Setting default selection for this menu.

	// Adding buttons for ok and cancel.
	unsigned int buttonsBaseXPos = (NEW_FILE_OPTIONS_WIDTH - (2 * BUTTON_WIDTH) - GENERIC_SPACING) / 2;
	unsigned int buttonsYPos = NEW_FILE_OPTIONS_HEIGHT - 2 * (BUTTON_HEIGHT);

	CreateWindow(WC_BUTTON, TEXT("Ok"), WS_VISIBLE | WS_CHILD | BS_CENTER | BS_VCENTER,
		buttonsBaseXPos, buttonsYPos, BUTTON_WIDTH, BUTTON_HEIGHT, windowHandle, (HMENU)NEW_FILE_OPTIONS_OK, NULL, NULL);

	CreateWindow(WC_BUTTON, TEXT("Cancel"), WS_VISIBLE | WS_CHILD | BS_CENTER | BS_VCENTER,
		buttonsBaseXPos + BUTTON_WIDTH + GENERIC_SPACING, buttonsYPos, BUTTON_WIDTH, BUTTON_HEIGHT, windowHandle, (HMENU)NEW_FILE_OPTIONS_CANCEL, NULL, NULL);
}

void CloseNewFileOptions(HWND windowHandle)
{
	// Enabling the parent window and destroying this one. The ordering is important.
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
		FileInfo* fileInfo;

		// Proceeding with creating a new file only if the user didn't choose to abort.
		CreateNewFile(&fileInfo, length, frequency, byteDepth);
		InitializeFileEditor(windowHandle, fileInfo);
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
	CreateWindow(WC_STATIC, TEXT("Create a new file or open an existing one?"), WS_VISIBLE | WS_CHILD | SS_CENTER,
		0, 15, SELECT_FILE_OPTION_WIDTH, STATIC_TEXT_HEIGHT, windowHandle, NULL, NULL, NULL);

	// Adding buttons for new file and open file.
	unsigned int buttonsBaseXPos = (SELECT_FILE_OPTION_WIDTH - (2 * BUTTON_WIDTH) - GENERIC_SPACING) / 2;
	unsigned int buttonsYPos = SELECT_FILE_OPTION_HEIGHT - 2 * (BUTTON_HEIGHT);

	CreateWindow(WC_BUTTON, TEXT("New file"), WS_VISIBLE | WS_CHILD | BS_CENTER | BS_VCENTER,
		buttonsBaseXPos, buttonsYPos, BUTTON_WIDTH, BUTTON_HEIGHT, windowHandle, (HMENU)FILE_ACTION_NEW, NULL, NULL);

	CreateWindow(WC_BUTTON, TEXT("Open file"), WS_VISIBLE | WS_CHILD | BS_CENTER | BS_VCENTER,
		buttonsBaseXPos + BUTTON_WIDTH + GENERIC_SPACING, buttonsYPos, BUTTON_WIDTH, BUTTON_HEIGHT, windowHandle, (HMENU)FILE_ACTION_OPEN, NULL, NULL);
}

void CloseSelectFileOption(HWND windowHandle)
{
	// Enabling the parent window and destroying this one.
	HWND parent = GetWindow(windowHandle, GW_OWNER);
	EnableWindow(parent, TRUE); // Important to enable the parent before destroying the child.
	SetForegroundWindow(parent); // Sometimes the parent window wasn't given the focus again when you close this window.
	DestroyWindow(windowHandle);
}

void ProcessSelectFileOptionCommand(HWND windowHandle, WPARAM wparam, LPARAM lparam)
{
	switch (HIWORD(wparam))
	{
		case BN_CLICKED:
			switch (LOWORD(wparam))
			{
				case FILE_ACTION_NEW:
					FileNew(windowHandle);
					break;
				case FILE_ACTION_OPEN:
					FileOpen(windowHandle);

					// If the FileOpen operation was a success, closing this menu.
					if (fileEditor.fileInfo != NULL)
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

void AddTrackbarWithTextbox(HWND windowHandle, HWND* trackbar, HWND* textbox, int xPos, int yPos, int minValue, int maxValue, int defaultValue, int linesize, int pagesize, LPCTSTR defaultValueStr, LPCTSTR units, char naturalsOnly)
{
	// Calculating tick length given the interval length and how many ticks we want to have. Rounding it up instead of down because I would rather have too few ticks than too many.
	WPARAM tickLength = DivCeilInt(maxValue - minValue, TRACKBAR_TICKS);

	// Adding trackbar.
	*trackbar = CreateWindow(TRACKBAR_CLASS, NULL, WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS, xPos, yPos - 6, TRACKBAR_WIDTH, TRACKBAR_HEIGHT, windowHandle, 0, NULL, NULL);
	SendMessage(*trackbar, TBM_SETRANGEMIN, (WPARAM)FALSE, (LPARAM)minValue); // Configuring min range of trackbar.
	SendMessage(*trackbar, TBM_SETRANGEMAX, (WPARAM)TRUE, (LPARAM)maxValue);  // Configuring max range of trackbar.
	SendMessage(*trackbar, TBM_SETPOS, (WPARAM)TRUE, (LPARAM)defaultValue);	  // Configuring default selection.
	SendMessage(*trackbar, TBM_SETLINESIZE, 0, (LPARAM)linesize);			  // Configuring sensitivity for arrow key inputs.
	SendMessage(*trackbar, TBM_SETPAGESIZE, 0, (LPARAM)pagesize);			  // Configuring sensitivity for PGUP/PGDOWN inputs.
	SendMessage(*trackbar, TBM_SETTICFREQ, tickLength, 0);					  // Configuring how many ticks are on it.

	// Adding textbox.
	*textbox = CreateWindow(WC_EDIT, defaultValueStr, WS_VISIBLE | WS_CHILD | WS_BORDER | ES_CENTER, xPos + TRACKBAR_WIDTH + GENERIC_SPACING, yPos - 2, INPUT_TEXTBOX_WIDTH, INPUT_TEXTBOX_HEIGHT, windowHandle, NULL, NULL, NULL);
	SendMessage(*textbox, EM_SETLIMITTEXT, (WPARAM)INPUT_TEXTBOX_CHARACTER_LIMIT, 0); // Setting character limit.

	if (naturalsOnly)
	{
		SetWindowLongPtr(*textbox, GWL_STYLE, GetWindowLongPtr(*textbox, GWL_STYLE) | ES_NUMBER); // Setting textbox to only accept natural numbers.
	}
	else
	{
		SetWindowSubclass(*textbox, FloatTextboxWindowProc, 0, 0); // Setting textbox to only accept positive float numbers.
	}

	// Adding text which says what units the content is in, unless no units were given.
	if (units != NULL)
	{
		CreateWindow(WC_STATIC, units, WS_VISIBLE | WS_CHILD, xPos + TRACKBAR_WIDTH + GENERIC_SPACING + INPUT_TEXTBOX_WIDTH + UNITS_AFTER_TEXTBOX_SPACING, yPos, STATIC_UNITS_WIDTH, STATIC_TEXT_HEIGHT, windowHandle, NULL, NULL, NULL);
	}
}

void SyncTextboxToTrackbar(HWND trackbar, HWND textbox)
{
	LRESULT val = (int)SendMessage(trackbar, TBM_GETPOS, 0, 0);
	TCHAR buffer[16];
	_ltot(val, buffer, 10);
	SendMessage(textbox, WM_SETTEXT, 0, (LPARAM)buffer);
}

void SyncTrackbarToTextbox(HWND trackbar, HWND textbox)
{
	TCHAR buffer[16];
	TCHAR* endChar;
	LRESULT length = SendMessage(textbox, WM_GETTEXT, 16, (LPARAM)buffer);
	long val = _tcstol(buffer, &endChar, 10);

	// Assigning value to trackbar if the conversion was successful. endChar points to the null terminator iff the conversion is successful or the string is empty.
	if (length != 0 && endChar == &(buffer[length]))
	{
		long min = SendMessage(trackbar, TBM_GETRANGEMIN, 0, 0);
		long max = SendMessage(trackbar, TBM_GETRANGEMAX, 0, 0);
		val = ClampInt(val, min, max);
		SendMessage(trackbar, TBM_SETPOS, (WPARAM)TRUE, (LPARAM)val);
	}
}

void SyncTextboxToTrackbarFloat(HWND trackbar, HWND textbox)
{
	long min = SendMessage(trackbar, TBM_GETRANGEMIN, 0, 0);
	long max = SendMessage(trackbar, TBM_GETRANGEMAX, 0, 0);
	double val = (((double)SendMessage(trackbar, TBM_GETPOS, 0, 0)) - min) / (max - min); // It's assumed that we want to bring the number to the [0, 1] range.

	TCHAR buffer[16];
	_stprintf_s(buffer, 16, TEXT("%.3f"), val);
	SendMessage(textbox, WM_SETTEXT, 0, (LPARAM)buffer);
}

void SyncTrackbarToTextboxFloat(HWND trackbar, HWND textbox)
{
	TCHAR buffer[16];
	TCHAR* endChar;
	LRESULT length = SendMessage(textbox, WM_GETTEXT, 16, (LPARAM)buffer);
	double val = _tcstod(buffer, &endChar);

	// Assigning value to trackbar if the conversion was successful. endChar points to the iff the conversion is successful.
	if (length != 0 && endChar == &(buffer[length]))
	{
		long min = SendMessage(trackbar, TBM_GETRANGEMIN, 0, 0);
		long max = SendMessage(trackbar, TBM_GETRANGEMAX, 0, 0);
		val = ClampDouble(min + (val * (max - min)), min, max); // Scaling val and clamping it. It's assumed that val is in [0,1] and we want to scale it to [min, max].
		SendMessage(trackbar, TBM_SETPOS, (WPARAM)TRUE, (long)val);
	}
}

LRESULT CALLBACK FloatTextboxWindowProc(HWND windowHandle, UINT msg, WPARAM wparam, LPARAM lparam, UINT_PTR subclassId, DWORD_PTR refData)
{
	switch (msg)
	{
		case WM_CHAR:
			// If the character isn't a digit or a dot, rejecting it.
			if (!(('0' <= wparam && wparam <= '9') || wparam == '.' ||
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