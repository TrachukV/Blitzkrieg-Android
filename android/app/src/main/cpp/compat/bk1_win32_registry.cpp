// The registry behind bk1_win32_registry.h: a tree of keys and values held in
// memory and written back to one file.
//
// The file is line-based and readable, because when a ported game misreads its
// own settings the first thing you want is to look at them:
//
//   [Software\Nival Interactive\Blitzkrieg]
//   InstallPath=1:D%3A%5CGames%5CBlitzkrieg
//
// A section header is a key's full path; a line is name=type:value, with
// anything outside printable ASCII percent-escaped so that a REG_BINARY value
// survives the round trip intact.
#include "bk1_win32_registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <string>
#include <vector>

namespace {

struct SValue
{
    DWORD                     dwType;
    std::vector<unsigned char> data;

    SValue() : dwType( REG_NONE ) {}
};

typedef std::map<std::string, SValue> TValues;

struct SKey
{
    std::string szPath;      // full path, roots included
    TValues     values;
};

typedef std::map<std::string, SKey *> TKeys;

TKeys       g_keys;
std::string g_szFile;
bool        g_bLoaded = false;
bool        g_bDirty = false;

// --- path handling ----------------------------------------------------------
const char *RootName( HKEY hKey )
{
    if ( hKey == HKEY_CLASSES_ROOT )   return "HKEY_CLASSES_ROOT";
    if ( hKey == HKEY_CURRENT_USER )   return "HKEY_CURRENT_USER";
    if ( hKey == HKEY_LOCAL_MACHINE )  return "HKEY_LOCAL_MACHINE";
    if ( hKey == HKEY_USERS )          return "HKEY_USERS";
    if ( hKey == HKEY_CURRENT_CONFIG ) return "HKEY_CURRENT_CONFIG";
    return 0;
}

bool IsRoot( HKEY hKey )
{
    return RootName( hKey ) != 0;
}

std::string Normalise( const std::string &szPath )
{
    // Windows registry paths are case-insensitive and slash-agnostic; the file
    // keeps the spelling it was first given, the lookup does not care.
    std::string res;
    res.reserve( szPath.size() );
    bool bLastSep = false;
    for ( size_t i = 0; i < szPath.size(); ++i )
    {
        char c = szPath[i];
        if ( c == '/' )
            c = '\\';
        if ( c == '\\' )
        {
            if ( bLastSep || res.empty() )
                continue;
            bLastSep = true;
            res.push_back( c );
            continue;
        }
        bLastSep = false;
        res.push_back( (char)( ( c >= 'A' && c <= 'Z' ) ? c - 'A' + 'a' : c ) );
    }
    while ( !res.empty() && res[res.size() - 1] == '\\' )
        res.erase( res.size() - 1 );
    return res;
}

void Load();

SKey *Find( const std::string &szPath )
{
    Load();
    TKeys::iterator it = g_keys.find( Normalise( szPath ) );
    return ( it == g_keys.end() ) ? 0 : it->second;
}

SKey *FindOrCreate( const std::string &szPath )
{
    Load();
    const std::string szNorm = Normalise( szPath );
    TKeys::iterator it = g_keys.find( szNorm );
    if ( it != g_keys.end() )
        return it->second;
    SKey *pKey = new SKey();
    pKey->szPath = szPath;
    g_keys[szNorm] = pKey;
    g_bDirty = true;
    return pKey;
}

// Resolves a handle plus a subkey name into a full path. Returns false when
// the handle is not one this registry issued.
bool ResolvePath( HKEY hKey, const char *pszSubKey, std::string *pOut )
{
    if ( IsRoot( hKey ) )
        *pOut = RootName( hKey );
    else if ( hKey != 0 )
        *pOut = reinterpret_cast<SKey *>( hKey )->szPath;
    else
        return false;

    if ( pszSubKey != 0 && pszSubKey[0] != 0 )
    {
        if ( !pOut->empty() && ( *pOut )[pOut->size() - 1] != '\\' )
            pOut->push_back( '\\' );
        *pOut += pszSubKey;
    }
    return true;
}

// --- escaping ---------------------------------------------------------------
bool NeedsEscape( unsigned char c )
{
    return c < 0x20 || c > 0x7E || c == '%' || c == '=' || c == '[' || c == ']';
}

std::string Escape( const std::vector<unsigned char> &data )
{
    static const char HEX[] = "0123456789ABCDEF";
    std::string res;
    res.reserve( data.size() );
    for ( size_t i = 0; i < data.size(); ++i )
    {
        const unsigned char c = data[i];
        if ( NeedsEscape( c ) )
        {
            res.push_back( '%' );
            res.push_back( HEX[c >> 4] );
            res.push_back( HEX[c & 0x0F] );
        }
        else
        {
            res.push_back( (char)c );
        }
    }
    return res;
}

int HexDigit( char c )
{
    if ( c >= '0' && c <= '9' ) return c - '0';
    if ( c >= 'a' && c <= 'f' ) return c - 'a' + 10;
    if ( c >= 'A' && c <= 'F' ) return c - 'A' + 10;
    return -1;
}

std::vector<unsigned char> Unescape( const std::string &sz )
{
    std::vector<unsigned char> res;
    res.reserve( sz.size() );
    for ( size_t i = 0; i < sz.size(); ++i )
    {
        if ( sz[i] == '%' && i + 2 < sz.size() )
        {
            const int hi = HexDigit( sz[i + 1] );
            const int lo = HexDigit( sz[i + 2] );
            if ( hi >= 0 && lo >= 0 )
            {
                res.push_back( (unsigned char)( ( hi << 4 ) | lo ) );
                i += 2;
                continue;
            }
        }
        res.push_back( (unsigned char)sz[i] );
    }
    return res;
}

// --- persistence ------------------------------------------------------------
void Load()
{
    if ( g_bLoaded )
        return;
    g_bLoaded = true;                       // even a failed read counts as tried
    if ( g_szFile.empty() )
        return;

    FILE *pFile = fopen( g_szFile.c_str(), "rb" );
    if ( pFile == 0 )
        return;

    std::string szLine;
    SKey *pCurrent = 0;
    int ch;
    for ( ;; )
    {
        ch = fgetc( pFile );
        if ( ch != EOF && ch != '\n' )
        {
            if ( ch != '\r' )
                szLine.push_back( (char)ch );
            continue;
        }

        if ( !szLine.empty() )
        {
            if ( szLine[0] == '[' && szLine[szLine.size() - 1] == ']' )
            {
                const std::string szPath = szLine.substr( 1, szLine.size() - 2 );
                SKey *pKey = new SKey();
                pKey->szPath = szPath;
                g_keys[Normalise( szPath )] = pKey;
                pCurrent = pKey;
            }
            else if ( pCurrent != 0 )
            {
                const size_t nEq = szLine.find( '=' );
                const size_t nColon = szLine.find( ':', nEq == std::string::npos ? 0 : nEq );
                if ( nEq != std::string::npos && nColon != std::string::npos )
                {
                    SValue v;
                    v.dwType = (DWORD)strtoul( szLine.substr( nEq + 1, nColon - nEq - 1 ).c_str(),
                                               0, 10 );
                    v.data = Unescape( szLine.substr( nColon + 1 ) );
                    pCurrent->values[szLine.substr( 0, nEq )] = v;
                }
            }
        }
        szLine.clear();
        if ( ch == EOF )
            break;
    }
    fclose( pFile );
    g_bDirty = false;
}

void Save()
{
    if ( g_szFile.empty() || !g_bDirty )
        return;

    // written beside the target and renamed, so an interrupted write cannot
    // leave the player without their settings
    const std::string szTemp = g_szFile + ".tmp";
    FILE *pFile = fopen( szTemp.c_str(), "wb" );
    if ( pFile == 0 )
        return;

    for ( TKeys::const_iterator it = g_keys.begin(); it != g_keys.end(); ++it )
    {
        fprintf( pFile, "[%s]\n", it->second->szPath.c_str() );
        for ( TValues::const_iterator v = it->second->values.begin();
              v != it->second->values.end(); ++v )
        {
            fprintf( pFile, "%s=%u:%s\n", v->first.c_str(),
                     (unsigned)v->second.dwType, Escape( v->second.data ).c_str() );
        }
        fprintf( pFile, "\n" );
    }
    fclose( pFile );
    rename( szTemp.c_str(), g_szFile.c_str() );
    g_bDirty = false;
}

}   // anonymous namespace

