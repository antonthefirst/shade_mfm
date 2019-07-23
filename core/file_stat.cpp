#include "file_stat.h"

// appears to be the same on linux and windows
#include <sys/stat.h>
#include <stdio.h>  // for fopen
#include <stdlib.h> // for malloc

int fileStat(const char* filename, FileStats* stats) {
	int res;
#ifdef _WIN32
	struct _stat buf;
	res = _stat(filename, &buf);
	stats->fs_mtime = buf.st_mtime;
#else
	struct stat buf;
	res = stat(filename, &buf);
	stats->fs_mtime = buf.st_mtime;
#endif
	return res;
}

char* fileReadBinaryIntoMem(const char* pathfile, size_t* bytesize_out) {
	size_t bytesize = 0;
	char* mem = 0;
	FILE* f = fopen(pathfile, "rb");
	if (f) {
		fseek(f, 0, SEEK_END);
		bytesize = ftell(f);
		fseek(f, 0, SEEK_SET);
		mem = (char*)malloc(bytesize);
		fread(mem, bytesize, 1, f);
		fclose(f);
	}
	if (bytesize_out) *bytesize_out = bytesize;
	return mem;
}

char* fileReadCStringIntoMem(const char* pathfile, size_t* bytesize_out) {
	size_t bytesize = 0;
	char* mem = 0;
	FILE* f = fopen(pathfile, "rb");
	if (f) {
		fseek(f, 0, SEEK_END);
		bytesize = ftell(f);
		fseek(f, 0, SEEK_SET);
		mem = (char*)malloc(bytesize+1);
		fread(mem, bytesize, 1, f);
		mem[bytesize] = 0; // null term
		fclose(f);
	}
	if (bytesize_out) *bytesize_out = bytesize;
	return mem;
}