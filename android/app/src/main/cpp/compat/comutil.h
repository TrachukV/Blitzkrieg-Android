#pragma once
// MSVC's COM utility header. The engine's property system stores values in
// VARIANT and names strings with _bstr_t. Misc/Manipulator.cpp reads the union
// members directly (vt, byref, bstrVal, lVal and the rest), so the layout here
// follows the Windows one rather than simplifying it.
#include "bk1_win32_types.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// BSTR
// ---------------------------------------------------------------------------
// OLECHAR is UTF-16 on Windows. WORD keeps that width here, where wchar_t is
// 32 bits.
typedef WORD     OLECHAR;
typedef OLECHAR *BSTR;
typedef long     SCODE;
typedef double   DATE;
typedef short    VARIANT_BOOL;

#define VARIANT_TRUE  ( (VARIANT_BOOL)-1 )
#define VARIANT_FALSE ( (VARIANT_BOOL)0 )

typedef struct tagCY {
    unsigned long Lo;
    long          Hi;
} CY;

struct IUnknown;
struct IDispatch;

// A BSTR points past a four-byte length prefix, as on Windows, so that
// SysStringByteLen can find the length from the pointer alone.
inline BSTR SysAllocStringByteLen( const char *pData, UINT nBytes )
{
    unsigned char *pBlock = (unsigned char *)malloc( 4 + nBytes + 2 );
    if ( pBlock == 0 )
        return 0;
    memcpy( pBlock, &nBytes, 4 );
    if ( pData != 0 && nBytes != 0 )
        memcpy( pBlock + 4, pData, nBytes );
    pBlock[4 + nBytes] = 0;
    pBlock[4 + nBytes + 1] = 0;
    return (BSTR)( pBlock + 4 );
}

inline UINT SysStringByteLen( BSTR bstr )
{
    if ( bstr == 0 )
        return 0;
    UINT nBytes = 0;
    memcpy( &nBytes, (const unsigned char *)bstr - 4, 4 );
    return nBytes;
}

inline UINT SysStringLen( BSTR bstr ) { return SysStringByteLen( bstr ) / 2; }

inline BSTR SysAllocString( const OLECHAR *psz )
{
    if ( psz == 0 )
        return 0;
    UINT nLen = 0;
    while ( psz[nLen] != 0 )
        ++nLen;
    return SysAllocStringByteLen( (const char *)psz, nLen * 2 );
}

inline void SysFreeString( BSTR bstr )
{
    if ( bstr != 0 )
        free( (unsigned char *)bstr - 4 );
}

// ---------------------------------------------------------------------------
// UTF-16 <-> wchar_t
// ---------------------------------------------------------------------------
// On Windows these are the same width and the engine moves between BSTR and
// std::wstring by assignment. Here wchar_t is 32 bits, so the two spellings are
// different encodings and the conversion is real.
inline std::wstring Bk1Utf16ToWide( const OLECHAR *psz, UINT nUnits )
{
    std::wstring res;
    res.reserve( nUnits );
    for ( UINT i = 0; i < nUnits; ++i )
    {
        unsigned int cp = psz[i];
        if ( cp >= 0xD800 && cp <= 0xDBFF && i + 1 < nUnits &&
             psz[i + 1] >= 0xDC00 && psz[i + 1] <= 0xDFFF )
        {
            cp = 0x10000 + ( ( cp - 0xD800 ) << 10 ) + ( psz[i + 1] - 0xDC00 );
            ++i;
        }
        res.push_back( (wchar_t)cp );
    }
    return res;
}

