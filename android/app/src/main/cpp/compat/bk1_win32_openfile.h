#pragma once
// The 16-bit-era OpenFile family. StreamIO/RandomGenInternal.cpp uses it to
// read a few bytes from an arbitrary file as an entropy source, then passes
// the result to SetFilePointer and ReadFile as a HANDLE -- so HFILE has to be
// the same handle the rest of the file API understands.
#include "bk1_win32_fileio.h"

#include <string.h>

typedef HANDLE HFILE;

#define HFILE_ERROR ( (HFILE)INVALID_HANDLE_VALUE )

#define OF_READ                 0x00000000
#define OF_WRITE                0x00000001
#define OF_READWRITE            0x00000002
#define OF_SHARE_COMPAT         0x00000000
#define OF_SHARE_EXCLUSIVE      0x00000010
#define OF_SHARE_DENY_WRITE     0x00000020
#define OF_SHARE_DENY_READ      0x00000030
#define OF_SHARE_DENY_NONE      0x00000040
#define OF_EXIST                0x00004000

#define OFS_MAXPATHNAME 128

typedef struct _OFSTRUCT {
    BYTE cBytes;
    BYTE fFixedDisk;
    WORD nErrCode;
    WORD Reserved1;
    WORD Reserved2;
    CHAR szPathName[OFS_MAXPATHNAME];
} OFSTRUCT, *POFSTRUCT, *LPOFSTRUCT;

inline HFILE OpenFile( const char *pszName, OFSTRUCT *pReOpenBuff, UINT uStyle )
{
    if ( pszName == 0 )
        return HFILE_ERROR;

    if ( pReOpenBuff != 0 )
    {
        pReOpenBuff->nErrCode = 0;
        snprintf( pReOpenBuff->szPathName, OFS_MAXPATHNAME, "%s", pszName );
    }

    DWORD dwAccess = GENERIC_READ;
    if ( ( uStyle & OF_READWRITE ) == OF_READWRITE )
        dwAccess = GENERIC_READ | GENERIC_WRITE;
    else if ( ( uStyle & OF_WRITE ) != 0 )
        dwAccess = GENERIC_WRITE;

    const HANDLE h = CreateFileA( pszName, dwAccess, 0, 0, OPEN_EXISTING, 0, 0 );
    return h == INVALID_HANDLE_VALUE ? HFILE_ERROR : (HFILE)h;
}

inline HFILE _lclose( HFILE hFile )
{
    return CloseHandle( hFile ) != FALSE ? (HFILE)0 : HFILE_ERROR;
}
