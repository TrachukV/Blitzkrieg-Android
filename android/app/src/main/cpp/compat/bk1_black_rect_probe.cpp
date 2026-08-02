#include "bk1_black_rect_probe.h"

#include <android/log.h>
#include <sys/system_properties.h>
#include <unwind.h>
#include <dlfcn.h>

#include <cmath>

namespace
{

// Walking the stack costs far more than the comparison that guards it, so the
// guard is the geometry itself: only the rectangle that was actually measured
// gets this far.
struct SUnwindState
{
	void **ppFrames;
	int nCapacity;
	int nCount;
};

_Unwind_Reason_Code CollectFrame( struct _Unwind_Context *pContext, void *pArg )
{
	SUnwindState *pState = static_cast<SUnwindState*>( pArg );
	const uintptr_t nPC = _Unwind_GetIP( pContext );
	if ( nPC != 0 )
	{
		if ( pState->nCount >= pState->nCapacity )
			return _URC_END_OF_STACK;
		pState->ppFrames[pState->nCount++] = reinterpret_cast<void*>( nPC );
	}
	return _URC_NO_REASON;
}

// The property is read afresh every time. A cached switch is what made two
// earlier instrumentation attempts report silence that was indistinguishable
// from a finding.
bool IsPropertySet( const char *pszName )
{
	char szValue[PROP_VALUE_MAX] = { 0 };
	if ( __system_property_get( pszName, szValue ) <= 0 )
		return false;
	return szValue[0] != '0' && szValue[0] != '\0';
}

bool IsProbeEnabled()
{
	return IsPropertySet( "debug.blitzkrieg.blackrect" );
}

bool NearlyEqual( float fA, float fB )
{
	return std::fabs( fA - fB ) <= 1.0f;
}

} // anonymous namespace

void Bk1ReportBlackScreenRect( float rminx, float rminy, float rmaxx, float rmaxy,
                               unsigned long dwColour,
                               float sminx, float sminy, float smaxx, float smaxy )
{
	// Opaque, and black in all three colour components: exactly what the frame
	// trace recorded for the last draw of the black mission.
	const unsigned long dwAlpha = ( dwColour >> 24 ) & 0xff;
	if ( dwAlpha != 0xff || ( dwColour & 0x00ffffff ) != 0 )
		return;
	// Covering the screen, within a pixel on every edge.
	if ( !NearlyEqual( rminx, sminx ) || !NearlyEqual( rminy, sminy ) ||
			 !NearlyEqual( rmaxx, smaxx ) || !NearlyEqual( rmaxy, smaxy ) )
		return;
	if ( !IsProbeEnabled() )
		return;

	void *pFrames[16] = { 0 };
	SUnwindState state = { pFrames, 16, 0 };
	_Unwind_Backtrace( &CollectFrame, &state );

	// Offsets from the library's own load address, so the numbers can be fed
	// straight to addr2line against the unstripped build.
	Dl_info selfInfo;
	uintptr_t nBase = 0;
	if ( dladdr( reinterpret_cast<void*>( &Bk1ReportBlackScreenRect ), &selfInfo ) != 0 )
		nBase = reinterpret_cast<uintptr_t>( selfInfo.dli_fbase );

	__android_log_print( ANDROID_LOG_INFO, "Blitzkrieg.black",
											 "full-screen opaque black rect: colour=0x%08lx rect=%.1f,%.1f-%.1f,%.1f base=%p frames=%d",
											 dwColour, rminx, rminy, rmaxx, rmaxy,
											 reinterpret_cast<void*>( nBase ), state.nCount );

	for ( int i = 0; i < state.nCount; ++i )
	{
		const uintptr_t nPC = reinterpret_cast<uintptr_t>( pFrames[i] );
		Dl_info info;
		const char *pszSymbol = "";
		const char *pszObject = "";
		if ( dladdr( pFrames[i], &info ) != 0 )
		{
			if ( info.dli_sname != 0 )
				pszSymbol = info.dli_sname;
			if ( info.dli_fname != 0 )
				pszObject = info.dli_fname;
		}
		__android_log_print( ANDROID_LOG_INFO, "Blitzkrieg.black",
												 "  #%02d  pc=%p  off=0x%lx  %s  %s",
												 i, pFrames[i],
												 (unsigned long)( nBase != 0 && nPC > nBase ? nPC - nBase : nPC ),
												 pszSymbol, pszObject );
	}
}

void Bk1TraceAlwaysObjects( const char *pszWhat, const void *pObject, int nSize, const void *pScene )
{
	if ( !IsPropertySet( "debug.blitzkrieg.always" ) )
		return;
	__android_log_print( ANDROID_LOG_INFO, "Blitzkrieg.always",
											 "%-10s obj=%p size=%d scene=%p",
											 pszWhat, pObject, nSize, pScene );
}

void Bk1TracePath( const char *pszTag, const char *pszPath, int nValue )
{
	if ( !IsPropertySet( "debug.blitzkrieg.saveload" ) )
		return;
	__android_log_print( ANDROID_LOG_INFO, "Blitzkrieg.saveload",
											 "%-14s n=%d path=[%s]",
											 pszTag, nValue, pszPath ? pszPath : "(null)" );
}

void Bk1TraceBacktrace( const char *pszTag )
{
	if ( !IsPropertySet( "debug.blitzkrieg.saveload" ) )
		return;

	void *pFrames[16] = { 0 };
	SUnwindState state = { pFrames, 16, 0 };
	_Unwind_Backtrace( &CollectFrame, &state );

	Dl_info selfInfo;
	uintptr_t nBase = 0;
	if ( dladdr( reinterpret_cast<void*>( &Bk1TraceBacktrace ), &selfInfo ) != 0 )
		nBase = reinterpret_cast<uintptr_t>( selfInfo.dli_fbase );

	__android_log_print( ANDROID_LOG_INFO, "Blitzkrieg.saveload",
											 "%s: base=%p frames=%d", pszTag,
											 reinterpret_cast<void*>( nBase ), state.nCount );
	for ( int i = 0; i < state.nCount; ++i )
	{
		const uintptr_t nPC = reinterpret_cast<uintptr_t>( pFrames[i] );
		__android_log_print( ANDROID_LOG_INFO, "Blitzkrieg.saveload",
												 "  #%02d off=0x%lx", i,
												 (unsigned long)( nBase != 0 && nPC > nBase ? nPC - nBase : nPC ) );
	}
}

void Bk1ReportBadCast( const char *pszType )
{
	if ( !IsPropertySet( "debug.blitzkrieg.badcast" ) )
		return;
	__android_log_print( ANDROID_LOG_WARN, "Blitzkrieg.badcast",
											 "wrong checked_cast from %s", pszType ? pszType : "(unknown)" );
}