inline BSTR SysAllocString( const wchar_t *psz )
{
    if ( psz == 0 )
        return 0;

    // a vector rather than a basic_string: libc++ has no char_traits for
    // 'unsigned short'
    std::vector<OLECHAR> units;
    for ( UINT i = 0; psz[i] != 0; ++i )
    {
        const unsigned int cp = (unsigned int)psz[i];
        if ( cp >= 0x10000 )
        {
            const unsigned int v = cp - 0x10000;
            units.push_back( (OLECHAR)( 0xD800 + ( v >> 10 ) ) );
            units.push_back( (OLECHAR)( 0xDC00 + ( v & 0x3FF ) ) );
        }
        else
        {
            units.push_back( (OLECHAR)cp );
        }
    }
    return SysAllocStringByteLen( units.empty() ? 0 : (const char *)&units[0],
                                  (UINT)( units.size() * 2 ) );
}

// ---------------------------------------------------------------------------
// VARIANT
// ---------------------------------------------------------------------------
enum VARENUM {
    VT_EMPTY = 0, VT_NULL = 1, VT_I2 = 2, VT_I4 = 3, VT_R4 = 4, VT_R8 = 5,
    VT_CY = 6, VT_DATE = 7, VT_BSTR = 8, VT_DISPATCH = 9, VT_ERROR = 10,
    VT_BOOL = 11, VT_VARIANT = 12, VT_UNKNOWN = 13, VT_DECIMAL = 14,
    VT_I1 = 16, VT_UI1 = 17, VT_UI2 = 18, VT_UI4 = 19, VT_I8 = 20, VT_UI8 = 21,
    VT_INT = 22, VT_UINT = 23, VT_VOID = 24, VT_BYREF = 0x4000
};

typedef unsigned short VARTYPE;

typedef struct tagVARIANT {
    VARTYPE vt;
    WORD    wReserved1;
    WORD    wReserved2;
    WORD    wReserved3;
    union {
        LONG          lVal;
        BYTE          bVal;
        short         iVal;
        int           intVal;
        unsigned int  uintVal;
        unsigned long ulVal;
        long long     llVal;
        unsigned long long ullVal;
        FLOAT         fltVal;
        double        dblVal;
        VARIANT_BOOL  boolVal;
        SCODE         scode;
        CY            cyVal;
        DATE          date;
        BSTR          bstrVal;
        IUnknown     *punkVal;
        IDispatch    *pdispVal;
        void         *byref;
    };
} VARIANT, *LPVARIANT;

#define V_VT( x )     ( (x)->vt )
#define V_BOOL( x )   ( (x)->boolVal )
#define V_I4( x )     ( (x)->lVal )
#define V_R4( x )     ( (x)->fltVal )
#define V_R8( x )     ( (x)->dblVal )
#define V_BSTR( x )   ( (x)->bstrVal )

inline void VariantInit( VARIANT *pVar )
{
    if ( pVar != 0 )
        memset( pVar, 0, sizeof( *pVar ) );
}

inline HRESULT VariantClear( VARIANT *pVar )
{
    if ( pVar == 0 )
        return 0;
    if ( pVar->vt == VT_BSTR )
        SysFreeString( pVar->bstrVal );
    memset( pVar, 0, sizeof( *pVar ) );
    return 0;
}

// ---------------------------------------------------------------------------
// _bstr_t
// ---------------------------------------------------------------------------
// Holds both spellings of the string: the engine passes it where a narrow
// string is wanted and assigns it to VARIANT::bstrVal.
class _bstr_t
{
public:
    _bstr_t() : bstr_( 0 ) {}

    _bstr_t( const char *pszValue ) : narrow_( pszValue != 0 ? pszValue : "" )
    {
        Build();
    }

    // Takes any NUL-terminated UTF-16 string, not just a real BSTR: callers
    // pass bk1_wstring::c_str() too, which has no length prefix to read. The
    // length is scanned, which is what _bstr_t( const wchar_t* ) does on
    // Windows.
    _bstr_t( const OLECHAR *psz ) : bstr_( 0 )
    {
        UINT nLen = 0;
        if ( psz != 0 )
        {
            while ( psz[nLen] != 0 )
                ++nLen;
        }
        const std::wstring wide = Bk1Utf16ToWide( psz, nLen );
        narrow_.reserve( wide.size() );
        for ( size_t i = 0; i < wide.size(); ++i )
        {
            const unsigned int cp = (unsigned int)wide[i];
            narrow_.push_back( cp < 0x100 ? (char)cp : '?' );
        }
        bstr_ = SysAllocStringByteLen( (const char *)psz, nLen * 2 );
    }

