#pragma once
#include <vector>
#include <windows.h>
#include "Task.h"

extern std::vector<Task> g_tasks;
extern std::vector<int> g_visibleIndices;
extern int g_selectedIndex;
extern bool g_isUpdatingUI;
extern volatile bool g_isSaving;

extern int g_sidebarWidth;
extern bool g_isDraggingSplitter;

extern bool g_hasSavedWindowPlacement;
extern WINDOWPLACEMENT g_savedWindowPlacement;

extern WNDPROC g_OldEditProc;
extern HWND g_hArchiveWindow;
