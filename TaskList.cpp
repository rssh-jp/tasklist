#include <windows.h>
#include <commctrl.h>
#include <vector>
#include <string>
#include <cstdio>

#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(lib, "comctl32.lib")

#define ID_EDIT_INPUT   101
#define ID_LISTVIEW     102
#define ID_BTN_ADD      103
#define ID_EDIT_DETAILS 104

const int SPLITTER_WIDTH = 6;

struct Task {
    std::wstring title;
    std::wstring details;
    std::wstring created_at;
    std::wstring updated_at;
};

std::vector<Task> g_tasks;
int g_selectedIndex = -1;
bool g_isUpdatingUI = false;

int g_sidebarWidth = 280;
bool g_isDraggingSplitter = false;

WNDPROC g_OldEditProc;

std::wstring GetCurrentTimeString() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t buf[64];
    swprintf_s(buf, L"%04d/%02d/%02d %02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
    return buf;
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

void UpdateLayout(HWND hwnd, HWND hEditInput, HWND hBtnAdd, HWND hList, HWND hEditDetails) {
    if (!hEditInput || !hBtnAdd || !hList || !hEditDetails) return;

    RECT rc;
    GetClientRect(hwnd, &rc);
    int winWidth = rc.right;
    int winHeight = rc.bottom;

    if (g_sidebarWidth < 180) g_sidebarWidth = 180;
    if (g_sidebarWidth > winWidth - 150) g_sidebarWidth = winWidth - 150;

    SetWindowPos(hEditInput, NULL, 10, 10, g_sidebarWidth - 80, 25, SWP_NOZORDER);
    SetWindowPos(hBtnAdd, NULL, g_sidebarWidth - 65, 10, 60, 25, SWP_NOZORDER);
    SetWindowPos(hList, NULL, 10, 40, g_sidebarWidth - 15, winHeight - 50, SWP_NOZORDER);

    ListView_SetColumnWidth(hList, 0, g_sidebarWidth - 25);

    int detailsX = g_sidebarWidth + SPLITTER_WIDTH + 5;
    int detailsWidth = winWidth - detailsX - 10;
    if (detailsWidth < 50) detailsWidth = 50;

    SetWindowPos(hEditDetails, NULL, detailsX, 10, detailsWidth, winHeight - 20, SWP_NOZORDER);
}

void AddTaskFromInput(HWND hwnd, HWND hEditInput, HWND hList, HWND hEditDetails) {
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

    int newIndex = static_cast<int>(g_tasks.size() - 1);

    LVITEM lvi = { 0 };
    lvi.mask = LVIF_TEXT;
    lvi.iItem = newIndex;
    lvi.pszText = buffer;
    ListView_InsertItem(hList, &lvi);

    ListView_SetItemState(hList, newIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);

    SetWindowText(hEditInput, L"");
    SetFocus(hEditDetails);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HWND hEditInput, hList, hBtnAdd, hEditDetails;

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

        g_OldEditProc = (WNDPROC)SetWindowLongPtr(hEditInput, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);

        UpdateLayout(hwnd, hEditInput, hBtnAdd, hList, hEditDetails);
        break;
    }

    case WM_SIZE: {
        UpdateLayout(hwnd, hEditInput, hBtnAdd, hList, hEditDetails);
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
            UpdateLayout(hwnd, hEditInput, hBtnAdd, hList, hEditDetails);
        }
        break;
    }

    case WM_LBUTTONUP: {
        if (g_isDraggingSplitter) {
            g_isDraggingSplitter = false;
            ReleaseCapture();
        }
        break;
    }

    case WM_COMMAND: {
        if (LOWORD(wParam) == ID_BTN_ADD) {
            AddTaskFromInput(hwnd, hEditInput, hList, hEditDetails);
        }
        else if (LOWORD(wParam) == 9999) { // Ctrl + N
            SetFocus(hEditInput);
            SendMessage(hEditInput, EM_SETSEL, 0, -1);
        }
        else if (LOWORD(wParam) == ID_EDIT_DETAILS && HIWORD(wParam) == EN_CHANGE) {
            if (!g_isUpdatingUI && g_selectedIndex >= 0 && g_selectedIndex < static_cast<int>(g_tasks.size())) {
                int length = GetWindowTextLength(hEditDetails);
                std::vector<wchar_t> buffer(length + 1);
                GetWindowText(hEditDetails, buffer.data(), length + 1);

                g_tasks[g_selectedIndex].details = buffer.data();
                g_tasks[g_selectedIndex].updated_at = GetCurrentTimeString();

                ListView_RedrawItems(hList, g_selectedIndex, g_selectedIndex);
                UpdateWindow(hList);
            }
        }
        break;
    }

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
                    if (index < 0 || index >= static_cast<int>(g_tasks.size())) return CDRF_DODEFAULT;

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
                    DrawText(hdc, g_tasks[index].title.c_str(), -1, &titleRc, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

                    SelectObject(hdc, hOldFont);
                    DeleteObject(hBoldFont);

                    // 作成・更新日時描画（フォントサイズ 11 → 17）
                    HFONT hSmallFont = CreateFont(17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
                    hOldFont = (HFONT)SelectObject(hdc, hSmallFont);

                    SetTextColor(hdc, RGB(110, 110, 110));
                    std::wstring timeStr = L"作成:" + g_tasks[index].created_at + L"  更新:" + g_tasks[index].updated_at;

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

                    if (g_selectedIndex >= 0 && g_selectedIndex < static_cast<int>(g_tasks.size())) {
                        g_isUpdatingUI = true;
                        EnableWindow(hEditDetails, TRUE);
                        SetWindowText(hEditDetails, g_tasks[g_selectedIndex].details.c_str());
                        g_isUpdatingUI = false;
                    }
                }
            }
            else if (pnmh->code == LVN_GETINFOTIP) {
                NMLVGETINFOTIP* pGetInfoTip = (NMLVGETINFOTIP*)lParam;
                int itemIndex = pGetInfoTip->iItem;
                if (itemIndex >= 0 && itemIndex < static_cast<int>(g_tasks.size())) {
                    wcscpy_s(pGetInfoTip->pszText, pGetInfoTip->cchTextMax, g_tasks[itemIndex].title.c_str());
                }
            }
        }
        break;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"TaskListAppClass";

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
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 850, 600,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) return 0;

    ShowWindow(hwnd, nCmdShow);

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