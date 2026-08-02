// Wide-string length, for a 16-bit wchar_t.
//
// The engine's strings are UTF-16, as they are on Windows, and the port keeps
// them that way with -fshort-wchar. Android's C library is built the other way:
// its wchar_t is 32 bits, and so is every wide-string function in it.
//
// That mismatch reached the screen. A mission's instruction is 198 characters
// in the data and arrived as 99, cut mid-word; the objective notices in the
// corner were cut the same way. Measured at the point the string is built, the
// same text counted 320 characters walking it by hand and 162 through
// std::wstring -- the constructor takes its length from
// char_traits<wchar_t>::length, which clang folds into an inline scan sized for
// the 32-bit wchar_t it thinks the target has.
//
// It is inline, which is why an earlier check for imported wcs* symbols found
// none and wrongly cleared the C library of any part in this.
//
// Defining wcslen here, together with -fno-builtin-wcslen on the engine, puts a
// correct one in the way of every std::wstring in the port at once. Nothing at
// the call sites has to change, and the MSVC build never sees any of it.

#include <stddef.h>
#include <wchar.h>

#if !defined( _MSC_VER )

static_assert( sizeof( wchar_t ) == 2,
               "this file exists because wchar_t is 16-bit here; build with -fshort-wchar" );

extern "C" size_t wcslen( const wchar_t *pszText )
{
	const wchar_t *pszAt = pszText;
	while ( *pszAt != 0 )
		++pszAt;
	return size_t( pszAt - pszText );
}

// The rest of the family, for the same reason. The engine calls wcscpy and
// wcscmp directly in a few places, and libc++ reaches for wmemcpy, wmemmove,
// wmemset, wmemcmp and wmemchr when it copies or compares a wstring. Every one
// of those walks or counts in wchar_t units, so every one of them is wrong by a
// factor of two against a 16-bit string.
extern "C" wchar_t *wcscpy( wchar_t *pszTo, const wchar_t *pszFrom )
{
	wchar_t *pszAt = pszTo;
	while ( ( *pszAt++ = *pszFrom++ ) != 0 )
		;
	return pszTo;
}

extern "C" int wcscmp( const wchar_t *pszLeft, const wchar_t *pszRight )
{
	while ( *pszLeft != 0 && *pszLeft == *pszRight )
	{
		++pszLeft;
		++pszRight;
	}
	// Compared as unsigned, the way UTF-16 code units order.
	return int( unsigned( *pszLeft ) ) - int( unsigned( *pszRight ) );
}

extern "C" wchar_t *wmemcpy( wchar_t *pTo, const wchar_t *pFrom, size_t nCount )
{
	for ( size_t i = 0; i < nCount; ++i )
		pTo[i] = pFrom[i];
	return pTo;
}

extern "C" wchar_t *wmemmove( wchar_t *pTo, const wchar_t *pFrom, size_t nCount )
{
	if ( pTo < pFrom )
	{
		for ( size_t i = 0; i < nCount; ++i )
			pTo[i] = pFrom[i];
	}
	else
	{
		for ( size_t i = nCount; i > 0; --i )
			pTo[i - 1] = pFrom[i - 1];
	}
	return pTo;
}

extern "C" wchar_t *wmemset( wchar_t *pAt, wchar_t chValue, size_t nCount )
{
	for ( size_t i = 0; i < nCount; ++i )
		pAt[i] = chValue;
	return pAt;
}

extern "C" int wmemcmp( const wchar_t *pLeft, const wchar_t *pRight, size_t nCount )
{
	for ( size_t i = 0; i < nCount; ++i )
	{
		if ( pLeft[i] != pRight[i] )
			return int( unsigned( pLeft[i] ) ) - int( unsigned( pRight[i] ) );
	}
	return 0;
}

extern "C" wchar_t *wmemchr( const wchar_t *pAt, wchar_t chValue, size_t nCount )
{
	for ( size_t i = 0; i < nCount; ++i )
	{
		if ( pAt[i] == chValue )
			return const_cast<wchar_t *>( pAt + i );
	}
	return 0;
}

#endif // !_MSC_VER