    _bstr_t( const _bstr_t &other ) : narrow_( other.narrow_ ) { Build(); }

    // StreamIO/OptionSystemInternal.cpp spells a variant's value as a string
    // with bstr_t( var ). Defined below, once _variant_t is complete.
    _bstr_t( const class _variant_t &var );

    _bstr_t &operator=( const _bstr_t &other )
    {
        if ( this != &other )
        {
            SysFreeString( bstr_ );
            narrow_ = other.narrow_;
            Build();
        }
        return *this;
    }

    ~_bstr_t() { SysFreeString( bstr_ ); }

    const char *operator*() const { return narrow_.c_str(); }
    operator const char *() const { return narrow_.c_str(); }
    // 'std::string sz = someBstr' is how the engine reads it, and going
    // through 'const char*' would be two user conversions, which C++ does not
    // apply in one step.
    operator std::string() const { return narrow_; }
    // The engine's wide text is UTF-16 and so is a BSTR; the two are the
    // same width here and differ only in spelling.
    operator const wchar_t *() const { return reinterpret_cast<const wchar_t *>( bstr_ ); }
    operator BSTR() const { return bstr_; }
    operator std::wstring() const
    {
        return Bk1Utf16ToWide( bstr_, SysStringLen( bstr_ ) );
    }

    UINT length() const { return (UINT)narrow_.size(); }

private:
    // The engine's _bstr_t values are ASCII literals, so widening a byte at a
    // time is exact for them.
    void Build()
    {
        const size_t nLen = narrow_.size();
        bstr_ = SysAllocStringByteLen( 0, (UINT)( nLen * 2 ) );
        for ( size_t i = 0; i < nLen; ++i )
            bstr_[i] = (OLECHAR)(unsigned char)narrow_[i];
    }

    std::string narrow_;
    BSTR        bstr_;
};

// ---------------------------------------------------------------------------
// _variant_t
// ---------------------------------------------------------------------------
class _variant_t : public VARIANT
{
public:
    _variant_t() { VariantInit( this ); }

    _variant_t( const VARIANT &var ) { *static_cast<VARIANT *>( this ) = var; }

    _variant_t( int nValue ) { VariantInit( this ); vt = VT_INT; intVal = nValue; }
    _variant_t( long nValue ) { VariantInit( this ); vt = VT_I4; lVal = nValue; }
    _variant_t( short nValue ) { VariantInit( this ); vt = VT_I2; iVal = nValue; }
    _variant_t( float fValue ) { VariantInit( this ); vt = VT_R4; fltVal = fValue; }
    _variant_t( double fValue ) { VariantInit( this ); vt = VT_R8; dblVal = fValue; }
    _variant_t( bool bValue )
    {
        VariantInit( this );
        vt = VT_BOOL;
        boolVal = bValue ? VARIANT_TRUE : VARIANT_FALSE;
    }

    _variant_t( const _bstr_t &str )
    {
        VariantInit( this );
        AssignBstr( str );
    }

    // An exact match for a narrow literal, without which assigning one is
    // ambiguous between the copy assignment and the _bstr_t one.
    _variant_t( const char *psz )
    {
        VariantInit( this );
        AssignBstr( _bstr_t( psz ) );
    }

    _variant_t &operator=( const char *psz )
    {
        VariantClear( this );
        AssignBstr( _bstr_t( psz ) );
        return *this;
    }

    _variant_t &operator=( const _bstr_t &str )
    {
        VariantClear( this );
        AssignBstr( str );
        return *this;
    }

