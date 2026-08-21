#pragma once
#include <windows.h>
#include <string>
#include <cstdio>

std::wstring GetCurrentTimeString();
std::wstring GetTaskStoragePath();
bool WriteWString(FILE* fp, const std::wstring& value);
bool ReadWString(FILE* fp, std::wstring& value);
void RefreshTaskList(HWND hList);
bool SaveTasksToFile();
bool LoadTasksFromFile(HWND hList, HWND hEditDetails);
