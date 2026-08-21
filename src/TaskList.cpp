#include <windows.h>
#include <commctrl.h>

#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include "AppState.h"
#include "UiState.h"
#include "MainWindow.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
	const wchar_t CLASS_NAME[] = L"TaskListAppClass";

	LoadUiState();

	WNDCLASS wc = { };
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);

	RegisterClass(&wc);

	HWND hwnd = CreateWindowEx(
		0,
		CLASS_NAME,
		L"タスク管理アプリ",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 850, 600,
		NULL, NULL, hInstance, NULL
	);

	if (hwnd == NULL) return 0;

	if (g_hasSavedWindowPlacement) {
		SetWindowPlacement(hwnd, &g_savedWindowPlacement);
	}

	int initialShowCmd = nCmdShow;
	if (g_hasSavedWindowPlacement) {
		initialShowCmd = g_savedWindowPlacement.showCmd;
		if (initialShowCmd == SW_SHOWMINIMIZED) {
			initialShowCmd = SW_SHOWNORMAL;
		}
	}

	ShowWindow(hwnd, initialShowCmd);

	MSG msg = { };
	while (GetMessage(&msg, NULL, 0, 0)) {
		if (msg.message == WM_KEYDOWN && msg.wParam == 'N' && (GetKeyState(VK_CONTROL) & 0x8000)) {
			SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(9999, 0), 0);
			continue;
		}

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}