    // Coerced by the variant's own type rather than by reading whichever union
    // member the caller asked for: a VT_R4 read as a long would otherwise
    // return the float's bit pattern.
    double AsDouble() const
    {
        switch ( vt )
        {
        case VT_EMPTY:
        case VT_NULL:   return 0.0;
        case VT_R4:     return (double)fltVal;
        case VT_R8:     return dblVal;
        case VT_BOOL:   return boolVal != VARIANT_FALSE ? 1.0 : 0.0;
        case VT_UI1:    return (double)bVal;
        case VT_I2:     return (double)iVal;
        case VT_INT:    return (double)intVal;
        case VT_UI4:    return (double)ulVal;
        default:        return (double)lVal;
        }
    }

    operator int() const { return (int)AsDouble(); }
    operator long() const { return (long)AsDouble(); }
    operator short() const { return (short)AsDouble(); }
    operator float() const { return (float)AsDouble(); }
    operator double() const { return AsDouble(); }
    operator bool() const { return AsDouble() != 0.0; }

    // Without these the implicit conversions above make 'a == b' ambiguous
    // against the built-in operators.
    bool operator==( const _variant_t &other ) const
    {
        if ( vt != other.vt )
            return false;
        if ( vt == VT_BSTR )
        {
            const UINT n = SysStringByteLen( bstrVal );
            return n == SysStringByteLen( other.bstrVal ) &&
                   memcmp( bstrVal, other.bstrVal, n ) == 0;
        }
        return memcmp( &lVal, &other.lVal, sizeof( CY ) ) == 0;
    }

    bool operator!=( const _variant_t &other ) const { return !( *this == other ); }

private:
    // The variant owns its BSTR, so it takes a copy rather than the _bstr_t's.
    void AssignBstr( const _bstr_t &str )
    {
        const BSTR src = (BSTR)str;
        vt = VT_BSTR;
        bstrVal = SysAllocStringByteLen( (const char *)src, SysStringByteLen( src ) );
    }

public:

    void Clear() { VariantClear( this ); }
};

// Spells a variant's value as a string, the way _bstr_t does on Windows.
inline _bstr_t::_bstr_t( const _variant_t &var ) : bstr_( 0 )
{
    char buff[64];
    switch ( var.vt )
    {
    case VT_BSTR:
        {
            const std::wstring wide =
                Bk1Utf16ToWide( var.bstrVal, SysStringLen( var.bstrVal ) );
            narrow_.reserve( wide.size() );
            for ( size_t i = 0; i < wide.size(); ++i )
            {
                const unsigned int cp = (unsigned int)wide[i];
                narrow_.push_back( cp < 0x100 ? (char)cp : '?' );
            }
        }
        break;
    case VT_EMPTY:
    case VT_NULL:
        break;
    case VT_BOOL:
        narrow_ = var.boolVal != VARIANT_FALSE ? "true" : "false";
        break;
    case VT_R4:
        snprintf( buff, sizeof( buff ), "%g", (double)var.fltVal );
        narrow_ = buff;
        break;
    case VT_R8:
        snprintf( buff, sizeof( buff ), "%g", var.dblVal );
        narrow_ = buff;
        break;
    case VT_UI1:
        snprintf( buff, sizeof( buff ), "%u", (unsigned)var.bVal );
        narrow_ = buff;
        break;
    case VT_I2:
        snprintf( buff, sizeof( buff ), "%d", (int)var.iVal );
        narrow_ = buff;
        break;
    case VT_INT:
        snprintf( buff, sizeof( buff ), "%d", var.intVal );
        narrow_ = buff;
        break;
    default:
        snprintf( buff, sizeof( buff ), "%ld", (long)var.lVal );
        narrow_ = buff;
        break;
    }
    Build();
}

typedef _variant_t variant_t;
typedef _bstr_t    bstr_t;
