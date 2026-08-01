#pragma once
// The Win32 file and directory calls Misc/FileUtils.cpp is written against,
// backed by POSIX. Directory scans fold case, because the game's data was
// authored on a case-insensitive filesystem and Android's is not.
#include "bk1_win32_types.h"

#include <string>

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#define FILE_ATTRIBUTE_READONLY   0x00000001
#define FILE_ATTRIBUTE_HIDDEN     0x00000002
#define FILE_ATTRIBUTE_SYSTEM     0x00000004
#define FILE_ATTRIBUTE_DIRECTORY  0x00000010
#define FILE_ATTRIBUTE_ARCHIVE    0x00000020
#define FILE_ATTRIBUTE_NORMAL     0x00000080
#define FILE_ATTRIBUTE_TEMPORARY  0x00000100
#define FILE_ATTRIBUTE_COMPRESSED 0x00000800
#define INVALID_FILE_ATTRIBUTES   ( (DWORD)-1 )

typedef struct _FILETIME {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
} FILETIME, *PFILETIME, *LPFILETIME;

// The broken-out calendar form of a file time. GameTT/Common.cpp reads the
// fields by name, so the order and spelling match Windows.
typedef struct _SYSTEMTIME {
    WORD wYear;
    WORD wMonth;
    WORD wDayOfWeek;
    WORD wDay;
    WORD wHour;
    WORD wMinute;
    WORD wSecond;
    WORD wMilliseconds;
} SYSTEMTIME, *PSYSTEMTIME, *LPSYSTEMTIME;

typedef struct _WIN32_FIND_DATAA {
    DWORD    dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD    nFileSizeHigh;
    DWORD    nFileSizeLow;
    DWORD    dwReserved0;
    DWORD    dwReserved1;
    CHAR     cFileName[MAX_PATH];
    CHAR     cAlternateFileName[14];
} WIN32_FIND_DATAA, WIN32_FIND_DATA, *PWIN32_FIND_DATA, *LPWIN32_FIND_DATA;

#ifdef __cplusplus
extern "C" {
#endif

HANDLE FindFirstFileA( const char *pszMask, WIN32_FIND_DATAA *pData );
BOOL   FindNextFileA( HANDLE hFind, WIN32_FIND_DATAA *pData );
BOOL   FindClose( HANDLE hFind );

DWORD  GetFileAttributesA( const char *pszPath );
BOOL   SetFileAttributesA( const char *pszPath, DWORD dwAttributes );
BOOL   MoveFileA( const char *pszFrom, const char *pszTo );
BOOL   DeleteFileA( const char *pszPath );
BOOL   CopyFileA( const char *pszFrom, const char *pszTo, BOOL bFailIfExists );
BOOL   CreateDirectoryA( const char *pszPath, LPVOID pSecurity );
BOOL   RemoveDirectoryA( const char *pszPath );
DWORD  GetCurrentDirectoryA( DWORD nSize, char *pszBuffer );
BOOL   SetCurrentDirectoryA( const char *pszPath );

// Applied to a path rather than to a handle: the engine's SetFileTime takes a
// file name. A null pointer leaves that stamp alone, as on Windows.
BOOL Bk1SetFileTimeByPath( const char *pszPath, const FILETIME *pCreation,
                           const FILETIME *pLastAccess, const FILETIME *pLastWrite );

// FILETIME is 100-nanosecond ticks since 1601; time_t is seconds since 1970.
FILETIME Bk1UnixTimeToFileTime( long long nUnixSeconds );
long long Bk1FileTimeToUnixTime( const FILETIME *pTime );

// Win32's three-way file time comparison: -1, 0 or +1 for the first time being
// before, equal to, or after the second. GameTT/SaveLoadCommon.h orders saved
// games by it, and MapEditor and buildversion compare timestamps with it too.
LONG CompareFileTime( const FILETIME *pFirst, const FILETIME *pSecond );

// StreamIO/FileAttribs.h converts between file times and the FAT date/time
// pair the archive formats store.
BOOL FileTimeToLocalFileTime( const FILETIME *pIn, FILETIME *pOut );
BOOL LocalFileTimeToFileTime( const FILETIME *pIn, FILETIME *pOut );
BOOL FileTimeToDosDateTime( const FILETIME *pIn, WORD *pFatDate, WORD *pFatTime );
BOOL DosDateTimeToFileTime( WORD nFatDate, WORD nFatTime, FILETIME *pOut );

// GameTT/Common.cpp formats a file's change stamp for display. It converts to
// local time first, as on Windows, so this reads the value exactly as given.
BOOL FileTimeToSystemTime( const FILETIME *pIn, SYSTEMTIME *pOut );

#ifdef __cplusplus
}
#endif

#define FindFirstFile     FindFirstFileA
#define FindNextFile      FindNextFileA
#define GetFileAttributes GetFileAttributesA
#define SetFileAttributes SetFileAttributesA
#define MoveFile          MoveFileA
#define DeleteFile        DeleteFileA
#define CopyFile          CopyFileA
#define CreateDirectory   CreateDirectoryA
#define RemoveDirectory   RemoveDirectoryA
#define GetCurrentDirectory GetCurrentDirectoryA
#define SetCurrentDirectory SetCurrentDirectoryA

#ifdef __cplusplus
// The one place the engine's world meets the filesystem's.
//
// Every path inside Blitzkrieg is written the Windows way -- "data\scenarios",
// "movies\intro" -- and more than twenty places in the engine take those apart
// by searching for a backslash. Teaching all of them about the other separator
// would be twenty chances to get it wrong, and several of those paths are
// engine-internal keys rather than filesystem paths, so changing them would be
// wrong even where it worked.
//
// The engine keeps its backslashes. They are translated here instead, at the
// boundary every path crosses exactly once. A path that already uses forward
// slashes -- one that came from Android rather than from the game data -- goes
// through unchanged.
//
// Outside the extern "C" block above because it returns a std::string.
std::string Bk1HostPath( const char *pszPath );

// Directory listings are remembered while resolving case. Anything that
// creates or removes a file has to say so, or the new name stays invisible.
void Bk1ForgetDirectory( const char *pszPath );
#endif