extern "C" {

void Bk1SetRegistryFile( const char *pszPath )
{
    g_szFile = ( pszPath != 0 ) ? pszPath : "";
    g_bLoaded = false;                      // reread from the new location
    for ( TKeys::iterator it = g_keys.begin(); it != g_keys.end(); ++it )
        delete it->second;
    g_keys.clear();
}

void Bk1FlushRegistry( void )
{
    Save();
}

LONG RegCreateKeyExA( HKEY hKey, const char *pszSubKey, DWORD, char *, DWORD,
                      REGSAM, void *, PHKEY phkResult, DWORD *pdwDisposition )
{
    std::string szPath;
    if ( !ResolvePath( hKey, pszSubKey, &szPath ) || phkResult == 0 )
        return ERROR_INVALID_HANDLE;

    const bool bExisted = ( Find( szPath ) != 0 );
    SKey *pKey = FindOrCreate( szPath );
    *phkResult = reinterpret_cast<HKEY>( pKey );
    if ( pdwDisposition != 0 )
        *pdwDisposition = bExisted ? REG_OPENED_EXISTING_KEY : REG_CREATED_NEW_KEY;
    if ( !bExisted )
        Save();
    return ERROR_SUCCESS;
}

LONG RegOpenKeyExA( HKEY hKey, const char *pszSubKey, DWORD, REGSAM, PHKEY phkResult )
{
    std::string szPath;
    if ( !ResolvePath( hKey, pszSubKey, &szPath ) || phkResult == 0 )
        return ERROR_INVALID_HANDLE;

    SKey *pKey = Find( szPath );
    if ( pKey == 0 )
    {
        *phkResult = 0;
        return ERROR_FILE_NOT_FOUND;
    }
    *phkResult = reinterpret_cast<HKEY>( pKey );
    return ERROR_SUCCESS;
}

LONG RegCloseKey( HKEY hKey )
{
    // Keys live for as long as the registry does; closing one only means the
    // caller is done with it.
    (void)hKey;
    return ERROR_SUCCESS;
}

LONG RegQueryValueExA( HKEY hKey, const char *pszValueName, DWORD *,
                       DWORD *pdwType, LPBYTE pData, DWORD *pcbData )
{
    if ( hKey == 0 || IsRoot( hKey ) )
        return ERROR_INVALID_HANDLE;
    SKey *pKey = reinterpret_cast<SKey *>( hKey );

    TValues::const_iterator it =
        pKey->values.find( pszValueName != 0 ? pszValueName : "" );
    if ( it == pKey->values.end() )
        return ERROR_FILE_NOT_FOUND;

    if ( pdwType != 0 )
        *pdwType = it->second.dwType;

    const DWORD cbNeeded = (DWORD)it->second.data.size();
    if ( pData == 0 )
    {
        if ( pcbData != 0 )
            *pcbData = cbNeeded;
        return ERROR_SUCCESS;
    }
    if ( pcbData == 0 )
        return ERROR_INVALID_DATA;
    if ( *pcbData < cbNeeded )
    {
        *pcbData = cbNeeded;
        return ERROR_MORE_DATA;
    }
    if ( cbNeeded > 0 )
        memcpy( pData, &it->second.data[0], cbNeeded );
    *pcbData = cbNeeded;
    return ERROR_SUCCESS;
}

LONG RegSetValueExA( HKEY hKey, const char *pszValueName, DWORD, DWORD dwType,
                     const BYTE *pData, DWORD cbData )
{
    if ( hKey == 0 || IsRoot( hKey ) )
        return ERROR_INVALID_HANDLE;
    SKey *pKey = reinterpret_cast<SKey *>( hKey );

    SValue v;
    v.dwType = dwType;
    if ( pData != 0 && cbData > 0 )
        v.data.assign( pData, pData + cbData );
    pKey->values[pszValueName != 0 ? pszValueName : ""] = v;
    g_bDirty = true;
    Save();                                 // settings survive a hard exit
    return ERROR_SUCCESS;
}

LONG RegDeleteKeyA( HKEY hKey, const char *pszSubKey )
{
    std::string szPath;
    if ( !ResolvePath( hKey, pszSubKey, &szPath ) )
        return ERROR_INVALID_HANDLE;

    Load();
    const std::string szNorm = Normalise( szPath );
    TKeys::iterator it = g_keys.find( szNorm );
    if ( it == g_keys.end() )
        return ERROR_FILE_NOT_FOUND;

    // as on Windows, a key with subkeys below it is not deleted
    const std::string szPrefix = szNorm + "\\";
    for ( TKeys::const_iterator k = g_keys.begin(); k != g_keys.end(); ++k )
    {
        if ( k->first.compare( 0, szPrefix.size(), szPrefix ) == 0 )
            return ERROR_ACCESS_DENIED;
    }

    delete it->second;
    g_keys.erase( it );
    g_bDirty = true;
    Save();
    return ERROR_SUCCESS;
}

LONG RegDeleteValueA( HKEY hKey, const char *pszValueName )
{
    if ( hKey == 0 || IsRoot( hKey ) )
        return ERROR_INVALID_HANDLE;
    SKey *pKey = reinterpret_cast<SKey *>( hKey );

    TValues::iterator it = pKey->values.find( pszValueName != 0 ? pszValueName : "" );
    if ( it == pKey->values.end() )
        return ERROR_FILE_NOT_FOUND;
    pKey->values.erase( it );
    g_bDirty = true;
    Save();
    return ERROR_SUCCESS;
}

LONG RegEnumKeyExA( HKEY hKey, DWORD dwIndex, char *pszName, DWORD *pcchName,
                    DWORD *, char *, DWORD *, void * )
{
    std::string szPath;
    if ( !ResolvePath( hKey, 0, &szPath ) || pszName == 0 || pcchName == 0 )
        return ERROR_INVALID_HANDLE;

    Load();
    const std::string szPrefix = Normalise( szPath ) + "\\";

    // immediate children only, in the order the map holds them, which is the
    // stable ordering an enumeration needs
    DWORD nSeen = 0;
    for ( TKeys::const_iterator it = g_keys.begin(); it != g_keys.end(); ++it )
    {
        if ( it->first.compare( 0, szPrefix.size(), szPrefix ) != 0 )
            continue;
        const std::string szRest = it->first.substr( szPrefix.size() );
        if ( szRest.empty() || szRest.find( '\\' ) != std::string::npos )
            continue;                       // a grandchild, not a child

        if ( nSeen == dwIndex )
        {
            // report the stored spelling rather than the folded one
            const std::string &szFull = it->second->szPath;
            const size_t nAt = szFull.find_last_of( '\\' );
            const std::string szLeaf =
                ( nAt == std::string::npos ) ? szFull : szFull.substr( nAt + 1 );
            if ( *pcchName <= szLeaf.size() )
            {
                *pcchName = (DWORD)szLeaf.size() + 1;
                return ERROR_MORE_DATA;
            }
            memcpy( pszName, szLeaf.c_str(), szLeaf.size() + 1 );
            *pcchName = (DWORD)szLeaf.size();
            return ERROR_SUCCESS;
        }
        ++nSeen;
    }
    return ERROR_NO_MORE_ITEMS;
}

LONG RegEnumValueA( HKEY hKey, DWORD dwIndex, char *pszValueName,
                    DWORD *pcchValueName, DWORD *, DWORD *pdwType,
                    LPBYTE pData, DWORD *pcbData )
{
    if ( hKey == 0 || IsRoot( hKey ) || pszValueName == 0 || pcchValueName == 0 )
        return ERROR_INVALID_HANDLE;
    SKey *pKey = reinterpret_cast<SKey *>( hKey );

    if ( dwIndex >= pKey->values.size() )
        return ERROR_NO_MORE_ITEMS;

    TValues::const_iterator it = pKey->values.begin();
    for ( DWORD i = 0; i < dwIndex; ++i )
        ++it;

    if ( *pcchValueName <= it->first.size() )
    {
        *pcchValueName = (DWORD)it->first.size() + 1;
        return ERROR_MORE_DATA;
    }
    memcpy( pszValueName, it->first.c_str(), it->first.size() + 1 );
    *pcchValueName = (DWORD)it->first.size();

    if ( pdwType != 0 )
        *pdwType = it->second.dwType;

    if ( pData != 0 && pcbData != 0 )
    {
        const DWORD cbNeeded = (DWORD)it->second.data.size();
        if ( *pcbData < cbNeeded )
        {
            *pcbData = cbNeeded;
            return ERROR_MORE_DATA;
        }
        if ( cbNeeded > 0 )
            memcpy( pData, &it->second.data[0], cbNeeded );
        *pcbData = cbNeeded;
    }
    return ERROR_SUCCESS;
}

}   // extern "C"
