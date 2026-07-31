// The floating-point control word declared in bk1_msvc_fpu.h.
#include "bk1_msvc_fpu.h"

namespace {

// The word the caller last asked for. It is kept so that a read returns what
// was written, as it does on Windows, even for the bits arm64 has no register
// for.
unsigned int g_nControlWord = _MCW_EM | _PC_64 | _RC_NEAR;

#if defined( __aarch64__ )

// FPCR's exception-enable bits. A bit set means the exception traps; the
// MSVC mask bits mean the opposite, so the two are inverses of each other.
const unsigned long long FPCR_IOE = 1ull << 8;   // invalid operation
const unsigned long long FPCR_DZE = 1ull << 9;   // divide by zero
const unsigned long long FPCR_OFE = 1ull << 10;  // overflow
const unsigned long long FPCR_UFE = 1ull << 11;  // underflow
const unsigned long long FPCR_IXE = 1ull << 12;  // inexact
const unsigned long long FPCR_IDE = 1ull << 15;  // input denormal

// FPCR's rounding mode, bits 22-23.
const unsigned long long FPCR_RMODE_SHIFT = 22;
const unsigned long long FPCR_RMODE_MASK = 3ull << FPCR_RMODE_SHIFT;

unsigned long long ReadFpcr()
{
    unsigned long long v;
    __asm__ __volatile__( "mrs %0, fpcr" : "=r"( v ) );
    return v;
}

void WriteFpcr( unsigned long long v )
{
    __asm__ __volatile__( "msr fpcr, %0" : : "r"( v ) );
}

unsigned long long ReadFpsr()
{
    unsigned long long v;
    __asm__ __volatile__( "mrs %0, fpsr" : "=r"( v ) );
    return v;
}

void WriteFpsr( unsigned long long v )
{
    __asm__ __volatile__( "msr fpsr, %0" : : "r"( v ) );
}

void ApplyExceptionMasks( unsigned int nControlWord )
{
    unsigned long long fpcr = ReadFpcr();

    // a set mask bit means "do not raise", which is a cleared enable bit
    const struct { unsigned int nMask; unsigned long long nEnable; } map[] = {
        { _EM_INVALID,    FPCR_IOE },
        { _EM_ZERODIVIDE, FPCR_DZE },
        { _EM_OVERFLOW,   FPCR_OFE },
        { _EM_UNDERFLOW,  FPCR_UFE },
        { _EM_INEXACT,    FPCR_IXE },
        { _EM_DENORMAL,   FPCR_IDE },
    };
    for ( unsigned i = 0; i < sizeof( map ) / sizeof( map[0] ); ++i )
    {
        if ( ( nControlWord & map[i].nMask ) != 0 )
            fpcr &= ~map[i].nEnable;
        else
            fpcr |= map[i].nEnable;
    }

    unsigned long long nRound = 0;          // to nearest
    switch ( nControlWord & _MCW_RC )
    {
    case _RC_DOWN: nRound = 2; break;       // toward minus infinity
    case _RC_UP:   nRound = 1; break;       // toward plus infinity
    case _RC_CHOP: nRound = 3; break;       // toward zero
    default:       nRound = 0; break;
    }
    fpcr = ( fpcr & ~FPCR_RMODE_MASK ) | ( nRound << FPCR_RMODE_SHIFT );

    WriteFpcr( fpcr );
}

unsigned int ReadStatusWord()
{
    // FPSR's cumulative exception bits, in the MSVC word's spelling
    const unsigned long long fpsr = ReadFpsr();
    unsigned int nStatus = 0;
    if ( fpsr & ( 1ull << 0 ) ) nStatus |= _EM_INVALID;
    if ( fpsr & ( 1ull << 1 ) ) nStatus |= _EM_ZERODIVIDE;
    if ( fpsr & ( 1ull << 2 ) ) nStatus |= _EM_OVERFLOW;
    if ( fpsr & ( 1ull << 3 ) ) nStatus |= _EM_UNDERFLOW;
    if ( fpsr & ( 1ull << 4 ) ) nStatus |= _EM_INEXACT;
    if ( fpsr & ( 1ull << 7 ) ) nStatus |= _EM_DENORMAL;
    return nStatus;
}

void ClearStatusWord()
{
    WriteFpsr( ReadFpsr() & ~0x9Full );
}

#else

void ApplyExceptionMasks( unsigned int ) {}
unsigned int ReadStatusWord() { return 0; }
void ClearStatusWord() {}

#endif

}   // anonymous namespace

extern "C" {

unsigned int _controlfp( unsigned int nNew, unsigned int nMask )
{
    if ( nMask != 0 )
    {
        g_nControlWord = ( g_nControlWord & ~nMask ) | ( nNew & nMask );
        ApplyExceptionMasks( g_nControlWord );
    }
    return g_nControlWord;
}

unsigned int _control87( unsigned int nNew, unsigned int nMask )
{
    return _controlfp( nNew, nMask );
}

unsigned int _statusfp( void )
{
    return ReadStatusWord();
}

unsigned int _clearfp( void )
{
    const unsigned int nStatus = ReadStatusWord();
    ClearStatusWord();
    return nStatus;
}

}   // extern "C"
