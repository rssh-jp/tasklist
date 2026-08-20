#include <windows.h>
#include <commctrl.h>
#include <vector>
#include <string>
#include <cstdio>

#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(lib, "comctl32.lib")

#define ID_EDIT_INPUT       101
#define ID_LISTVIEW         102
#define ID_BTN_ADD          103
#define ID_EDIT_DETAILS     104
#define ID_BTN_ARCHIVE_LIST 105
#define ID_ARCHIVE_LISTVIEW 106
#define ID_BTN_UNARCHIVE    107
#define IDM_ARCHIVE         201

const int SPLITTER_WIDTH = 6;

struct Task {
    std::wstring title;
    std::wstring details;
    std::wstring created_at;
    std::wstring updated_at;
    bool archived = false;
};

std::vector<Task> g_tasks;
std::vector<int> g_visibleIndices;
int g_selectedIndex = -1;
bool g_isUpdatingUI = false;

int g_sidebarWidth = 280;
bool g_isDraggingSplitter = false;

bool g_hasSavedWindowPlacement = false;
WINDOWPLACEMENT g_savedWindowPlacement = { sizeof(WINDOWPLACEMENT) };

WNDPROC g_OldEditProc;

std::wstring GetCurrentTimeString() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t buf[64];
    swprintf_s(buf, L"%04d/%02d/%02d %02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
    return buf;
}

std::wstring GetTaskStoragePath() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(NULL, path, MAX_PATH);

    std::wstring filePath = path;
    size_t pos = filePath.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        filePath.erase(pos + 1);
    }
    else {
        filePath.clear();
    }

    filePath += L"tasks.dat";
    return filePath;
}

bool WriteWString(FILE* fp, const std::wstring& value) {
    unsigned int length = static_cast<unsigned int>(value.size());
    if (fwrite(&length, sizeof(length), 1, fp) != 1) {
        return false;
    }

    if (length > 0 && fwrite(value.data(), sizeof(wchar_t), length, fp) != length) {
        return false;
    }

    return true;
}

bool ReadWString(FILE* fp, std::wstring& value) {
    unsigned int length = 0;
    if (fread(&length, sizeof(length), 1, fp) != 1) {
        return false;
    }

    value.resize(length);
    if (length > 0 && fread(&value[0], sizeof(wchar_t), length, fp) != length) {
        return false;
    }

    return true;
}

void RefreshTaskList(HWND hList) {
    ListView_DeleteAllItems(hList);
    g_visibleIndices.clear();

    for (int i = 0; i < static_cast<int>(g_tasks.size()); ++i) {
        if (g_tasks[i].archived) continue;
        int visiblePos = static_cast<int>(g_visibleIndices.size());
        g_visibleIndices.push_back(i);
        LVITEM lvi = { 0 };
        lvi.mask = LVIF_TEXT;
        lvi.iItem = visiblePos;
        lvi.pszText = const_cast<LPWSTR>(g_tasks[i].title.c_str());
        ListView_InsertItem(hList, &lvi);
    }
}

bool SaveTasksToFile() {
    std::wstring path = GetTaskStoragePath();
    FILE* fp = nullptr;
    if (_wfopen_s(&fp, path.c_str(), L"wb") != 0 || !fp) {
        return false;
    }

    DWORD magic = 0x534B5441; // 'ATKS'
    DWORD version = 2;
    unsigned int count = static_cast<unsigned int>(g_tasks.size());
    bool ok = fwrite(&magic, sizeof(magic), 1, fp) == 1
        && fwrite(&version, sizeof(version), 1, fp) == 1
        && fwrite(&count, sizeof(count), 1, fp) == 1;
    for (const Task& task : g_tasks) {
        ok = ok && WriteWString(fp, task.title)
            && WriteWString(fp, task.details)
            && WriteWString(fp, task.created_at)
            && WriteWString(fp, task.updated_at)
            && fwrite(&task.archived, sizeof(task.archived), 1, fp) == 1;
        if (!ok) {
            break;
        }
    }

    fclose(fp);
    return ok;
}

bool LoadTasksFromFile(HWND hList, HWND hEditDetails) {
    g_tasks.clear();

    std::wstring path = GetTaskStoragePath();
    FILE* fp = nullptr;
    if (_wfopen_s(&fp, path.c_str(), L"rb") != 0 || !fp) {
        RefreshTaskList(hList);
        g_selectedIndex = -1;
        EnableWindow(hEditDetails, FALSE);
        SetWindowText(hEditDetails, L"");
        return false;
    }

    // マジックナンバーを読み込んでバージョンを判定
    DWORD magic = 0;
    DWORD version = 1;
    unsigned int count = 0;
    bool ok = fread(&magic, sizeof(magic), 1, fp) == 1;
    if (ok && magic == 0x534B5441) {
        // v2以降: magic + version + count
        ok = fread(&version, sizeof(version), 1, fp) == 1
            && fread(&count, sizeof(count), 1, fp) == 1;
    } else {
        // v1: 先頭4バイトがcountだった
        version = 1;
        count = static_cast<unsigned int>(magic);
    }

    for (unsigned int i = 0; ok && i < count; ++i) {
        Task task;
        ok = ReadWString(fp, task.title)
            && ReadWString(fp, task.details)
            && ReadWString(fp, task.created_at)
            && ReadWString(fp, task.updated_at);
        if (ok && version >= 2) {
            ok = fread(&task.archived, sizeof(task.archived), 1, fp) == 1;
        }
        if (ok) {
            g_tasks.push_back(task);
        }
    }

    fclose(fp);

    if (!ok) {
        g_tasks.clear();
    }

    RefreshTaskList(hList);
    g_selectedIndex = -1;

    if (!g_visibleIndices.empty()) {
        g_selectedIndex = 0;
        EnableWindow(hEditDetails, TRUE);
        SetWindowText(hEditDetails, g_tasks[g_visibleIndices[0]].details.c_str());
        ListView_SetItemState(hList, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(hList, 0, FALSE);
    }
    else {
        EnableWindow(hEditDetails, FALSE);
        SetWindowText(hEditDetails, L"");
    }

    return ok;
}

std::wstring GetUiStatePath() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(NULL, path, MAX_PATH);

    std::wstring filePath = path;
    size_t pos = filePath.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        filePath.erase(pos + 1);
    }
    else {
        filePath.clear();
    }

    filePath += L"ui_state.dat";
    return filePath;
}

