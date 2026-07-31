// The file-version reader. See bk1_win32_version.h for why every file reports
// the port's own version.
#include "bk1_win32_version.h"

#include <string.h>

namespace {

// The block the three calls pass between themselves. Windows hands back an
// opaque buffer from GetFileVersionInfo and expects it back at VerQueryValue;
// this is that buffer, and it holds exactly what the query returns.
struct SVersionBlock
{
    DWORD            dwMagic;
    VS_FIXEDFILEINFO info;
};

const DWORD BLOCK_MAGIC = 0x424B3156;   // 'BK1V'

void FillVersion( VS_FIXEDFILEINFO *pInfo )
{
    memset( pInfo, 0, sizeof( *pInfo ) );
    pInfo->dwSignature = VS_FFI_SIGNATURE;
    pInfo->dwStrucVersion = VS_FFI_STRUCVERSION;
    // Windows packs the four numbers into two DWORDs, high pair then low.
    pInfo->dwFileVersionMS =
        ( (DWORD)BK1_VERSION_MAJOR << 16 ) | (DWORD)BK1_VERSION_MINOR;
    pInfo->dwFileVersionLS =
        ( (DWORD)BK1_VERSION_BUILD << 16 ) | (DWORD)BK1_VERSION_REVISION;
    pInfo->dwProductVersionMS = pInfo->dwFileVersionMS;
    pInfo->dwProductVersionLS = pInfo->dwFileVersionLS;
    pInfo->dwFileOS = VOS_NT_WINDOWS32;
    pInfo->dwFileType = VFT_APP;
}

}   // anonymous namespace

extern "C" {

DWORD GetFileVersionInfoSizeA( const char *pszFileName, DWORD *pdwHandle )
{
    if ( pdwHandle != 0 )
        *pdwHandle = 0;
    if ( pszFileName == 0 )
        return 0;
    return (DWORD)sizeof( SVersionBlock );
}

BOOL GetFileVersionInfoA( const char *pszFileName, DWORD, DWORD dwLen,
                          void *pData )
{
    if ( pszFileName == 0 || pData == 0 || dwLen < sizeof( SVersionBlock ) )
        return FALSE;
    SVersionBlock *pBlock = (SVersionBlock *)pData;
    pBlock->dwMagic = BLOCK_MAGIC;
    FillVersion( &pBlock->info );
    return TRUE;
}

BOOL VerQueryValueA( const void *pBlock, const char *pszSubBlock,
                     void **ppBuffer, UINT *puLen )
{
    if ( pBlock == 0 || ppBuffer == 0 )
        return FALSE;
    const SVersionBlock *pVersion = (const SVersionBlock *)pBlock;
    if ( pVersion->dwMagic != BLOCK_MAGIC )
        return FALSE;

    // "\\" is the root query, which is what returns the fixed information.
    // The engine asks for nothing else; a different query is a question this
    // has no answer to, and saying so is better than handing back the root.
    if ( pszSubBlock == 0 ||
         ( strcmp( pszSubBlock, "\\" ) != 0 && strcmp( pszSubBlock, "" ) != 0 ) )
    {
        *ppBuffer = 0;
        return FALSE;
    }

    *ppBuffer = (void *)&pVersion->info;
    if ( puLen != 0 )
        *puLen = (UINT)sizeof( VS_FIXEDFILEINFO );
    return TRUE;
}

}   // extern "C"
