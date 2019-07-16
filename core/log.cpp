#include "log.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "core/cpu_timer.h"

#ifdef _WIN32
#include <windows.h> // for OutputDebugString
#else
#include <stdio.h> // for printf
#endif

void log(const char * format, ...) {
	va_list args;
	va_start(args, format);

	static char buff[4096]; 
	
#ifdef _WIN32
	vsprintf_s(buff, format, args);
	OutputDebugString(buff);
#else
	vsnprintf(buff, sizeof(buff), format, args);
	printf("%s", buff);
#endif

	va_end(args);
}

void logError(const char* system, int code, const char * format, ...) {
	va_list args;
	va_start(args, format);

	static char buff[4096]; 
	char* str = buff;
	int64_t t = timeCounterSinceStart();
	int64_t f = time_frequency();

	str += sprintf(str, "%lld.%lld [ ERROR ] [%s]", t / f, t%f, system);
	if (code)
		str += sprintf(str, " [code %d (0x%08x)]", code, code);
	str += sprintf(str, ": '");
	str += vsprintf(str, format, args);
	str += sprintf(str, "'\n");
#ifdef _WIN32
	OutputDebugString(buff);
#else
	printf("%s", buff);
#endif

	va_end(args);
}
void logInfo(const char* system, const char * format, ...) {
	va_list args;
	va_start(args, format);

	static char buff[4096]; 
	char* str = buff;
	int64_t t = timeCounterSinceStart();
	int64_t f = time_frequency();
	str += sprintf(str, "%lld.%lld [ INFO  ] [%s]: ", t/f, t%f, system);
	str += vsprintf(str, format, args);
	str += sprintf(str, "\n");
#ifdef _WIN32
	OutputDebugString(buff);
#else
	printf("%s", buff);
#endif

	va_end(args);
}

#define TEMPSTR_STACK_COUNT 4
static char tempstr_stack[TEMPSTR_STACK_COUNT][2048];
static int tempstr_idx = 0;
static const char* error_str = "Out of temp strings :(";
TempStr::TempStr(const char* format, ...) {
	if (tempstr_idx < (TEMPSTR_STACK_COUNT - 1)) {
		va_list args;
		va_start(args, format);
		len = vsprintf(tempstr_stack[tempstr_idx], format, args);
		va_end(args);
		str = tempstr_stack[tempstr_idx];
		tempstr_idx += 1;
	} else {
		str = error_str;
		len = strlen(error_str);
		assert(false);
	}
}
TempStr::~TempStr() {
	if (str != error_str)
		tempstr_idx -= 1;
	assert(tempstr_idx >= 0);
}