bool LoadUiState() {
    g_sidebarWidth = 280;
    g_hasSavedWindowPlacement = false;
    g_savedWindowPlacement = { sizeof(WINDOWPLACEMENT) };

    std::wstring path = GetUiStatePath();
    FILE* fp = nullptr;
    if (_wfopen_s(&fp, path.c_str(), L"rb") != 0 || !fp) {
        return false;
    }

    DWORD magic = 0;
    DWORD version = 0;
    bool hasPlacement = false;
    bool ok = fread(&magic, sizeof(magic), 1, fp) == 1
        && fread(&version, sizeof(version), 1, fp) == 1
        && fread(&g_sidebarWidth, sizeof(g_sidebarWidth), 1, fp) == 1
        && fread(&hasPlacement, sizeof(hasPlacement), 1, fp) == 1;

    if (ok && magic == 0x31534955 && version == 1 && hasPlacement) {
        g_savedWindowPlacement.length = sizeof(g_savedWindowPlacement);
        ok = fread(&g_savedWindowPlacement.flags, sizeof(g_savedWindowPlacement.flags), 1, fp) == 1
            && fread(&g_savedWindowPlacement.showCmd, sizeof(g_savedWindowPlacement.showCmd), 1, fp) == 1
            && fread(&g_savedWindowPlacement.ptMinPosition, sizeof(g_savedWindowPlacement.ptMinPosition), 1, fp) == 1
            && fread(&g_savedWindowPlacement.ptMaxPosition, sizeof(g_savedWindowPlacement.ptMaxPosition), 1, fp) == 1
            && fread(&g_savedWindowPlacement.rcNormalPosition, sizeof(g_savedWindowPlacement.rcNormalPosition), 1, fp) == 1;
        g_hasSavedWindowPlacement = ok;
    }
    else if (!hasPlacement) {
        ok = ok && magic == 0x31534955 && version == 1;
    }

    fclose(fp);

    if (!ok) {
        g_sidebarWidth = 280;
        g_hasSavedWindowPlacement = false;
        g_savedWindowPlacement = { sizeof(WINDOWPLACEMENT) };
    }

    return ok;
}

bool SaveUiState(HWND hwnd) {
    std::wstring path = GetUiStatePath();
    FILE* fp = nullptr;
    if (_wfopen_s(&fp, path.c_str(), L"wb") != 0 || !fp) {
        return false;
    }

    DWORD magic = 0x31534955;
    DWORD version = 1;
    bool hasPlacement = false;
    WINDOWPLACEMENT placement = { sizeof(WINDOWPLACEMENT) };
    if (hwnd != NULL && IsWindow(hwnd) && GetWindowPlacement(hwnd, &placement)) {
        hasPlacement = true;
    }

    bool ok = fwrite(&magic, sizeof(magic), 1, fp) == 1
        && fwrite(&version, sizeof(version), 1, fp) == 1
        && fwrite(&g_sidebarWidth, sizeof(g_sidebarWidth), 1, fp) == 1
        && fwrite(&hasPlacement, sizeof(hasPlacement), 1, fp) == 1;

    if (ok && hasPlacement) {
        ok = fwrite(&placement.flags, sizeof(placement.flags), 1, fp) == 1
            && fwrite(&placement.showCmd, sizeof(placement.showCmd), 1, fp) == 1
            && fwrite(&placement.ptMinPosition, sizeof(placement.ptMinPosition), 1, fp) == 1
            && fwrite(&placement.ptMaxPosition, sizeof(placement.ptMaxPosition), 1, fp) == 1
            && fwrite(&placement.rcNormalPosition, sizeof(placement.rcNormalPosition), 1, fp) == 1;
    }

    fclose(fp);
    return ok;
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

void UpdateLayout(HWND hwnd, HWND hEditInput, HWND hBtnAdd, HWND hList, HWND hEditDetails, HWND hBtnArchiveList) {
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

HWND g_hArchiveWindow = NULL;

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

    case WM_DESTROY:
        g_hArchiveWindow = NULL;
        break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
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
        SaveUiState(hwnd);
        SaveTasksToFile();
        PostQuitMessage(0);
        break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

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
