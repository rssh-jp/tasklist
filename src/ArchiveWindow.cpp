#include <windows.h>
#include <commctrl.h>
#include <vector>
#include "ArchiveWindow.h"
#include "AppState.h"
#include "TaskStorage.h"
#include "Ids.h"

LRESULT CALLBACK ArchiveWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	static HWND hArchiveList, hBtnUnarchive;
	static std::vector<int> s_archiveIndices;

	switch (msg) {
	case WM_CREATE: {
		hArchiveList = CreateWindowEx(0, WC_LISTVIEW, L"",
			WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_NOCOLUMNHEADER | LVS_SINGLESEL,
			0, 0, 0, 0, hwnd, (HMENU)ID_ARCHIVE_LISTVIEW, GetModuleHandle(NULL), NULL);
		ListView_SetExtendedListViewStyle(hArchiveList, LVS_EX_FULLROWSELECT);

		LVCOLUMN lvc = { LVCF_TEXT | LVCF_WIDTH, 0, 400, (LPWSTR)L"Task" };
		ListView_InsertColumn(hArchiveList, 0, &lvc);

		hBtnUnarchive = CreateWindowEx(0, L"BUTTON", L"アーカイブ解除",
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			0, 0, 0, 0, hwnd, (HMENU)ID_BTN_UNARCHIVE, GetModuleHandle(NULL), NULL);

		// アーカイブ済みタスクを列挙
		s_archiveIndices.clear();
		ListView_DeleteAllItems(hArchiveList);
		for (int i = 0; i < static_cast<int>(g_tasks.size()); ++i) {
			if (!g_tasks[i].archived) continue;
			int pos = static_cast<int>(s_archiveIndices.size());
			s_archiveIndices.push_back(i);
			LVITEM lvi = { 0 };
			lvi.mask = LVIF_TEXT;
			lvi.iItem = pos;
			lvi.pszText = const_cast<LPWSTR>(g_tasks[i].title.c_str());
			ListView_InsertItem(hArchiveList, &lvi);
		}
		break;
	}

	case WM_SIZE: {
		RECT rc;
		GetClientRect(hwnd, &rc);
		int w = rc.right;
		int h = rc.bottom;
		SetWindowPos(hArchiveList, NULL, 10, 10, w - 20, h - 50, SWP_NOZORDER);
		SetWindowPos(hBtnUnarchive, NULL, 10, h - 35, 120, 25, SWP_NOZORDER);
		ListView_SetColumnWidth(hArchiveList, 0, w - 30);
		break;
	}

	case WM_COMMAND: {
		if (LOWORD(wParam) == ID_BTN_UNARCHIVE) {
			int selItem = ListView_GetNextItem(hArchiveList, -1, LVNI_SELECTED);
			if (selItem >= 0 && selItem < static_cast<int>(s_archiveIndices.size())) {
				int taskIdx = s_archiveIndices[selItem];
				g_tasks[taskIdx].archived = false;
				SaveTasksToFile();

				// リストを再構築
				s_archiveIndices.clear();
				ListView_DeleteAllItems(hArchiveList);
				for (int i = 0; i < static_cast<int>(g_tasks.size()); ++i) {
					if (!g_tasks[i].archived) continue;
					int pos = static_cast<int>(s_archiveIndices.size());
					s_archiveIndices.push_back(i);
					LVITEM lvi = { 0 };
					lvi.mask = LVIF_TEXT;
					lvi.iItem = pos;
					lvi.pszText = const_cast<LPWSTR>(g_tasks[i].title.c_str());
					ListView_InsertItem(hArchiveList, &lvi);
				}

				// 親ウィンドウのリストも更新
				HWND hParent = GetWindow(hwnd, GW_OWNER);
				if (hParent) {
					HWND hMainList = GetDlgItem(hParent, ID_LISTVIEW);
					HWND hMainDetails = GetDlgItem(hParent, ID_EDIT_DETAILS);
					if (hMainList) {
						RefreshTaskList(hMainList);
						if (!g_visibleIndices.empty()) {
							g_selectedIndex = 0;
							if (hMainDetails) {
								EnableWindow(hMainDetails, TRUE);
								SetWindowText(hMainDetails, g_tasks[g_visibleIndices[0]].details.c_str());
							}
							ListView_SetItemState(hMainList, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
						} else {
							g_selectedIndex = -1;
							if (hMainDetails) {
								EnableWindow(hMainDetails, FALSE);
								SetWindowText(hMainDetails, L"");
							}
						}
					}
				}
			}
		}
		break;
	}

	case WM_APP_FILE_CHANGED: {
		if (GetForegroundWindow() != hwnd) {
			s_archiveIndices.clear();
			ListView_DeleteAllItems(hArchiveList);
			for (int i = 0; i < static_cast<int>(g_tasks.size()); ++i) {
				if (!g_tasks[i].archived) continue;
				int pos = static_cast<int>(s_archiveIndices.size());
				s_archiveIndices.push_back(i);
				LVITEM lvi = { 0 };
				lvi.mask = LVIF_TEXT;
				lvi.iItem = pos;
				lvi.pszText = const_cast<LPWSTR>(g_tasks[i].title.c_str());
				ListView_InsertItem(hArchiveList, &lvi);
			}
		}
		break;
	}

	case WM_DESTROY:
		g_hArchiveWindow = NULL;
		break;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}
