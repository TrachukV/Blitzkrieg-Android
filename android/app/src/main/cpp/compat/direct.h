#pragma once
// Stands in for the MSVC directory header.
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

inline int _mkdir( const char *pszPath ) { return mkdir( pszPath, 0777 ); }
inline int _rmdir( const char *pszPath ) { return rmdir( pszPath ); }
inline int _chdir( const char *pszPath ) { return chdir( pszPath ); }
inline char *_getcwd( char *pszBuffer, int nSize ) { return getcwd( pszBuffer, nSize ); }

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
