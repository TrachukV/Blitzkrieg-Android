#pragma once
// Process creation. RandomMapGen's ExecuteProcess shells out to the map
// building tools; the game itself does not, but the code has to build and
// behave sanely when it is called.
//
// The handle CreateProcess returns is the same waitable HANDLE as an event or
// a thread, so WaitForSingleObject on it waits for the child to exit and
// CloseHandle reaps it.
#include "bk1_win32_platform.h"

typedef struct _PROCESS_INFORMATION {
    HANDLE hProcess;
    HANDLE hThread;
    DWORD  dwProcessId;
    DWORD  dwThreadId;
} PROCESS_INFORMATION, *LPPROCESS_INFORMATION;

typedef struct _STARTUPINFOA {
    DWORD  cb;
    char  *lpReserved;
    char  *lpDesktop;
    char  *lpTitle;
    DWORD  dwX;
    DWORD  dwY;
    DWORD  dwXSize;
    DWORD  dwYSize;
    DWORD  dwXCountChars;
    DWORD  dwYCountChars;
    DWORD  dwFillAttribute;
    DWORD  dwFlags;
    WORD   wShowWindow;
    WORD   cbReserved2;
    BYTE  *lpReserved2;
    HANDLE hStdInput;
    HANDLE hStdOutput;
    HANDLE hStdError;
} STARTUPINFOA, STARTUPINFO, *LPSTARTUPINFOA, *LPSTARTUPINFO;

#define STILL_ACTIVE 259

#ifdef __cplusplus
extern "C" {
#endif

BOOL CreateProcessA( const char *pszApplicationName, char *pszCommandLine,
                     void *pProcessAttributes, void *pThreadAttributes,
                     BOOL bInheritHandles, DWORD dwCreationFlags,
                     void *pEnvironment, const char *pszCurrentDirectory,
                     STARTUPINFOA *pStartupInfo,
                     PROCESS_INFORMATION *pProcessInformation );

BOOL GetExitCodeProcess( HANDLE hProcess, DWORD *pdwExitCode );
BOOL TerminateProcess( HANDLE hProcess, UINT uExitCode );
DWORD GetCurrentProcessId( void );

#ifdef __cplusplus
}
#endif

#define CreateProcess CreateProcessA
