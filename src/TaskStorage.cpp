#include <windows.h>
#include <commctrl.h>
#include <vector>
#include <string>
#include <cstdio>
#include "TaskStorage.h"
#include "AppState.h"
#include "Ids.h"

#pragma comment(lib, "comctl32.lib")

// ファイルウォッチャー用スタティック変数
static HWND    s_hWatcherTarget = NULL;
static HANDLE  s_hStopEvent     = NULL;
static HANDLE  s_hWatcherThread = NULL;

static DWORD WINAPI FileWatcherThread(LPVOID) {
    std::wstring path = GetTaskStoragePath();
    std::wstring dir  = path;
    size_t pos = dir.find_last_of(L"\\/");
    if (pos != std::wstring::npos) dir.erase(pos + 1);

    HANDLE hDir = CreateFileW(dir.c_str(), FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);
    if (hDir == INVALID_HANDLE_VALUE) return 1;

    OVERLAPPED ov   = {};
    ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    BYTE buf[4096];
    HANDLE handles[2] = { ov.hEvent, s_hStopEvent };

    while (true) {
        ReadDirectoryChangesW(hDir, buf, sizeof(buf), FALSE,
            FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME,
            NULL, &ov, NULL);

        DWORD wait = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
        if (wait != WAIT_OBJECT_0) break; // 停止イベント or エラー

        // tasks.dat への変更か確認
        bool relevant = false;
        auto* fni = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buf);
        for (;;) {
            std::wstring name(fni->FileName, fni->FileNameLength / sizeof(wchar_t));
            if (_wcsicmp(name.c_str(), L"tasks.dat") == 0) {
                relevant = true;
                break;
            }
            if (fni->NextEntryOffset == 0) break;
            fni = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
                reinterpret_cast<BYTE*>(fni) + fni->NextEntryOffset);
        }

        if (relevant) {
            if (g_isSaving) {
                // 自分の保存による通知は無視してフラグをリセット
                g_isSaving = false;
            } else {
                PostMessage(s_hWatcherTarget, WM_APP_FILE_CHANGED, 0, 0);
            }
        }

        ResetEvent(ov.hEvent);
    }

    CloseHandle(ov.hEvent);
    CloseHandle(hDir);
    return 0;
}

void StartFileWatcher(HWND hWnd) {
    s_hWatcherTarget = hWnd;
    s_hStopEvent     = CreateEvent(NULL, TRUE, FALSE, NULL);
    s_hWatcherThread = CreateThread(NULL, 0, FileWatcherThread, NULL, 0, NULL);
}

void StopFileWatcher() {
    if (s_hStopEvent) {
        SetEvent(s_hStopEvent);
    }
    if (s_hWatcherThread) {
        WaitForSingleObject(s_hWatcherThread, 3000);
        CloseHandle(s_hWatcherThread);
        s_hWatcherThread = NULL;
    }
    if (s_hStopEvent) {
        CloseHandle(s_hStopEvent);
        s_hStopEvent = NULL;
    }
    s_hWatcherTarget = NULL;
}

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
	g_isSaving = true;

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
