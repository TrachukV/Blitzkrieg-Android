#pragma once
// The Win32 file and directory calls Misc/FileUtils.cpp is written against,
// backed by POSIX. Directory scans fold case, because the game's data was
// authored on a case-insensitive filesystem and Android's is not.
#include "bk1_win32_types.h"

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#define FILE_ATTRIBUTE_READONLY   0x00000001
#define FILE_ATTRIBUTE_HIDDEN     0x00000002
#define FILE_ATTRIBUTE_SYSTEM     0x00000004
#define FILE_ATTRIBUTE_DIRECTORY  0x00000010
#define FILE_ATTRIBUTE_ARCHIVE    0x00000020
#define FILE_ATTRIBUTE_NORMAL     0x00000080
#define INVALID_FILE_ATTRIBUTES   ( (DWORD)-1 )

typedef struct _FILETIME {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
} FILETIME, *PFILETIME, *LPFILETIME;

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

// StreamIO/FileAttribs.h converts between file times and the FAT date/time
// pair the archive formats store.
BOOL FileTimeToLocalFileTime( const FILETIME *pIn, FILETIME *pOut );
BOOL LocalFileTimeToFileTime( const FILETIME *pIn, FILETIME *pOut );
BOOL FileTimeToDosDateTime( const FILETIME *pIn, WORD *pFatDate, WORD *pFatTime );
BOOL DosDateTimeToFileTime( WORD nFatDate, WORD nFatTime, FILETIME *pOut );

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
