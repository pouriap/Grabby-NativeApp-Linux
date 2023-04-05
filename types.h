#pragma once

#include "wintypes.h"

struct process_result
{
	DWORD exitCode;
	std::string output;
};
