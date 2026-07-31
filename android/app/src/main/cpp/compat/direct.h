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
#include "bk1_win32_fileio.h"			// _fullpath
