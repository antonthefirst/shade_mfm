#pragma once
#include "core/container.h"
#include "core/string_range.h"

struct ProgramInfo {
	Bunch<StringRange> type_names;
};

void checkForSplatProgramChanges(bool* file_change, bool* project_change, ProgramInfo* info);