// Implementations of the Win32 text entry points declared in
// bk1_win32_strings.h. Only the code pages Blitzkrieg's shipped data actually
// uses are supported: the two Windows ANSI pages and UTF-8.
#include "bk1_win32_strings.h"

#include <stdio.h>
#include <string.h>

#if defined( __ANDROID__ )
#include <android/log.h>
#endif

namespace {

const unsigned short REPLACEMENT = 0xFFFD;

// CP1251 (Windows Cyrillic), code points for 0x80..0xBF. From 0xC0 up the page
// is contiguous: 0xC0..0xFF map to U+0410..U+044F.
const unsigned short CP1251_HIGH[64] = {
    0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021,
    0x20AC, 0x2030, 0x0409, 0x2039, 0x040A, 0x040C, 0x040B, 0x040F,
    0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    REPLACEMENT, 0x2122, 0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F,
    0x00A0, 0x040E, 0x045E, 0x0408, 0x00A4, 0x0490, 0x00A6, 0x00A7,
    0x0401, 0x00A9, 0x0404, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x0407,
    0x00B0, 0x00B1, 0x0406, 0x0456, 0x0491, 0x00B5, 0x00B6, 0x00B7,
    0x0451, 0x2116, 0x0454, 0x00BB, 0x0458, 0x0405, 0x0455, 0x0457,
};

// CP1252 (Windows Western), code points for 0x80..0x9F. From 0xA0 up the page
// is Latin-1: 0xA0..0xFF map to U+00A0..U+00FF.
const unsigned short CP1252_HIGH[32] = {
    0x20AC, REPLACEMENT, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, REPLACEMENT, 0x017D, REPLACEMENT,
    REPLACEMENT, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, REPLACEMENT, 0x017E, 0x0178,
};

UINT g_nAnsiCodePage = 1251;

UINT ResolveCodePage( UINT nCodePage )
{
    if ( nCodePage == CP_ACP || nCodePage == CP_OEMCP )
        return g_nAnsiCodePage;
    return nCodePage;
}

unsigned int AnsiByteToUnicode( unsigned char c, UINT nCodePage )
{
    if ( c < 0x80 )
        return c;
    if ( nCodePage == 1251 )
        return c < 0xC0 ? CP1251_HIGH[c - 0x80] : ( 0x0410 + ( c - 0xC0 ) );
    // 1252 and anything else western
    return c < 0xA0 ? CP1252_HIGH[c - 0x80] : c;
}

// Reverse of the above. Returns -1 when the code point has no representation.
int UnicodeToAnsiByte( unsigned int cp, UINT nCodePage )
{
    if ( cp < 0x80 )
        return static_cast<int>( cp );
    if ( nCodePage == 1251 )
    {
        if ( cp >= 0x0410 && cp <= 0x044F )
            return static_cast<int>( 0xC0 + ( cp - 0x0410 ) );
        for ( int i = 0; i < 64; ++i )
        {
            if ( CP1251_HIGH[i] == cp && CP1251_HIGH[i] != REPLACEMENT )
                return 0x80 + i;
        }
        return -1;
    }
    if ( cp >= 0xA0 && cp <= 0xFF )
        return static_cast<int>( cp );
    for ( int i = 0; i < 32; ++i )
    {
        if ( CP1252_HIGH[i] == cp && CP1252_HIGH[i] != REPLACEMENT )
            return 0x80 + i;
    }
    return -1;
}

// Decodes one UTF-8 sequence, advancing *pIndex. Malformed input yields the
// replacement character and consumes one byte, which keeps callers terminating.
unsigned int DecodeUtf8( const char *psz, int nLen, int *pIndex )
{
    const unsigned char *p = reinterpret_cast<const unsigned char *>( psz );
    int i = *pIndex;
    unsigned int c = p[i];
    int nExtra = 0;
    unsigned int cp = 0;

    if ( c < 0x80 ) { *pIndex = i + 1; return c; }
    else if ( ( c & 0xE0 ) == 0xC0 ) { nExtra = 1; cp = c & 0x1F; }
    else if ( ( c & 0xF0 ) == 0xE0 ) { nExtra = 2; cp = c & 0x0F; }
    else if ( ( c & 0xF8 ) == 0xF0 ) { nExtra = 3; cp = c & 0x07; }
    else { *pIndex = i + 1; return REPLACEMENT; }

    if ( i + nExtra >= nLen ) { *pIndex = i + 1; return REPLACEMENT; }
    for ( int k = 1; k <= nExtra; ++k )
    {
        unsigned int cc = p[i + k];
        if ( ( cc & 0xC0 ) != 0x80 ) { *pIndex = i + 1; return REPLACEMENT; }
        cp = ( cp << 6 ) | ( cc & 0x3F );
    }
    *pIndex = i + nExtra + 1;
    return cp;
}

int EncodeUtf8( unsigned int cp, char *pDst, int nRoom )
{
    int nNeed = cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4;
    if ( pDst == 0 )
        return nNeed;
    if ( nNeed > nRoom )
        return 0;
    unsigned char *p = reinterpret_cast<unsigned char *>( pDst );
    switch ( nNeed )
    {
    case 1: p[0] = static_cast<unsigned char>( cp ); break;
    case 2: p[0] = 0xC0 | ( cp >> 6 );   p[1] = 0x80 | ( cp & 0x3F ); break;
    case 3: p[0] = 0xE0 | ( cp >> 12 );  p[1] = 0x80 | ( ( cp >> 6 ) & 0x3F );
            p[2] = 0x80 | ( cp & 0x3F ); break;
    default: p[0] = 0xF0 | ( cp >> 18 ); p[1] = 0x80 | ( ( cp >> 12 ) & 0x3F );
             p[2] = 0x80 | ( ( cp >> 6 ) & 0x3F ); p[3] = 0x80 | ( cp & 0x3F ); break;
    }
    return nNeed;
}

}   // anonymous namespace

