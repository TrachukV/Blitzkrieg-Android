// POSIX implementations of the handle-based Win32 file API.
#include "bk1_win32_fileio.h"
#include "bk1_win32_files.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <unistd.h>

namespace {

// A file handle is a HandleBase like the events and threads, so that the one
// CloseHandle in bk1_win32_platform.h disposes of every handle kind.
struct SFileHandle : NBk1Win32::SHandleBase
{
    int fd;

    explicit SFileHandle( int _fd ) : fd( _fd ) {}

    ~SFileHandle() override
    {
        if ( fd >= 0 )
            close( fd );
    }

    DWORD Wait( DWORD ) override { return WAIT_OBJECT_0; }
};

SFileHandle *AsFile( HANDLE h )
{
    return dynamic_cast<SFileHandle *>( NBk1Win32::FromHandle( h ) );
}

}   // anonymous namespace

extern "C" {

HANDLE CreateFileA( const char *pszName, DWORD dwAccess, DWORD /*dwShare*/,
                    LPSECURITY_ATTRIBUTES /*pSecurity*/, DWORD dwCreation,
                    DWORD /*dwFlags*/, HANDLE /*hTemplate*/ )
{
    if ( pszName == 0 )
        return INVALID_HANDLE_VALUE;

    // Everything the engine opens is named the Windows way; this is where that
    // becomes a path the kernel understands.
    const std::string szName = Bk1HostPath( pszName );
    pszName = szName.c_str();

    // A file that is about to appear must not stay hidden behind a remembered
    // directory listing.
    if ( dwCreation == CREATE_NEW || dwCreation == CREATE_ALWAYS ||
         dwCreation == OPEN_ALWAYS )
    {
        Bk1ForgetDirectory( pszName );
    }

    const bool bRead = ( dwAccess & GENERIC_READ ) != 0;
    const bool bWrite = ( dwAccess & GENERIC_WRITE ) != 0;

    int nFlags = bRead && bWrite ? O_RDWR : ( bWrite ? O_WRONLY : O_RDONLY );
    switch ( dwCreation )
    {
    case CREATE_NEW:        nFlags |= O_CREAT | O_EXCL; break;
    case CREATE_ALWAYS:     nFlags |= O_CREAT | O_TRUNC; break;
    case OPEN_ALWAYS:       nFlags |= O_CREAT; break;
    case TRUNCATE_EXISTING: nFlags |= O_TRUNC; break;
    case OPEN_EXISTING:
    default:                break;
    }

    const int fd = open( pszName, nFlags, 0666 );
    if ( fd < 0 )
        return INVALID_HANDLE_VALUE;
    return reinterpret_cast<HANDLE>( new SFileHandle( fd ) );
}

BOOL ReadFile( HANDLE hFile, LPVOID pBuffer, DWORD nToRead, DWORD *pnRead, LPVOID )
{
    SFileHandle *p = AsFile( hFile );
    if ( p == 0 || pBuffer == 0 )
        return FALSE;
    const ssize_t nRead = read( p->fd, pBuffer, nToRead );
    if ( nRead < 0 )
    {
        if ( pnRead != 0 )
            *pnRead = 0;
        return FALSE;
    }
    if ( pnRead != 0 )
        *pnRead = (DWORD)nRead;
    return TRUE;
}

BOOL WriteFile( HANDLE hFile, LPCVOID pBuffer, DWORD nToWrite, DWORD *pnWritten, LPVOID )
{
    SFileHandle *p = AsFile( hFile );
    if ( p == 0 || pBuffer == 0 )
        return FALSE;
    const ssize_t nWritten = write( p->fd, pBuffer, nToWrite );
    if ( nWritten < 0 )
    {
        if ( pnWritten != 0 )
            *pnWritten = 0;
        return FALSE;
    }
    if ( pnWritten != 0 )
        *pnWritten = (DWORD)nWritten;
    return TRUE;
}

DWORD SetFilePointer( HANDLE hFile, LONG nDistance, LONG *pnDistanceHigh, DWORD dwMethod )
{
    SFileHandle *p = AsFile( hFile );
    if ( p == 0 )
        return INVALID_SET_FILE_POINTER;

    off_t nOffset = nDistance;
    if ( pnDistanceHigh != 0 )
        nOffset = (off_t)( ( (long long)*pnDistanceHigh << 32 ) | (unsigned long)nDistance );

    int nWhence = SEEK_SET;
    if ( dwMethod == FILE_CURRENT )
        nWhence = SEEK_CUR;
    else if ( dwMethod == FILE_END )
        nWhence = SEEK_END;

    const off_t nNew = lseek( p->fd, nOffset, nWhence );
    if ( nNew == (off_t)-1 )
        return INVALID_SET_FILE_POINTER;
    if ( pnDistanceHigh != 0 )
        *pnDistanceHigh = (LONG)( (long long)nNew >> 32 );
    return (DWORD)( (unsigned long long)nNew & 0xFFFFFFFFull );
}

BOOL SetEndOfFile( HANDLE hFile )
{
    SFileHandle *p = AsFile( hFile );
    if ( p == 0 )
        return FALSE;
    const off_t nPos = lseek( p->fd, 0, SEEK_CUR );
    if ( nPos == (off_t)-1 )
        return FALSE;
    return ftruncate( p->fd, nPos ) == 0 ? TRUE : FALSE;
}

BOOL FlushFileBuffers( HANDLE hFile )
{
    SFileHandle *p = AsFile( hFile );
    if ( p == 0 )
        return FALSE;
    return fsync( p->fd ) == 0 ? TRUE : FALSE;
}

DWORD GetFileSize( HANDLE hFile, DWORD *pnSizeHigh )
{
    SFileHandle *p = AsFile( hFile );
    struct stat st;
    if ( p == 0 || fstat( p->fd, &st ) != 0 )
        return (DWORD)-1;
    if ( pnSizeHigh != 0 )
        *pnSizeHigh = (DWORD)( (unsigned long long)st.st_size >> 32 );
    return (DWORD)( (unsigned long long)st.st_size & 0xFFFFFFFFull );
}

BOOL GetFileInformationByHandle( HANDLE hFile, BY_HANDLE_FILE_INFORMATION *pInfo )
{
    SFileHandle *p = AsFile( hFile );
    struct stat st;
    if ( p == 0 || pInfo == 0 || fstat( p->fd, &st ) != 0 )
        return FALSE;

    memset( pInfo, 0, sizeof( *pInfo ) );
    pInfo->dwFileAttributes = S_ISDIR( st.st_mode ) ? FILE_ATTRIBUTE_DIRECTORY
                                                    : FILE_ATTRIBUTE_NORMAL;
    pInfo->ftCreationTime   = Bk1UnixTimeToFileTime( (long long)st.st_ctime );
    pInfo->ftLastAccessTime = Bk1UnixTimeToFileTime( (long long)st.st_atime );
    pInfo->ftLastWriteTime  = Bk1UnixTimeToFileTime( (long long)st.st_mtime );
    pInfo->nFileSizeHigh    = (DWORD)( (unsigned long long)st.st_size >> 32 );
    pInfo->nFileSizeLow     = (DWORD)( (unsigned long long)st.st_size & 0xFFFFFFFFull );
    pInfo->nNumberOfLinks   = (DWORD)st.st_nlink;
    pInfo->nFileIndexHigh   = (DWORD)( (unsigned long long)st.st_ino >> 32 );
    pInfo->nFileIndexLow    = (DWORD)( (unsigned long long)st.st_ino & 0xFFFFFFFFull );
    pInfo->dwVolumeSerialNumber = (DWORD)st.st_dev;
    return TRUE;
}

BOOL SetFileTime( HANDLE hFile, const FILETIME * /*pCreation*/,
                  const FILETIME *pLastAccess, const FILETIME *pLastWrite )
{
    // POSIX has no creation stamp to set, so that argument is ignored.
    SFileHandle *p = AsFile( hFile );
    struct stat st;
    if ( p == 0 || fstat( p->fd, &st ) != 0 )
        return FALSE;

    struct timespec times[2];
    times[0].tv_nsec = 0;
    times[1].tv_nsec = 0;
    times[0].tv_sec = pLastAccess != 0 ? (time_t)Bk1FileTimeToUnixTime( pLastAccess )
                                       : st.st_atime;
    times[1].tv_sec = pLastWrite != 0 ? (time_t)Bk1FileTimeToUnixTime( pLastWrite )
                                      : st.st_mtime;
    return futimens( p->fd, times ) == 0 ? TRUE : FALSE;
}

HANDLE GetCurrentProcess( void )
{
    // A pseudo handle, as on Windows: DuplicateHandle below ignores it.
    return INVALID_HANDLE_VALUE;
}

BOOL DuplicateHandle( HANDLE, HANDLE hSource, HANDLE, HANDLE *phTarget,
                      DWORD, BOOL, DWORD )
{
    SFileHandle *p = AsFile( hSource );
    if ( p == 0 || phTarget == 0 )
        return FALSE;
    const int fd = dup( p->fd );
    if ( fd < 0 )
        return FALSE;
    *phTarget = reinterpret_cast<HANDLE>( new SFileHandle( fd ) );
    return TRUE;
}

DWORD GetFullPathNameA( const char *pszFileName, DWORD nBufferLength,
                        char *pszBuffer, char **ppszFilePart )
{
    if ( pszFileName == 0 || pszBuffer == 0 )
        return 0;
    const std::string szTranslated = Bk1HostPath( pszFileName );
    pszFileName = szTranslated.c_str();

    char szResolved[PATH_MAX];
    if ( pszFileName[0] == '/' )
    {
        snprintf( szResolved, sizeof( szResolved ), "%s", pszFileName );
    }
    else
    {
        char szCwd[PATH_MAX];
        if ( getcwd( szCwd, sizeof( szCwd ) ) == 0 )
            return 0;
        snprintf( szResolved, sizeof( szResolved ), "%s/%s", szCwd, pszFileName );
    }

    const size_t nLen = strlen( szResolved );
    if ( nLen + 1 > nBufferLength )
        return (DWORD)( nLen + 1 );        // required size, as on Windows

    memcpy( pszBuffer, szResolved, nLen + 1 );
    if ( ppszFilePart != 0 )
    {
        char *pszSlash = strrchr( pszBuffer, '/' );
        *ppszFilePart = pszSlash != 0 ? pszSlash + 1 : pszBuffer;
    }
    return (DWORD)nLen;
}

BOOL GetDiskFreeSpaceA( const char *pszRoot, DWORD *pnSectorsPerCluster,
                        DWORD *pnBytesPerSector, DWORD *pnFreeClusters,
                        DWORD *pnTotalClusters )
{
    struct statvfs vfs;
    if ( statvfs( pszRoot != 0 ? pszRoot : "/", &vfs ) != 0 )
        return FALSE;
    if ( pnSectorsPerCluster != 0 )
        *pnSectorsPerCluster = 1;
    if ( pnBytesPerSector != 0 )
        *pnBytesPerSector = (DWORD)vfs.f_frsize;
    if ( pnFreeClusters != 0 )
        *pnFreeClusters = (DWORD)vfs.f_bavail;
    if ( pnTotalClusters != 0 )
        *pnTotalClusters = (DWORD)vfs.f_blocks;
    return TRUE;
}

DWORD GetLogicalDriveStringsA( DWORD nBufferLength, char *pszBuffer )
{
    // The Windows form is a run of NUL-terminated roots ending in a second
    // NUL. There is one root here.
    static const char szRoot[] = "/\0";
    const DWORD nNeeded = (DWORD)sizeof( szRoot );
    if ( pszBuffer == 0 || nBufferLength < nNeeded )
        return nNeeded;
    memcpy( pszBuffer, szRoot, nNeeded );
    return nNeeded - 1;
}

UINT GetDriveTypeA( const char * )
{
    return DRIVE_FIXED;
}

HMODULE GetModuleHandleA( const char * )
{
    // The engine only compares the result against zero.
    return (HMODULE)(void *)1;
}

}   // extern "C"
