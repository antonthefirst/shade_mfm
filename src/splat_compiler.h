#pragma once
#include "core/container.h"
#include "core/string_range.h"
#include "data_fields.h"

#define COLOR_TEXT vec4(1.0f, 1.0f, 1.0f, 1.0f)
#define COLOR_ERROR vec4(1.0f, 0.1f, 0.1f, 1.0f)

struct ElementInfo {
	StringRange name = "";
	StringRange symbol = "";
	u32 color = 0; // in ABGR format. (not ARGB, like MFM)
	Bunch<DataField> data;
};

struct ProgramInfo {
	Bunch<ElementInfo> elems;
	s64 time_to_lex;
	s64 time_to_parse;
	s64 time_to_emit;
};

void checkForSplatProgramChanges(bool* file_change, bool* project_change, ProgramInfo* info);
void showSplatCompilerErrors(ProgramInfo* info, StringRange glsl_err, float glsl_time_to_compile);