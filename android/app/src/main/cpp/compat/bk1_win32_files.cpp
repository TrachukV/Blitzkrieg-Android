// POSIX implementations of the Win32 file calls declared in bk1_win32_files.h.
#include "bk1_win32_files.h"

#include <dirent.h>
#include <fnmatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <ctype.h>

#include <map>
#include <string>
#include <vector>

namespace {

// 1601-01-01 to 1970-01-01 in seconds.
const long long EPOCH_DELTA = 11644473600LL;
const long long TICKS_PER_SECOND = 10000000LL;

struct SFindHandle
{
    DIR *pDir;
    std::string szDirectory;
    std::string szPattern;

    SFindHandle() : pDir( 0 ) {}
    ~SFindHandle() { if ( pDir != 0 ) closedir( pDir ); }
};

}   // anonymous namespace

namespace {

// Case folding, and why a path needs it.
//
// The game's data was authored on Windows, where the filesystem does not care
// about case, and the engine relies on that far more than it looks. It builds
// paths by concatenating names out of XML, and CFileSystem::IsStreamExist even
// lowercases the whole path before asking whether it exists -- harmless on
// Windows, fatal here, because the path it is handed contains "/Android/" and
// the lowercase spelling of that directory does not exist.
//
// So a path is resolved a component at a time: if a name is there verbatim it
// is used, and if it is not, its parent directory is read and matched without
// regard to case. That reproduces what Windows does, at the one boundary every
// path crosses.
//
// Directory listings are remembered, because the engine asks about tens of
// thousands of files and a scan per lookup would make loading crawl.
std::map<std::string, std::vector<std::string> > g_directoryCache;

bool EqualsNoCase( const std::string &a, const std::string &b )
{
    if ( a.size() != b.size() )
        return false;
    for ( size_t i = 0; i < a.size(); ++i )
    {
        if ( tolower( (unsigned char)a[i] ) != tolower( (unsigned char)b[i] ) )
            return false;
    }
    return true;
}

// The real spelling of pszName inside szDirectory, or empty if nothing matches.
std::string MatchInDirectory( const std::string &szDirectory, const std::string &szName )
{
    std::map<std::string, std::vector<std::string> >::iterator it =
        g_directoryCache.find( szDirectory );
    if ( it == g_directoryCache.end() )
    {
        std::vector<std::string> entries;
        if ( DIR *pDir = opendir( szDirectory.empty() ? "/" : szDirectory.c_str() ) )
        {
            while ( struct dirent *pEnt = readdir( pDir ) )
                entries.push_back( pEnt->d_name );
            closedir( pDir );
        }
        it = g_directoryCache.insert( std::make_pair( szDirectory, entries ) ).first;
    }
    for ( size_t i = 0; i < it->second.size(); ++i )
    {
        if ( EqualsNoCase( it->second[i], szName ) )
            return it->second[i];
    }
    return std::string();
}

}   // anonymous namespace

void Bk1ForgetDirectory( const char *pszPath )
{
    // A directory that has just been written to must be read again.
    if ( pszPath == 0 )
    {
        g_directoryCache.clear();
        return;
    }
    std::string szPath = Bk1HostPath( pszPath );
    const size_t nSlash = szPath.rfind( '/' );
    g_directoryCache.erase( ( nSlash == std::string::npos ) ? std::string()
                                                            : szPath.substr( 0, nSlash ) );
}

std::string Bk1HostPath( const char *pszPath )
{
    if ( pszPath == 0 )
        return std::string();

    std::string szResult( pszPath );
    for ( size_t i = 0; i < szResult.size(); ++i )
    {
        if ( szResult[i] == '\\' )
            szResult[i] = '/';
    }

    // Present as given? Then nothing else is needed, which is the common case
    // and costs one stat.
    struct stat st;
    if ( stat( szResult.c_str(), &st ) == 0 )
        return szResult;

    // Otherwise walk it, folding case where a component does not match.
    const bool bAbsolute = !szResult.empty() && szResult[0] == '/';
    std::string szBuilt = bAbsolute ? "" : ".";
    size_t nStart = bAbsolute ? 1 : 0;

    while ( nStart <= szResult.size() )
    {
        size_t nEnd = szResult.find( '/', nStart );
        if ( nEnd == std::string::npos )
            nEnd = szResult.size();
        const std::string szComponent = szResult.substr( nStart, nEnd - nStart );

        if ( !szComponent.empty() && szComponent != "." )
        {
            const std::string szCandidate = szBuilt + "/" + szComponent;
            if ( stat( szCandidate.c_str(), &st ) == 0 )
            {
                szBuilt = szCandidate;
            }
            else
            {
                const std::string szReal = MatchInDirectory( szBuilt, szComponent );
                if ( szReal.empty() )
                {
                    // Nothing matches. Hand back the rest unchanged -- the
                    // caller is about to fail, and it should fail against the
                    // name it asked for.
                    return szBuilt + "/" + szResult.substr( nStart );
                }
                szBuilt += "/" + szReal;
            }
        }

        if ( nEnd == szResult.size() )
            break;
        nStart = nEnd + 1;
    }
    return szBuilt;
}

