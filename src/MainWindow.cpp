#include <windows.h>
#include <commctrl.h>
#include <vector>
#include <string>
#include "MainWindow.h"
#include "AppState.h"
#include "TaskStorage.h"
#include "UiState.h"
#include "ArchiveWindow.h"
#include "Ids.h"

static void UpdateLayout(HWND hwnd, HWND hEditInput, HWND hBtnAdd, HWND hList, HWND hEditDetails, HWND hBtnArchiveList) {
	if (!hEditInput || !hBtnAdd || !hList || !hEditDetails) return;

	RECT rc;
	GetClientRect(hwnd, &rc);
	int winWidth = rc.right;
	int winHeight = rc.bottom;

	if (g_sidebarWidth < 180) g_sidebarWidth = 180;
	if (g_sidebarWidth > winWidth - 150) g_sidebarWidth = winWidth - 150;

	const int btnArchiveH = 25;
	const int listBottom = winHeight - 50 - btnArchiveH - 6;

	SetWindowPos(hEditInput, NULL, 10, 10, g_sidebarWidth - 80, 25, SWP_NOZORDER);
	SetWindowPos(hBtnAdd, NULL, g_sidebarWidth - 65, 10, 60, 25, SWP_NOZORDER);
	SetWindowPos(hList, NULL, 10, 40, g_sidebarWidth - 15, listBottom, SWP_NOZORDER);
	if (hBtnArchiveList) {
		SetWindowPos(hBtnArchiveList, NULL, 10, 40 + listBottom + 6, g_sidebarWidth - 15, btnArchiveH, SWP_NOZORDER);
	}

	ListView_SetColumnWidth(hList, 0, g_sidebarWidth - 25);

	int detailsX = g_sidebarWidth + SPLITTER_WIDTH + 5;
	int detailsWidth = winWidth - detailsX - 10;
	if (detailsWidth < 50) detailsWidth = 50;

	SetWindowPos(hEditDetails, NULL, detailsX, 10, detailsWidth, winHeight - 20, SWP_NOZORDER);
}

