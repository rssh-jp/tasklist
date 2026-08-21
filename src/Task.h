#pragma once
#include <string>

struct Task {
	std::wstring title;
	std::wstring details;
	std::wstring created_at;
	std::wstring updated_at;
	bool archived = false;
};