namespace {

void SplitMask( const char *pszMask, std::string *pDir, std::string *pPattern )
{
    const char *pszSlash = strrchr( pszMask, '/' );
    const char *pszBack = strrchr( pszMask, '\\' );
    const char *pszSep = pszSlash > pszBack ? pszSlash : pszBack;
    if ( pszSep == 0 )
    {
        *pDir = ".";
        *pPattern = pszMask;
        return;
    }
    pDir->assign( pszMask, pszSep - pszMask );
    if ( pDir->empty() )
        *pDir = "/";
    *pPattern = pszSep + 1;
    // the engine writes Windows separators; normalise the directory part
    for ( size_t i = 0; i < pDir->size(); ++i )
    {
        if ( ( *pDir )[i] == '\\' )
            ( *pDir )[i] = '/';
    }
}

bool Fill( const std::string &szDir, const char *pszName, WIN32_FIND_DATAA *pData )
{
    std::string szFull = szDir;
    szFull += "/";
    szFull += pszName;

    struct stat st;
    if ( stat( szFull.c_str(), &st ) != 0 )
        return false;

    memset( pData, 0, sizeof( *pData ) );
    pData->dwFileAttributes = S_ISDIR( st.st_mode ) ? FILE_ATTRIBUTE_DIRECTORY
                                                    : FILE_ATTRIBUTE_NORMAL;
    if ( ( st.st_mode & S_IWUSR ) == 0 )
        pData->dwFileAttributes |= FILE_ATTRIBUTE_READONLY;
    pData->ftCreationTime   = Bk1UnixTimeToFileTime( (long long)st.st_ctime );
    pData->ftLastAccessTime = Bk1UnixTimeToFileTime( (long long)st.st_atime );
    pData->ftLastWriteTime  = Bk1UnixTimeToFileTime( (long long)st.st_mtime );
    pData->nFileSizeLow  = (DWORD)( (unsigned long long)st.st_size & 0xFFFFFFFFu );
    pData->nFileSizeHigh = (DWORD)( (unsigned long long)st.st_size >> 32 );
    snprintf( pData->cFileName, MAX_PATH, "%s", pszName );
    return true;
}

// FNM_CASEFOLD keeps Windows' case-insensitive matching, which the game data
// depends on.
//
// "*.*" is the one pattern where fnmatch and Windows disagree, and the
// disagreement is not academic. fnmatch reads it literally -- something, a dot,
// something -- so a name with no dot in it does not match. Windows has treated
// "*.*" as "everything" since MS-DOS, extension or no extension, and the engine
// leans on that: CCommonFileSystem enumerates a directory with "*.*" and
// recurses into whatever comes back marked as a directory.
//
// Directories rarely have dots. So on Android the walk never descended, and
// every list built by scanning a tree came up empty -- which is why the
// Tutorials screen listed nothing while six tutorials sat in the data.
bool Matches( const std::string &szPattern, const char *pszName )
{
    if ( szPattern == "*.*" )
        return true;
    return fnmatch( szPattern.c_str(), pszName, FNM_CASEFOLD ) == 0;
}

bool NextMatch( SFindHandle *pFind, WIN32_FIND_DATAA *pData )
{
    for ( struct dirent *pEnt = readdir( pFind->pDir ); pEnt != 0;
          pEnt = readdir( pFind->pDir ) )
    {
        if ( strcmp( pEnt->d_name, "." ) == 0 || strcmp( pEnt->d_name, ".." ) == 0 )
            continue;
        if ( !Matches( pFind->szPattern, pEnt->d_name ) )
            continue;
        if ( Fill( pFind->szDirectory, pEnt->d_name, pData ) )
            return true;
    }
    return false;
}

}   // anonymous namespace

