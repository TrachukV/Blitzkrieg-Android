#pragma once
// Stands in for the MSVC low-level I/O header. The engine only reaches for a
// handful of these: '_access', '_filelength' and '_fileno'.
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "bk1_win32_files.h"

#ifndef F_OK
#define F_OK 0
#endif

// Through the path boundary, not straight to the kernel.
// CFileSystem::IsStreamExist lowercases the whole path before asking whether a
// file exists -- free on Windows, and on Android it turns "/Android/" into a
// directory that is not there. Bk1HostPath resolves the real spelling.
inline int _access( const char *pszPath, int nMode )
{
    return access( Bk1HostPath( pszPath ).c_str(), nMode );
}
inline int _fileno( FILE *pFile ) { return fileno( pFile ); }
inline int _close( int fd ) { return close( fd ); }
inline int _read( int fd, void *pBuf, unsigned int nCount ) { return (int)read( fd, pBuf, nCount ); }
inline int _write( int fd, const void *pBuf, unsigned int nCount ) { return (int)write( fd, pBuf, nCount ); }
inline long _lseek( int fd, long nOffset, int nOrigin ) { return (long)lseek( fd, nOffset, nOrigin ); }
inline int _unlink( const char *pszPath ) { return unlink( pszPath ); }

inline long _filelength( int fd )
{
    struct stat st;
    if ( fstat( fd, &st ) != 0 )
        return -1L;
    return (long)st.st_size;
}