static void AddTaskFromInput(HWND hwnd, HWND hEditInput, HWND hList, HWND hEditDetails) {
	wchar_t buffer[1024];
	GetWindowText(hEditInput, buffer, 1024);

	if (wcslen(buffer) == 0) return;

	std::wstring now = GetCurrentTimeString();

	Task newTask;
	newTask.title = buffer;
	newTask.details = L"";
	newTask.created_at = now;
	newTask.updated_at = now;

	g_tasks.push_back(newTask);

	int visiblePos = static_cast<int>(g_visibleIndices.size());
	g_visibleIndices.push_back(static_cast<int>(g_tasks.size() - 1));

	LVITEM lvi = { 0 };
	lvi.mask = LVIF_TEXT;
	lvi.iItem = visiblePos;
	lvi.pszText = buffer;
	ListView_InsertItem(hList, &lvi);

	ListView_SetItemState(hList, visiblePos, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
	SaveTasksToFile();

	SetWindowText(hEditInput, L"");
	SetFocus(hEditDetails);
}

LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
		SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(ID_BTN_ADD, BN_CLICKED), 0);
		return 0;
	}
	if (msg == WM_CHAR && wParam == VK_RETURN) {
		return 0;
	}
	return CallWindowProc(g_OldEditProc, hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	static HWND hEditInput, hList, hBtnAdd, hEditDetails, hBtnArchiveList;

	switch (msg) {
	case WM_CREATE: {
		INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_LISTVIEW_CLASSES };
		InitCommonControlsEx(&icex);

		hEditInput = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
			WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
			0, 0, 0, 0, hwnd, (HMENU)ID_EDIT_INPUT, GetModuleHandle(NULL), NULL);

		hBtnAdd = CreateWindowEx(0, L"BUTTON", L"追加",
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			0, 0, 0, 0, hwnd, (HMENU)ID_BTN_ADD, GetModuleHandle(NULL), NULL);

		hList = CreateWindowEx(0, WC_LISTVIEW, L"",
			WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_NOCOLUMNHEADER | LVS_SINGLESEL,
			0, 0, 0, 0, hwnd, (HMENU)ID_LISTVIEW, GetModuleHandle(NULL), NULL);

		ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);

		// 文字サイズ変更に伴い1行の高さを 72px に拡張
		HIMAGELIST hDummyImg = ImageList_Create(1, 72, ILC_COLOR32, 1, 1);
		ListView_SetImageList(hList, hDummyImg, LVSIL_SMALL);

		LVCOLUMN lvc = { LVCF_TEXT | LVCF_WIDTH, 0, g_sidebarWidth - 25, (LPWSTR)L"Task" };
		ListView_InsertColumn(hList, 0, &lvc);

		hEditDetails = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
			WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL,
			0, 0, 0, 0, hwnd, (HMENU)ID_EDIT_DETAILS, GetModuleHandle(NULL), NULL);

		EnableWindow(hEditDetails, FALSE);

		hBtnArchiveList = CreateWindowEx(0, L"BUTTON", L"アーカイブ一覧",
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			0, 0, 0, 0, hwnd, (HMENU)ID_BTN_ARCHIVE_LIST, GetModuleHandle(NULL), NULL);

		g_OldEditProc = (WNDPROC)SetWindowLongPtr(hEditInput, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);

		LoadTasksFromFile(hList, hEditDetails);
		StartFileWatcher(hwnd);
		UpdateLayout(hwnd, hEditInput, hBtnAdd, hList, hEditDetails, hBtnArchiveList);
		break;
	}

	case WM_SIZE: {
		UpdateLayout(hwnd, hEditInput, hBtnAdd, hList, hEditDetails, hBtnArchiveList);
		break;
	}

	case WM_SETCURSOR: {
		if (LOWORD(lParam) == HTCLIENT) {
			POINT pt;
			GetCursorPos(&pt);
			ScreenToClient(hwnd, &pt);

			if (pt.x >= g_sidebarWidth - 3 && pt.x <= g_sidebarWidth + SPLITTER_WIDTH + 3) {
				SetCursor(LoadCursor(NULL, IDC_SIZEWE));
				return TRUE;
			}
		}
		break;
	}

	case WM_LBUTTONDOWN: {
		int x = (short)LOWORD(lParam);
		if (x >= g_sidebarWidth - 3 && x <= g_sidebarWidth + SPLITTER_WIDTH + 3) {
			g_isDraggingSplitter = true;
			SetCapture(hwnd);
		}
		break;
	}

	case WM_MOUSEMOVE: {
		if (g_isDraggingSplitter) {
			g_sidebarWidth = (short)LOWORD(lParam);
			UpdateLayout(hwnd, hEditInput, hBtnAdd, hList, hEditDetails, hBtnArchiveList);
		}
		break;
	}

	case WM_LBUTTONUP: {
		if (g_isDraggingSplitter) {
			g_isDraggingSplitter = false;
			ReleaseCapture();
			SaveUiState(hwnd);
		}
		break;
	}

	case WM_COMMAND: {
		if (LOWORD(wParam) == ID_BTN_ADD) {
			AddTaskFromInput(hwnd, hEditInput, hList, hEditDetails);
		}
		else if (LOWORD(wParam) == ID_BTN_ARCHIVE_LIST) {
			if (g_hArchiveWindow && IsWindow(g_hArchiveWindow)) {
				SetForegroundWindow(g_hArchiveWindow);
			} else {
				const wchar_t ARCH_CLASS[] = L"ArchiveWindowClass";
				WNDCLASS wc = {};
				wc.lpfnWndProc = ArchiveWndProc;
				wc.hInstance = GetModuleHandle(NULL);
				wc.lpszClassName = ARCH_CLASS;
				wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
				wc.hCursor = LoadCursor(NULL, IDC_ARROW);
				RegisterClass(&wc);

				g_hArchiveWindow = CreateWindowEx(0, ARCH_CLASS, L"アーカイブ一覧",
					WS_OVERLAPPEDWINDOW,
					CW_USEDEFAULT, CW_USEDEFAULT, 500, 400,
					hwnd, NULL, GetModuleHandle(NULL), NULL);
				ShowWindow(g_hArchiveWindow, SW_SHOW);
			}
		}
		else if (LOWORD(wParam) == 9999) { // Ctrl + N
			SetFocus(hEditInput);
			SendMessage(hEditInput, EM_SETSEL, 0, -1);
		}
		else if (LOWORD(wParam) == IDM_ARCHIVE) {
			int selItem = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
			if (selItem >= 0 && selItem < static_cast<int>(g_visibleIndices.size())) {
				int taskIdx = g_visibleIndices[selItem];
				g_tasks[taskIdx].archived = true;
				SaveTasksToFile();
				RefreshTaskList(hList);
				g_selectedIndex = -1;
				if (!g_visibleIndices.empty()) {
					int newSel = min(selItem, static_cast<int>(g_visibleIndices.size()) - 1);
					g_selectedIndex = newSel;
					g_isUpdatingUI = true;
					EnableWindow(hEditDetails, TRUE);
					SetWindowText(hEditDetails, g_tasks[g_visibleIndices[newSel]].details.c_str());
					g_isUpdatingUI = false;
					ListView_SetItemState(hList, newSel, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
				} else {
					EnableWindow(hEditDetails, FALSE);
					SetWindowText(hEditDetails, L"");
				}
			}
		}
		else if (LOWORD(wParam) == ID_EDIT_DETAILS && HIWORD(wParam) == EN_CHANGE) {
			if (!g_isUpdatingUI && g_selectedIndex >= 0 && g_selectedIndex < static_cast<int>(g_visibleIndices.size())) {
				int taskIdx = g_visibleIndices[g_selectedIndex];
				int length = GetWindowTextLength(hEditDetails);
				std::vector<wchar_t> buffer(length + 1);
				GetWindowText(hEditDetails, buffer.data(), length + 1);

				g_tasks[taskIdx].details = buffer.data();
				g_tasks[taskIdx].updated_at = GetCurrentTimeString();
				SaveTasksToFile();

				ListView_RedrawItems(hList, g_selectedIndex, g_selectedIndex);
				UpdateWindow(hList);
			}
		}
		break;
	}

	case WM_APP_FILE_CHANGED:
		LoadTasksFromFile(hList, hEditDetails);
		if (g_hArchiveWindow && IsWindow(g_hArchiveWindow)) {
			PostMessage(g_hArchiveWindow, WM_APP_FILE_CHANGED, 0, 0);
		}
		break;

	case WM_EXITSIZEMOVE:
		SaveUiState(hwnd);
		break;

	case WM_NOTIFY: {
		LPNMHDR pnmh = (LPNMHDR)lParam;
		if (pnmh->idFrom == ID_LISTVIEW) {
			if (pnmh->code == NM_CUSTOMDRAW) {
				LPNMLVCUSTOMDRAW pLVCD = (LPNMLVCUSTOMDRAW)lParam;
				if (pLVCD->nmcd.dwDrawStage == CDDS_PREPAINT) {
					return CDRF_NOTIFYITEMDRAW;
				}
				else if (pLVCD->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
					int index = (int)pLVCD->nmcd.dwItemSpec;
					if (index < 0 || index >= static_cast<int>(g_visibleIndices.size())) return CDRF_DODEFAULT;
					int taskIdx = g_visibleIndices[index];

					HDC hdc = pLVCD->nmcd.hdc;
					RECT rc = pLVCD->nmcd.rc;
					bool isSelected = (pLVCD->nmcd.uItemState & CDIS_SELECTED) != 0;

					HBRUSH hBgBrush = CreateSolidBrush(isSelected ? RGB(220, 235, 252) : RGB(248, 249, 250));
					HPEN hBorderPen = CreatePen(PS_SOLID, 1, isSelected ? RGB(0, 120, 215) : RGB(210, 210, 210));

					HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBgBrush);
					HPEN hOldPen = (HPEN)SelectObject(hdc, hBorderPen);

					RECT cassetteRc = rc;
					cassetteRc.left += 2;
					cassetteRc.right -= 2;
					cassetteRc.top += 2;
					cassetteRc.bottom -= 2;

					RoundRect(hdc, cassetteRc.left, cassetteRc.top, cassetteRc.right, cassetteRc.bottom, 6, 6);

					SelectObject(hdc, hOldBrush);
					SelectObject(hdc, hOldPen);
					DeleteObject(hBgBrush);
					DeleteObject(hBorderPen);

					SetBkMode(hdc, TRANSPARENT);

					RECT textRc = cassetteRc;
					textRc.left += 10;
					textRc.right -= 10;

					// タイトル描画（フォントサイズ 15 → 23）
					SetTextColor(hdc, RGB(30, 30, 30));
					HFONT hBoldFont = CreateFont(23, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
						DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
					HFONT hOldFont = (HFONT)SelectObject(hdc, hBoldFont);

					RECT titleRc = textRc;
					titleRc.top += 6;
					titleRc.bottom = titleRc.top + 28;
					DrawText(hdc, g_tasks[taskIdx].title.c_str(), -1, &titleRc, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

					SelectObject(hdc, hOldFont);
					DeleteObject(hBoldFont);

					// 作成・更新日時描画（フォントサイズ 11 → 17）
					HFONT hSmallFont = CreateFont(17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
						DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
					hOldFont = (HFONT)SelectObject(hdc, hSmallFont);

					SetTextColor(hdc, RGB(110, 110, 110));
					std::wstring timeStr = L"作成:" + g_tasks[taskIdx].created_at + L"  更新:" + g_tasks[taskIdx].updated_at;

					RECT timeRc = textRc;
					timeRc.top += 38;
					DrawText(hdc, timeStr.c_str(), -1, &timeRc, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

					SelectObject(hdc, hOldFont);
					DeleteObject(hSmallFont);

					return CDRF_SKIPDEFAULT;
				}
			}
			else if (pnmh->code == LVN_ITEMCHANGED) {
				LPNMLISTVIEW pNmlv = (LPNMLISTVIEW)lParam;
				if ((pNmlv->uChanged & LVIF_STATE) && (pNmlv->uNewState & LVIS_SELECTED)) {
					g_selectedIndex = pNmlv->iItem;

					if (g_selectedIndex >= 0 && g_selectedIndex < static_cast<int>(g_visibleIndices.size())) {
						g_isUpdatingUI = true;
						EnableWindow(hEditDetails, TRUE);
						SetWindowText(hEditDetails, g_tasks[g_visibleIndices[g_selectedIndex]].details.c_str());
						g_isUpdatingUI = false;
					}
				}
			}
			else if (pnmh->code == LVN_GETINFOTIP) {
				NMLVGETINFOTIP* pGetInfoTip = (NMLVGETINFOTIP*)lParam;
				int itemIndex = pGetInfoTip->iItem;
				if (itemIndex >= 0 && itemIndex < static_cast<int>(g_visibleIndices.size())) {
					wcscpy_s(pGetInfoTip->pszText, pGetInfoTip->cchTextMax, g_tasks[g_visibleIndices[itemIndex]].title.c_str());
				}
			}
			else if (pnmh->code == NM_RCLICK) {
				LPNMITEMACTIVATE pNMIA = (LPNMITEMACTIVATE)lParam;
				int clickedItem = pNMIA->iItem;
				if (clickedItem >= 0 && clickedItem < static_cast<int>(g_visibleIndices.size())) {
					POINT pt;
					GetCursorPos(&pt);
					HMENU hMenu = CreatePopupMenu();
					AppendMenu(hMenu, MF_STRING, IDM_ARCHIVE, L"アーカイブ");
					TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
					DestroyMenu(hMenu);
				}
			}
		}
		break;
	}

	case WM_DESTROY:
		StopFileWatcher();
		SaveUiState(hwnd);
		SaveTasksToFile();
		PostQuitMessage(0);
		break;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}
