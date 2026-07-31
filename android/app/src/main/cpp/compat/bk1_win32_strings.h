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

// The same conversions against UTF-16 rather than the platform's wchar_t.
// The engine's wide strings, and the text MSXML held, are UTF-16, so these are
// what the XML and text layers need; MultiByteToWideChar above is for the
// places that genuinely want wchar_t.
//
// Both return the number of units written, or the number required when the
// destination is null. 'nSrcLen' of -1 means the source is NUL-terminated and
// the terminator is included, as on Windows.
int Bk1AnsiToUtf16( UINT nCodePage, const char *pszSrc, int nSrcLen,
                    unsigned short *pDst, int nDstLen );
int Bk1Utf16ToAnsi( UINT nCodePage, const unsigned short *pSrc, int nSrcLen,
                    char *pszDst, int nDstLen );

// Length of a NUL-terminated UTF-16 string, in code units.
int Bk1Utf16Len( const unsigned short *psz );

void OutputDebugStringA( const char *pszText );
#define OutputDebugString OutputDebugStringA

char *_itoa( int nValue, char *pszBuffer, int nRadix );

// MSVC's case-insensitive comparisons. The wide ones fold only ASCII, which is
// what the engine compares with them -- file names and control identifiers.
int _stricmp( const char *pszA, const char *pszB );
int _strnicmp( const char *pszA, const char *pszB, size_t nCount );
int _wcsicmp( const wchar_t *pszA, const wchar_t *pszB );
int _wcsnicmp( const wchar_t *pszA, const wchar_t *pszB, size_t nCount );
char *_ltoa( long nValue, char *pszBuffer, int nRadix );

#ifdef __cplusplus
}
#endif
