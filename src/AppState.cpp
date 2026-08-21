#include <windows.h>
#include "AppState.h"

std::vector<Task> g_tasks;
std::vector<int> g_visibleIndices;
int g_selectedIndex = -1;
bool g_isUpdatingUI = false;
volatile bool g_isSaving = false;

int g_sidebarWidth = 280;
bool g_isDraggingSplitter = false;

bool g_hasSavedWindowPlacement = false;
WINDOWPLACEMENT g_savedWindowPlacement = { sizeof(WINDOWPLACEMENT) };

WNDPROC g_OldEditProc;
HWND g_hArchiveWindow = NULL;
