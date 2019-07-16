#include "dir.h"

#ifdef _WIN32
#include "core/win_dirent.h"
#else
#include <dirent.h>   /* for real dirent */
#endif

#include <string.h>   /* for strlen */
#include "core/maths.h" // for min

static void dirScanInternal(const char* directory, const char* suffix_or_extension, DirScanFullCallback full_callback, DirScanPathfileCallback pathfile_callback, DirScanDirectoryCallback directory_callback) {
	char strtmp[2048];
	char nametmp[1024];
	char exttmp[32];
	size_t dir_namlen = strlen(directory);
	memcpy(strtmp, directory, min(dir_namlen + 1, sizeof(strtmp)));
	strtmp[dir_namlen] = 0;
	DIR *dirp = opendir(strtmp);
	unsigned extlen = suffix_or_extension ? strlen(suffix_or_extension) : 0;
	if (dirp) { 
		struct dirent *entry;
		while ((entry = readdir(dirp))) {
			size_t d_namlen = strlen(entry->d_name);
			bool type_filter = entry->d_type == (directory_callback ? DT_DIR : DT_REG);
			bool ext_filter = directory_callback || entry->d_type == DT_DIR || (d_namlen > extlen && (strstr(entry->d_name, suffix_or_extension) == (entry->d_name + d_namlen) - extlen));
			bool dot_filter = entry->d_type == DT_REG || (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0);
			if (type_filter && ext_filter && dot_filter) {
				if (full_callback || pathfile_callback) {
					memcpy(strtmp + dir_namlen, entry->d_name, min(d_namlen + 1, sizeof(strtmp))); // save one slot for 0
					strtmp[dir_namlen + d_namlen] = 0;
					if (pathfile_callback) {
						pathfile_callback(strtmp);
					}
					if (full_callback) {
						memcpy(nametmp, entry->d_name, d_namlen - extlen);
						nametmp[d_namlen - extlen] = 0;
						memcpy(exttmp, entry->d_name + d_namlen - extlen, extlen);
						exttmp[extlen] = 0;
						full_callback(strtmp, nametmp, exttmp);
					}
				}
				if (directory_callback) {
					memcpy(strtmp + dir_namlen, entry->d_name, min(d_namlen + 2, sizeof(strtmp))); // save one slot for 0 and slash
					strtmp[dir_namlen + d_namlen + 0] = '/';
					strtmp[dir_namlen + d_namlen + 1] = 0;
					memcpy(nametmp, entry->d_name, d_namlen - extlen);
					nametmp[d_namlen - extlen] = 0;
					directory_callback(strtmp, nametmp);
				}
			}
		}
		closedir(dirp);
	}
} 
void dirScan(const char* directory, DirScanDirectoryCallback callback) {
	dirScanInternal(directory, NULL, NULL, NULL, callback);
}
void dirScan(const char* directory, const char* suffix_or_extension, DirScanFullCallback callback) {
	dirScanInternal(directory, suffix_or_extension, callback, NULL, NULL);
}
void dirScan(const char* directory, const char* suffix_or_extension, DirScanPathfileCallback callback) {
	dirScanInternal(directory, suffix_or_extension, NULL, callback, NULL);
}