extern "C" {

UINT GetACP( void )
{
    return g_nAnsiCodePage;
}

void Bk1SetAnsiCodePage( UINT nCodePage )
{
    g_nAnsiCodePage = nCodePage;
}

int MultiByteToWideChar( UINT nCodePage, DWORD /*dwFlags*/, const char *pszSrc,
                         int nSrcLen, wchar_t *pDst, int nDstLen )
{
    if ( pszSrc == 0 )
        return 0;
    const bool bTerminated = ( nSrcLen < 0 );
    if ( bTerminated )
        nSrcLen = static_cast<int>( strlen( pszSrc ) ) + 1;   // terminator included, as on Windows

    const UINT nPage = ResolveCodePage( nCodePage );
    int nWritten = 0;
    int i = 0;
    while ( i < nSrcLen )
    {
        unsigned int cp;
        if ( nPage == CP_UTF8 )
            cp = DecodeUtf8( pszSrc, nSrcLen, &i );
        else
            cp = AnsiByteToUnicode( static_cast<unsigned char>( pszSrc[i++] ), nPage );

        if ( pDst != 0 )
        {
            if ( nWritten >= nDstLen )
                return 0;                    // Windows reports insufficient buffer
            pDst[nWritten] = static_cast<wchar_t>( cp );
        }
        ++nWritten;
    }
    return nWritten;
}

int WideCharToMultiByte( UINT nCodePage, DWORD /*dwFlags*/, const wchar_t *pSrc,
                         int nSrcLen, char *pszDst, int nDstLen,
                         const char *pszDefault, BOOL *pbUsedDefault )
{
    if ( pSrc == 0 )
        return 0;
    if ( nSrcLen < 0 )
    {
        nSrcLen = 0;
        while ( pSrc[nSrcLen] != 0 )
            ++nSrcLen;
        ++nSrcLen;                           // terminator included, as on Windows
    }

    const UINT nPage = ResolveCodePage( nCodePage );
    const char chDefault = pszDefault != 0 ? *pszDefault : '?';
    if ( pbUsedDefault != 0 )
        *pbUsedDefault = FALSE;

    int nWritten = 0;
    for ( int i = 0; i < nSrcLen; ++i )
    {
        const unsigned int cp = static_cast<unsigned int>( pSrc[i] );
        if ( nPage == CP_UTF8 )
        {
            char buff[4];
            const int nNeed = EncodeUtf8( cp, pszDst != 0 ? buff : 0, 4 );
            if ( pszDst != 0 )
            {
                if ( nWritten + nNeed > nDstLen )
                    return 0;
                memcpy( pszDst + nWritten, buff, nNeed );
            }
            nWritten += nNeed;
            continue;
        }

        int nByte = UnicodeToAnsiByte( cp, nPage );
        if ( nByte < 0 )
        {
            nByte = static_cast<unsigned char>( chDefault );
            if ( pbUsedDefault != 0 )
                *pbUsedDefault = TRUE;
        }
        if ( pszDst != 0 )
        {
            if ( nWritten >= nDstLen )
                return 0;
            pszDst[nWritten] = static_cast<char>( nByte );
        }
        ++nWritten;
    }
    return nWritten;
}

int Bk1Utf16Len( const unsigned short *psz )
{
    if ( psz == 0 )
        return 0;
    int n = 0;
    while ( psz[n] != 0 )
        ++n;
    return n;
}

int Bk1AnsiToUtf16( UINT nCodePage, const char *pszSrc, int nSrcLen,
                    unsigned short *pDst, int nDstLen )
{
    if ( pszSrc == 0 )
        return 0;
    if ( nSrcLen < 0 )
        nSrcLen = (int)strlen( pszSrc ) + 1;      // terminator included

    const UINT nPage = ResolveCodePage( nCodePage );
    int nWritten = 0;
    int i = 0;
    while ( i < nSrcLen )
    {
        unsigned int cp;
        if ( nPage == CP_UTF8 )
            cp = DecodeUtf8( pszSrc, nSrcLen, &i );
        else
            cp = AnsiByteToUnicode( (unsigned char)pszSrc[i++], nPage );

        // a code point outside the basic plane needs a surrogate pair
        if ( cp >= 0x10000 )
        {
            const unsigned int v = cp - 0x10000;
            if ( pDst != 0 )
            {
                if ( nWritten + 2 > nDstLen )
                    return 0;
                pDst[nWritten]     = (unsigned short)( 0xD800 + ( v >> 10 ) );
                pDst[nWritten + 1] = (unsigned short)( 0xDC00 + ( v & 0x3FF ) );
            }
            nWritten += 2;
            continue;
        }

        if ( pDst != 0 )
        {
            if ( nWritten >= nDstLen )
                return 0;
            pDst[nWritten] = (unsigned short)cp;
        }
        ++nWritten;
    }
    return nWritten;
}

int Bk1Utf16ToAnsi( UINT nCodePage, const unsigned short *pSrc, int nSrcLen,
                    char *pszDst, int nDstLen )
{
    if ( pSrc == 0 )
        return 0;
    if ( nSrcLen < 0 )
        nSrcLen = Bk1Utf16Len( pSrc ) + 1;        // terminator included

    const UINT nPage = ResolveCodePage( nCodePage );
    int nWritten = 0;
    for ( int i = 0; i < nSrcLen; ++i )
    {
        unsigned int cp = pSrc[i];
        // recombine a surrogate pair before converting
        if ( cp >= 0xD800 && cp <= 0xDBFF && i + 1 < nSrcLen &&
             pSrc[i + 1] >= 0xDC00 && pSrc[i + 1] <= 0xDFFF )
        {
            cp = 0x10000 + ( ( cp - 0xD800 ) << 10 ) + ( pSrc[i + 1] - 0xDC00 );
            ++i;
        }

        if ( nPage == CP_UTF8 )
        {
            char buff[4];
            const int nNeed = EncodeUtf8( cp, pszDst != 0 ? buff : 0, 4 );
            if ( pszDst != 0 )
            {
                if ( nWritten + nNeed > nDstLen )
                    return 0;
                memcpy( pszDst + nWritten, buff, nNeed );
            }
            nWritten += nNeed;
            continue;
        }

        int nByte = UnicodeToAnsiByte( cp, nPage );
        if ( nByte < 0 )
            nByte = '?';
        if ( pszDst != 0 )
        {
            if ( nWritten >= nDstLen )
                return 0;
            pszDst[nWritten] = (char)nByte;
        }
        ++nWritten;
    }
    return nWritten;
}

// ---------------------------------------------------------------------------
// Case-insensitive comparison
// ---------------------------------------------------------------------------
// ASCII folding only. The engine compares file names, control identifiers and
// option keys with these, all of which are ASCII; folding a code page's own
// letters would need that page's rules and the engine never asks for it.
namespace {

inline int FoldAscii( unsigned int c )
{
    return ( c >= 'A' && c <= 'Z' ) ? (int)( c - 'A' + 'a' ) : (int)c;
}

}   // anonymous namespace

extern "C" int _stricmp( const char *pszA, const char *pszB )
{
    if ( pszA == 0 || pszB == 0 )
        return ( pszA == pszB ) ? 0 : ( pszA == 0 ? -1 : 1 );
    for ( ;; )
    {
        const int a = FoldAscii( (unsigned char)*pszA++ );
        const int b = FoldAscii( (unsigned char)*pszB++ );
        if ( a != b )
            return a - b;
        if ( a == 0 )
            return 0;
    }
}

extern "C" int _strnicmp( const char *pszA, const char *pszB, size_t nCount )
{
    if ( pszA == 0 || pszB == 0 )
        return ( pszA == pszB ) ? 0 : ( pszA == 0 ? -1 : 1 );
    for ( size_t i = 0; i < nCount; ++i )
    {
        const int a = FoldAscii( (unsigned char)pszA[i] );
        const int b = FoldAscii( (unsigned char)pszB[i] );
        if ( a != b )
            return a - b;
        if ( a == 0 )
            return 0;
    }
    return 0;
}

extern "C" int _wcsicmp( const wchar_t *pszA, const wchar_t *pszB )
{
    if ( pszA == 0 || pszB == 0 )
        return ( pszA == pszB ) ? 0 : ( pszA == 0 ? -1 : 1 );
    for ( ;; )
    {
        const int a = FoldAscii( (unsigned int)*pszA++ );
        const int b = FoldAscii( (unsigned int)*pszB++ );
        if ( a != b )
            return a - b;
        if ( a == 0 )
            return 0;
    }
}

extern "C" int _wcsnicmp( const wchar_t *pszA, const wchar_t *pszB, size_t nCount )
{
    if ( pszA == 0 || pszB == 0 )
        return ( pszA == pszB ) ? 0 : ( pszA == 0 ? -1 : 1 );
    for ( size_t i = 0; i < nCount; ++i )
    {
        const int a = FoldAscii( (unsigned int)pszA[i] );
        const int b = FoldAscii( (unsigned int)pszB[i] );
        if ( a != b )
            return a - b;
        if ( a == 0 )
            return 0;
    }
    return 0;
}

void OutputDebugStringA( const char *pszText )
{
#if defined( __ANDROID__ )
    __android_log_write( ANDROID_LOG_DEBUG, "Blitzkrieg", pszText != 0 ? pszText : "" );
#else
    fputs( pszText != 0 ? pszText : "", stderr );
#endif
}

char *_ltoa( long nValue, char *pszBuffer, int nRadix )
{
    if ( pszBuffer == 0 )
        return 0;
    if ( nRadix == 10 )
    {
        sprintf( pszBuffer, "%ld", nValue );
        return pszBuffer;
    }
    if ( nRadix == 16 ) { sprintf( pszBuffer, "%lx", nValue ); return pszBuffer; }
    if ( nRadix == 8 )  { sprintf( pszBuffer, "%lo", nValue ); return pszBuffer; }

    // general case, matching the CRT: digits emitted low-to-high then reversed
    unsigned long v = static_cast<unsigned long>( nValue );
    char *p = pszBuffer;
    if ( nValue < 0 && nRadix == 10 )
        v = static_cast<unsigned long>( -nValue );
    if ( v == 0 )
        *p++ = '0';
    while ( v != 0 )
    {
        const int d = static_cast<int>( v % static_cast<unsigned long>( nRadix ) );
        *p++ = static_cast<char>( d < 10 ? '0' + d : 'a' + d - 10 );
        v /= static_cast<unsigned long>( nRadix );
    }
    *p = 0;
    for ( char *a = pszBuffer, *b = p - 1; a < b; ++a, --b )
    {
        const char t = *a;
        *a = *b;
        *b = t;
    }
    return pszBuffer;
}

char *_itoa( int nValue, char *pszBuffer, int nRadix )
{
    return _ltoa( nValue, pszBuffer, nRadix );
}

}   // extern "C"
