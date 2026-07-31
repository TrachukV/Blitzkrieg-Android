#pragma once
// The Win32 text entry points the engine calls directly.
//
// Blitzkrieg stores its narrow strings in a Windows ANSI code page and its wide
// strings as UTF-16 (`WORD*`), while it also builds std::wstring over the
// platform's wchar_t. On Android wchar_t is 32-bit, so these conversions are
// real transcoding rather than the no-ops they were under MSVC 6.
#include "bk1_win32_types.h"

#define CP_ACP        0
#define CP_OEMCP      1
#define CP_UTF8       65001

#ifdef __cplusplus
extern "C" {
#endif

// The ANSI code page the narrow strings in the game data are written in.
// Blitzkrieg's Russian build ships CP1251 text; western builds ship CP1252.
// 'Bk1SetAnsiCodePage' lets the startup code pick before any text is read.
UINT GetACP( void );
void Bk1SetAnsiCodePage( UINT nCodePage );

int MultiByteToWideChar( UINT nCodePage, DWORD dwFlags, const char *pszSrc,
                         int nSrcLen, wchar_t *pDst, int nDstLen );
int WideCharToMultiByte( UINT nCodePage, DWORD dwFlags, const wchar_t *pSrc,
                         int nSrcLen, char *pszDst, int nDstLen,
                         const char *pszDefault, BOOL *pbUsedDefault );

void OutputDebugStringA( const char *pszText );
#define OutputDebugString OutputDebugStringA

char *_itoa( int nValue, char *pszBuffer, int nRadix );
char *_ltoa( long nValue, char *pszBuffer, int nRadix );

#ifdef __cplusplus
}
#endif
