#pragma once
// The handle-based Win32 file API. File handles are the same opaque HANDLE as
// the events and threads in bk1_win32_platform.h, so CloseHandle closes them
// all through the one virtual destructor.
#include "bk1_win32_files.h"
#include "bk1_win32_platform.h"

#ifndef _MAX_PATH
#define _MAX_PATH MAX_PATH
#endif

#define GENERIC_READ              0x80000000u
#define GENERIC_WRITE             0x40000000u

#define FILE_SHARE_READ           0x00000001u
#define FILE_SHARE_WRITE          0x00000002u
#define FILE_SHARE_DELETE         0x00000004u

#define CREATE_NEW                1
#define CREATE_ALWAYS             2
#define OPEN_EXISTING             3
#define OPEN_ALWAYS               4
#define TRUNCATE_EXISTING         5

#define FILE_BEGIN                0
#define FILE_CURRENT              1
#define FILE_END                  2

#define INVALID_SET_FILE_POINTER  ( (DWORD)-1 )
#define DUPLICATE_SAME_ACCESS     0x00000002u

typedef struct _SECURITY_ATTRIBUTES {
    DWORD  nLength;
    LPVOID lpSecurityDescriptor;
    BOOL   bInheritHandle;
} SECURITY_ATTRIBUTES, *PSECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;

typedef union _ULARGE_INTEGER {
    struct { DWORD LowPart; DWORD HighPart; };
    unsigned long long QuadPart;
} ULARGE_INTEGER, *PULARGE_INTEGER;

typedef struct _BY_HANDLE_FILE_INFORMATION {
    DWORD    dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD    dwVolumeSerialNumber;
    DWORD    nFileSizeHigh;
    DWORD    nFileSizeLow;
    DWORD    nNumberOfLinks;
    DWORD    nFileIndexHigh;
    DWORD    nFileIndexLow;
} BY_HANDLE_FILE_INFORMATION, *PBY_HANDLE_FILE_INFORMATION;

#ifdef __cplusplus
extern "C" {
#endif

HANDLE CreateFileA( const char *pszName, DWORD dwAccess, DWORD dwShare,
                    LPSECURITY_ATTRIBUTES pSecurity, DWORD dwCreation,
                    DWORD dwFlags, HANDLE hTemplate );
BOOL  ReadFile( HANDLE hFile, LPVOID pBuffer, DWORD nToRead, DWORD *pnRead, LPVOID pOverlapped );
BOOL  WriteFile( HANDLE hFile, LPCVOID pBuffer, DWORD nToWrite, DWORD *pnWritten, LPVOID pOverlapped );
DWORD SetFilePointer( HANDLE hFile, LONG nDistance, LONG *pnDistanceHigh, DWORD dwMethod );
BOOL  SetEndOfFile( HANDLE hFile );
BOOL  FlushFileBuffers( HANDLE hFile );
DWORD GetFileSize( HANDLE hFile, DWORD *pnSizeHigh );
BOOL  GetFileInformationByHandle( HANDLE hFile, BY_HANDLE_FILE_INFORMATION *pInfo );
BOOL  SetFileTime( HANDLE hFile, const FILETIME *pCreation,
                   const FILETIME *pLastAccess, const FILETIME *pLastWrite );

HANDLE GetCurrentProcess( void );
BOOL   DuplicateHandle( HANDLE hSourceProcess, HANDLE hSource, HANDLE hTargetProcess,
                        HANDLE *phTarget, DWORD dwAccess, BOOL bInherit, DWORD dwOptions );

DWORD  GetFullPathNameA( const char *pszFileName, DWORD nBufferLength,
                         char *pszBuffer, char **ppszFilePart );
BOOL   GetDiskFreeSpaceA( const char *pszRoot, DWORD *pnSectorsPerCluster,
                          DWORD *pnBytesPerSector, DWORD *pnFreeClusters,
                          DWORD *pnTotalClusters );
HMODULE GetModuleHandleA( const char *pszModule );

#ifdef __cplusplus
}
#endif

// Drive enumeration. StreamIO/RandomGenInternal.cpp stirs the volume layout
// into its seed; Android has one filesystem, so that is what it reports.
#define DRIVE_UNKNOWN     0
#define DRIVE_NO_ROOT_DIR 1
#define DRIVE_REMOVABLE   2
#define DRIVE_FIXED       3
#define DRIVE_REMOTE      4
#define DRIVE_CDROM       5
#define DRIVE_RAMDISK     6

#ifdef __cplusplus
extern "C" {
#endif
DWORD GetLogicalDriveStringsA( DWORD nBufferLength, char *pszBuffer );
UINT  GetDriveTypeA( const char *pszRoot );
#ifdef __cplusplus
}
#endif

#define GetLogicalDriveStrings GetLogicalDriveStringsA
#define GetDriveType           GetDriveTypeA

#define CreateFile      CreateFileA
#define GetFullPathName GetFullPathNameA
#define GetDiskFreeSpace GetDiskFreeSpaceA
#define GetModuleHandle GetModuleHandleA

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

inline char *_fullpath( char *pszAbs, const char *pszRel, size_t nMax )
{
    // MSVC resolves relative paths against the cwd; realpath is the equivalent
    // but requires the file to exist, so fall back to a plain join.
    if ( pszRel == 0 || pszAbs == 0 )
        return 0;
    if ( realpath( pszRel, pszAbs ) != 0 )
        return pszAbs;
    if ( pszRel[0] == '/' )
    {
        snprintf( pszAbs, nMax, "%s", pszRel );
        return pszAbs;
    }
    char szCwd[4096];
    if ( getcwd( szCwd, sizeof( szCwd ) ) == 0 )
        return 0;
    snprintf( pszAbs, nMax, "%s/%s", szCwd, pszRel );
    return pszAbs;
}