extern "C" {

FILETIME Bk1UnixTimeToFileTime( long long nUnixSeconds )
{
    const unsigned long long nTicks =
        (unsigned long long)( ( nUnixSeconds + EPOCH_DELTA ) * TICKS_PER_SECOND );
    FILETIME ft;
    ft.dwLowDateTime  = (DWORD)( nTicks & 0xFFFFFFFFull );
    ft.dwHighDateTime = (DWORD)( nTicks >> 32 );
    return ft;
}

long long Bk1FileTimeToUnixTime( const FILETIME *pTime )
{
    if ( pTime == 0 )
        return 0;
    const unsigned long long nTicks =
        ( (unsigned long long)pTime->dwHighDateTime << 32 ) | pTime->dwLowDateTime;
    return (long long)( nTicks / TICKS_PER_SECOND ) - EPOCH_DELTA;
}

LONG CompareFileTime( const FILETIME *pFirst, const FILETIME *pSecond )
{
    if ( pFirst == 0 || pSecond == 0 )
        return 0;
    // The pair is one unsigned 64-bit tick count, high word first.
    const unsigned long long nFirst =
        ( (unsigned long long)pFirst->dwHighDateTime << 32 ) | pFirst->dwLowDateTime;
    const unsigned long long nSecond =
        ( (unsigned long long)pSecond->dwHighDateTime << 32 ) | pSecond->dwLowDateTime;
    if ( nFirst < nSecond )
        return -1;
    if ( nFirst > nSecond )
        return 1;
    return 0;
}

BOOL FileTimeToLocalFileTime( const FILETIME *pIn, FILETIME *pOut )
{
    if ( pIn == 0 || pOut == 0 )
        return FALSE;
    const time_t nUnix = (time_t)Bk1FileTimeToUnixTime( pIn );
    struct tm local;
    if ( localtime_r( &nUnix, &local ) == 0 )
        return FALSE;
    *pOut = Bk1UnixTimeToFileTime( (long long)nUnix + local.tm_gmtoff );
    return TRUE;
}

BOOL LocalFileTimeToFileTime( const FILETIME *pIn, FILETIME *pOut )
{
    if ( pIn == 0 || pOut == 0 )
        return FALSE;
    const long long nLocal = Bk1FileTimeToUnixTime( pIn );
    time_t nProbe = (time_t)nLocal;
    struct tm local;
    if ( localtime_r( &nProbe, &local ) == 0 )
        return FALSE;
    *pOut = Bk1UnixTimeToFileTime( nLocal - local.tm_gmtoff );
    return TRUE;
}

BOOL FileTimeToDosDateTime( const FILETIME *pIn, WORD *pFatDate, WORD *pFatTime )
{
    if ( pIn == 0 || pFatDate == 0 || pFatTime == 0 )
        return FALSE;
    const time_t nUnix = (time_t)Bk1FileTimeToUnixTime( pIn );
    struct tm t;
    if ( gmtime_r( &nUnix, &t ) == 0 )
        return FALSE;
    // FAT epoch is 1980; seconds are stored in two-second units.
    const int nYear = t.tm_year + 1900 - 1980;
    if ( nYear < 0 )
        return FALSE;
    *pFatDate = (WORD)( ( nYear << 9 ) | ( ( t.tm_mon + 1 ) << 5 ) | t.tm_mday );
    *pFatTime = (WORD)( ( t.tm_hour << 11 ) | ( t.tm_min << 5 ) | ( t.tm_sec / 2 ) );
    return TRUE;
}

BOOL DosDateTimeToFileTime( WORD nFatDate, WORD nFatTime, FILETIME *pOut )
{
    if ( pOut == 0 )
        return FALSE;
    struct tm t;
    memset( &t, 0, sizeof( t ) );
    t.tm_year = ( ( nFatDate >> 9 ) & 0x7F ) + 1980 - 1900;
    t.tm_mon  = ( ( nFatDate >> 5 ) & 0x0F ) - 1;
    t.tm_mday = nFatDate & 0x1F;
    t.tm_hour = ( nFatTime >> 11 ) & 0x1F;
    t.tm_min  = ( nFatTime >> 5 ) & 0x3F;
    t.tm_sec  = ( nFatTime & 0x1F ) * 2;
    const time_t nUnix = timegm( &t );
    if ( nUnix == (time_t)-1 )
        return FALSE;
    *pOut = Bk1UnixTimeToFileTime( (long long)nUnix );
    return TRUE;
}

BOOL FileTimeToSystemTime( const FILETIME *pIn, SYSTEMTIME *pOut )
{
    if ( pIn == 0 || pOut == 0 )
        return FALSE;
    const time_t nUnix = (time_t)Bk1FileTimeToUnixTime( pIn );
    struct tm t;
    if ( gmtime_r( &nUnix, &t ) == 0 )
        return FALSE;
    const unsigned long long nTicks =
        ( (unsigned long long)pIn->dwHighDateTime << 32 ) | pIn->dwLowDateTime;
    pOut->wYear         = (WORD)( t.tm_year + 1900 );
    pOut->wMonth        = (WORD)( t.tm_mon + 1 );
    pOut->wDayOfWeek    = (WORD)t.tm_wday;
    pOut->wDay          = (WORD)t.tm_mday;
    pOut->wHour         = (WORD)t.tm_hour;
    pOut->wMinute       = (WORD)t.tm_min;
    pOut->wSecond       = (WORD)t.tm_sec;
    pOut->wMilliseconds = (WORD)( ( nTicks % (unsigned long long)TICKS_PER_SECOND ) / 10000ull );
    return TRUE;
}

HANDLE FindFirstFileA( const char *pszMask, WIN32_FIND_DATAA *pData )
{
    if ( pszMask == 0 || pData == 0 )
        return INVALID_HANDLE_VALUE;

    SFindHandle *pFind = new SFindHandle();
    SplitMask( pszMask, &pFind->szDirectory, &pFind->szPattern );
    pFind->pDir = opendir( pFind->szDirectory.c_str() );
    if ( pFind->pDir == 0 )
    {
        delete pFind;
        return INVALID_HANDLE_VALUE;
    }
    if ( !NextMatch( pFind, pData ) )
    {
        delete pFind;
        return INVALID_HANDLE_VALUE;
    }
    return reinterpret_cast<HANDLE>( pFind );
}

BOOL FindNextFileA( HANDLE hFind, WIN32_FIND_DATAA *pData )
{
    if ( hFind == 0 || hFind == INVALID_HANDLE_VALUE || pData == 0 )
        return FALSE;
    return NextMatch( reinterpret_cast<SFindHandle *>( hFind ), pData ) ? TRUE : FALSE;
}

BOOL FindClose( HANDLE hFind )
{
    if ( hFind == 0 || hFind == INVALID_HANDLE_VALUE )
        return FALSE;
    delete reinterpret_cast<SFindHandle *>( hFind );
    return TRUE;
}

DWORD GetFileAttributesA( const char *pszPath )
{
    if ( pszPath == 0 )
        return INVALID_FILE_ATTRIBUTES;
    const std::string szPath = Bk1HostPath( pszPath );
    pszPath = szPath.c_str();
    struct stat st;
    if ( stat( pszPath, &st ) != 0 )
        return INVALID_FILE_ATTRIBUTES;
    DWORD dwAttr = S_ISDIR( st.st_mode ) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
    if ( ( st.st_mode & S_IWUSR ) == 0 )
        dwAttr |= FILE_ATTRIBUTE_READONLY;
    return dwAttr;
}

BOOL SetFileAttributesA( const char *pszPath, DWORD dwAttributes )
{
    if ( pszPath == 0 )
        return FALSE;
    const std::string szPath = Bk1HostPath( pszPath );
    pszPath = szPath.c_str();
    struct stat st;
    if ( stat( pszPath, &st ) != 0 )
        return FALSE;
    mode_t mode = st.st_mode;
    if ( ( dwAttributes & FILE_ATTRIBUTE_READONLY ) != 0 )
        mode &= ~(mode_t)( S_IWUSR | S_IWGRP | S_IWOTH );
    else
        mode |= S_IWUSR;
    return chmod( pszPath, mode ) == 0 ? TRUE : FALSE;
}

BOOL MoveFileA( const char *pszFrom, const char *pszTo )
{
    if ( pszFrom == 0 || pszTo == 0 )
        return FALSE;
    const std::string szFrom = Bk1HostPath( pszFrom );
    const std::string szTo = Bk1HostPath( pszTo );
    pszFrom = szFrom.c_str();
    pszTo = szTo.c_str();
    Bk1ForgetDirectory( pszFrom );
    Bk1ForgetDirectory( pszTo );
    return rename( pszFrom, pszTo ) == 0 ? TRUE : FALSE;
}

BOOL DeleteFileA( const char *pszPath )
{
    if ( pszPath == 0 )
        return FALSE;
    const std::string szPath = Bk1HostPath( pszPath );
    pszPath = szPath.c_str();
    Bk1ForgetDirectory( pszPath );
    return unlink( pszPath ) == 0 ? TRUE : FALSE;
}

BOOL CopyFileA( const char *pszFrom, const char *pszTo, BOOL bFailIfExists )
{
    if ( pszFrom == 0 || pszTo == 0 )
        return FALSE;
    const std::string szFrom = Bk1HostPath( pszFrom );
    const std::string szTo = Bk1HostPath( pszTo );
    pszFrom = szFrom.c_str();
    pszTo = szTo.c_str();
    if ( bFailIfExists != FALSE && access( pszTo, F_OK ) == 0 )
        return FALSE;

    FILE *pIn = fopen( pszFrom, "rb" );
    if ( pIn == 0 )
        return FALSE;
    FILE *pOut = fopen( pszTo, "wb" );
    if ( pOut == 0 )
    {
        fclose( pIn );
        return FALSE;
    }
    char buff[16384];
    size_t nRead;
    BOOL bOk = TRUE;
    while ( ( nRead = fread( buff, 1, sizeof( buff ), pIn ) ) > 0 )
    {
        if ( fwrite( buff, 1, nRead, pOut ) != nRead )
        {
            bOk = FALSE;
            break;
        }
    }
    fclose( pIn );
    fclose( pOut );
    return bOk;
}

BOOL CreateDirectoryA( const char *pszPath, LPVOID )
{
    if ( pszPath == 0 )
        return FALSE;
    const std::string szPath = Bk1HostPath( pszPath );
    pszPath = szPath.c_str();
    Bk1ForgetDirectory( pszPath );
    return mkdir( pszPath, 0777 ) == 0 ? TRUE : FALSE;
}

BOOL RemoveDirectoryA( const char *pszPath )
{
    if ( pszPath == 0 )
        return FALSE;
    const std::string szPath = Bk1HostPath( pszPath );
    pszPath = szPath.c_str();
    Bk1ForgetDirectory( pszPath );
    return rmdir( pszPath ) == 0 ? TRUE : FALSE;
}

DWORD GetCurrentDirectoryA( DWORD nSize, char *pszBuffer )
{
    if ( pszBuffer == 0 || nSize == 0 )
        return 0;
    if ( getcwd( pszBuffer, nSize ) == 0 )
        return 0;
    return (DWORD)strlen( pszBuffer );
}

BOOL SetCurrentDirectoryA( const char *pszPath )
{
    if ( pszPath == 0 )
        return FALSE;
    const std::string szPath = Bk1HostPath( pszPath );
    pszPath = szPath.c_str();
    return chdir( pszPath ) == 0 ? TRUE : FALSE;
}

BOOL Bk1SetFileTimeByPath( const char *pszPath, const FILETIME * /*pCreation*/,
                           const FILETIME *pLastAccess, const FILETIME *pLastWrite )
{
    // POSIX has no creation stamp to set, so that argument is ignored.
    if ( pszPath == 0 )
        return FALSE;
    const std::string szPath = Bk1HostPath( pszPath );
    pszPath = szPath.c_str();

    struct stat st;
    if ( stat( pszPath, &st ) != 0 )
        return FALSE;

    struct timeval times[2];
    times[0].tv_sec = pLastAccess != 0
                          ? (time_t)Bk1FileTimeToUnixTime( pLastAccess )
                          : st.st_atime;
    times[0].tv_usec = 0;
    times[1].tv_sec = pLastWrite != 0
                          ? (time_t)Bk1FileTimeToUnixTime( pLastWrite )
                          : st.st_mtime;
    times[1].tv_usec = 0;
    return utimes( pszPath, times ) == 0 ? TRUE : FALSE;
}

}   // extern "C"
