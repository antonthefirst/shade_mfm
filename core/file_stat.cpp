#include "file_stat.h"

// appears to be the same on linux and windows
#include <sys/stat.h>

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
