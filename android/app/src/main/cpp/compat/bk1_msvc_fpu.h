#pragma once
// MSVC's floating-point predicates, which the engine spells with the leading
// underscore. It guards its physics with _finite -- a coordinate that has gone
// to infinity -- from inside NI_ASSERT, so these only became reachable once
// _DO_ASSERT was defined and those asserts started compiling.
#ifdef __cplusplus
#include <cmath>
inline int _finite( double d ) { return std::isfinite( d ) ? 1 : 0; }
inline int _isnan( double d )  { return std::isnan( d ) ? 1 : 0; }
#endif

// MSVC's floating-point control word, which the engine sets before computing
// the checksums a multiplayer session compares.
//
// On x86 this masked every floating-point exception and dropped the x87 unit
// to 24-bit precision, so that intermediate results were not silently carried
// at 80 bits and the same map produced the same checksum on every machine.
//
// arm64 has neither problem to solve. It has no extended-precision registers:
// a float computation is carried at single precision because that is the only
// width the instruction has, which is what _PC_24 was asking for. Exception
// masking does have an equivalent -- the enable bits in FPCR -- and that part
// is applied.
#include "bk1_win32_types.h"

// --- exception masks ---
#define _EM_INVALID     0x00000010
#define _EM_DENORMAL    0x00080000
#define _EM_ZERODIVIDE  0x00000008
#define _EM_OVERFLOW    0x00000004
#define _EM_UNDERFLOW   0x00000002
#define _EM_INEXACT     0x00000001
#define _MCW_EM         0x0008001F

// --- precision control, which arm64 has no register for ---
#define _PC_24          0x00020000
#define _PC_53          0x00010000
#define _PC_64          0x00000000
#define _MCW_PC         0x00030000

// --- rounding control ---
#define _RC_NEAR        0x00000000
#define _RC_DOWN        0x00000100
#define _RC_UP          0x00000200
#define _RC_CHOP        0x00000300
#define _MCW_RC         0x00000300

#ifdef __cplusplus
extern "C" {
#endif

// Sets the bits of 'nNew' selected by 'nMask' and returns the resulting word.
unsigned int _controlfp( unsigned int nNew, unsigned int nMask );
unsigned int _control87( unsigned int nNew, unsigned int nMask );
unsigned int _clearfp( void );
unsigned int _statusfp( void );

#ifdef __cplusplus
}
#endif
