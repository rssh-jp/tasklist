#include <windows.h>
#include <string>
#include <cstdio>
#include "UiState.h"
#include "AppState.h"

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
