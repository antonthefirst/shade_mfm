#pragma once
#include "basic_types.h"

struct FileStats {
	int64_t fs_mtime;
};

int fileStat(const char* filename, FileStats* stats);
