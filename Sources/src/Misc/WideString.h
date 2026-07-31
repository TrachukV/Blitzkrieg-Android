#ifndef __WIDESTRING_H__
#define __WIDESTRING_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// The engine's wide strings are UTF-16: they cross its own interfaces as
// 'const WORD*' and are stored in std::wstring, which under MSVC 6 was the
// same 16-bit type. Where wchar_t is wider than that -- as it is on Android --
// the two are different encodings, so the storage has to say which one it
// means. 'bk1_wstring' is that storage: std::wstring where wchar_t is UTF-16,
// and a UTF-16 string of the same shape everywhere else.
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <string>
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// The test is the width of wchar_t, not the name of the compiler: MSVC 6
// made it 16 bits, and the Android build asks for the same with
// -fshort-wchar, so in both of those std::wstring already is the UTF-16
// storage this names.
#if ( defined( __SIZEOF_WCHAR_T__ ) && __SIZEOF_WCHAR_T__ == 2 ) || \
    ( defined( _MSC_VER ) && !defined( _NATIVE_WCHAR_T_DEFINED ) )
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef std::wstring bk1_wstring;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#else
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// A character traits class rather than a specialisation of std::char_traits:
// WORD is a fundamental type, and specialising a standard template for one is
// not ours to do.
struct SBk1Utf16Traits
{
	typedef WORD					char_type;
	typedef int						int_type;
	typedef std::streamoff		off_type;
	typedef std::streampos		pos_type;
	typedef std::mbstate_t		state_type;

	static void assign( char_type &a, const char_type &b ) { a = b; }
	static bool eq( const char_type &a, const char_type &b ) { return a == b; }
	static bool lt( const char_type &a, const char_type &b ) { return a < b; }

	static int compare( const char_type *a, const char_type *b, size_t n )
	{
		for ( size_t i = 0; i < n; ++i )
		{
			if ( a[i] < b[i] )
				return -1;
			if ( b[i] < a[i] )
				return 1;
		}
		return 0;
	}

	static size_t length( const char_type *s )
	{
		size_t n = 0;
		while ( s[n] != 0 )
			++n;
		return n;
	}

	static const char_type* find( const char_type *s, size_t n, const char_type &c )
	{
		for ( size_t i = 0; i < n; ++i )
		{
			if ( s[i] == c )
				return s + i;
		}
		return 0;
	}

	static char_type* move( char_type *dst, const char_type *src, size_t n )
	{
		return n == 0 ? dst : static_cast<char_type*>( memmove( dst, src, n * sizeof( char_type ) ) );
	}

	static char_type* copy( char_type *dst, const char_type *src, size_t n )
	{
		return n == 0 ? dst : static_cast<char_type*>( memcpy( dst, src, n * sizeof( char_type ) ) );
	}

	static char_type* assign( char_type *s, size_t n, char_type c )
	{
		for ( size_t i = 0; i < n; ++i )
			s[i] = c;
		return s;
	}

	static int_type not_eof( const int_type &c ) { return c == eof() ? 0 : c; }
	static char_type to_char_type( const int_type &c ) { return static_cast<char_type>( c ); }
	static int_type to_int_type( const char_type &c ) { return static_cast<int_type>( c ); }
	static bool eq_int_type( const int_type &a, const int_type &b ) { return a == b; }
	static int_type eof() { return -1; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef std::basic_string<WORD, SBk1Utf16Traits> bk1_wstring;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// The engine spells the same UTF-16 string two ways -- 'const WORD*' at some
// of its interfaces and 'const wchar_t*' at others -- because MSVC 6 made
// those one type. They are one width here too, but not one type, so the few
// places where the two meet say so through these rather than through a bare
// cast that no one can audit later.
//
// The assertion is the point: if the wchar_t decision is ever revisited and
// the widths stop matching, every one of these stops compiling instead of
// quietly reinterpreting half a string.
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if !defined( _MSC_VER )
static_assert( sizeof( wchar_t ) == sizeof( WORD ),
               "the engine's wide strings are UTF-16; build with -fshort-wchar" );
#endif

inline const wchar_t* Bk1AsWide( const WORD *pszText )
{
	return reinterpret_cast<const wchar_t*>( pszText );
}
inline wchar_t* Bk1AsWide( WORD *pszText )
{
	return reinterpret_cast<wchar_t*>( pszText );
}
inline const WORD* Bk1AsUtf16( const wchar_t *pszText )
{
	return reinterpret_cast<const WORD*>( pszText );
}
inline WORD* Bk1AsUtf16( wchar_t *pszText )
{
	return reinterpret_cast<WORD*>( pszText );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __WIDESTRING_H__
