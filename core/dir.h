#pragma once

// pathfile = directory + name + extension (eg. 'data/file.bin')
// name = name without extension (eg. 'file')
// ext  = extension (including the dot, or other character) (eg. '.bin')

typedef void(*DirScanDirectoryCallback)(const char* pathfile, const char* name);
typedef void(*DirScanFullCallback)(const char* pathfile, const char* name, const char* ext);
typedef void(*DirScanPathfileCallback)(const char* pathfile);

// directory should include trailing slash (eg. 'something/dir/temp/'), suffix/extension is just the end of the files to care about (eg. '.png' or '_myfile') 
void dirScan(const char* directory, DirScanDirectoryCallback callback);
void dirScan(const char* directory, const char* suffix_or_extension, DirScanFullCallback callback);
void dirScan(const char* directory, const char* suffix_or_extension, DirScanPathfileCallback callback);
void dirCreate(const char* directory);