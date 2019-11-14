#pragma once
#include "basic_types.h"
#include "stdlib.h" // for size_t
#include "string_range.h"

struct FileStats {
	int64_t fs_mtime;
};

int fileStat(const char* filename, FileStats* stats);
void fileDelete(const char* filename);

char* fileReadBinaryIntoMem(const char* pathfile, size_t* bytesize = 0); 
char* fileReadCStringIntoMem(const char* pathfile, size_t* bytesize = 0); 
bool fileWriteBinary(const char* pathfile, char* bytes, size_t bytesize);