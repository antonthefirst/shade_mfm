#include "core/string_range.h"

#ifdef _WIN32

#include <stdio.h>
#include <windows.h>

void runProg(const char* command_line, String* output) {

	HANDLE g_hChildStd_OUT_Rd = NULL;
	HANDLE g_hChildStd_OUT_Wr = NULL;

	// Create a pipe to listen to the output:
	{
		SECURITY_ATTRIBUTES saAttr; 
		saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
		saAttr.bInheritHandle = TRUE; 
		saAttr.lpSecurityDescriptor = NULL; 
		if (!CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &saAttr, 0)) 
			abort();
		if (!SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0))
			abort(); 
	}

	// Run the compiler:
	{
		STARTUPINFO si;     
		PROCESS_INFORMATION pi;

		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		si.hStdError = g_hChildStd_OUT_Wr;
		si.hStdOutput = g_hChildStd_OUT_Wr;
		si.hStdInput = NULL;
		si.dwFlags |= STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
		si.wShowWindow = SW_HIDE;
		ZeroMemory(&pi, sizeof(pi));

		char command[2048];
		sprintf_s(command, sizeof(command), command_line);
		CreateProcess(
		NULL,     // Path
		command,  // Command line
		NULL,     // Process handle not inheritable
		NULL,     // Thread handle not inheritable
		TRUE,     // Handle inheritance
		0,        // No creation flags
		NULL,     // Use parent's environment block
		NULL,     // Use parent's starting directory 
		&si,      // Pointer to STARTUPINFO structure
		&pi       // Pointer to PROCESS_INFORMATION structure (removed extra parentheses)
		);

		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		CloseHandle(g_hChildStd_OUT_Wr);
	}

	// Read the output:
	{
		DWORD dwRead, dwWritten; 
		CHAR chBuf[4096]; 
		BOOL bSuccess = FALSE;
		while(true)  { 
			bSuccess = ReadFile(g_hChildStd_OUT_Rd, chBuf, sizeof(chBuf), &dwRead, NULL);
			output->append(chBuf, dwRead);
			if(!bSuccess || dwRead == 0 ) break;
		}
		CloseHandle(g_hChildStd_OUT_Rd);
	}
}

#else // Linux

#include <stdio.h>

void runProg(const char* command_line, String* output) {
	// use 2>&1 hack to force stderr to stdout
	FILE* out = popen(TempStr("%s 2>&1", command_line), "r");
	if (out) {
		char out_str[4096];
		while(fgets(out_str, sizeof(out_str), out))
			output->append(out_str);
		pclose(out);
	}
}

#endif