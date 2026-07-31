#include "StdAfx.h"
#include "HPTimer.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NHPTimer;
static double fProcFreq1 = 1;
////////////////////////////////////////////////////////////////////////////////////////////////////
double NHPTimer::GetSeconds( const NHPTimer::STime &a )
{
	return (static_cast<double>(a)) * fProcFreq1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Time counters
////////////////////////////////////////////////////////////////////////////////////////////////////
static inline void GetCounter( int64 *pTime )
{
#if defined( _MSC_VER ) && defined( _M_IX86 )
	__asm
	{
		rdtsc
		mov esi, pTime
		mov [esi], eax
		mov [esi+4], edx
	}
#else
	// There is no cycle counter available from user space on arm64, so the
	// monotonic clock stands in and the counter's unit becomes the nanosecond.
	// InitHPTimer sets the scale to match instead of calibrating against
	// QueryPerformanceCounter.
	const std::chrono::steady_clock::duration now =
		std::chrono::steady_clock::now().time_since_epoch();
	*pTime = std::chrono::duration_cast<std::chrono::nanoseconds>( now ).count();
#endif
}
////////////////////////////////////////////////////////////////////////////////////////////////////
double NHPTimer::GetClockRate()
{
	return 1 / fProcFreq1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void NHPTimer::GetTime( STime *pTime )
{
	GetCounter( pTime );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
double NHPTimer::GetTimePassed( STime *pTime )
{
	STime old(*pTime );
	GetTime( pTime );
	return GetSeconds( *pTime - old );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void InitHPTimer()
{
#if !( defined( _MSC_VER ) && defined( _M_IX86 ) )
	// GetCounter counts nanoseconds here, so the scale is known up front and
	// the calibration loop below has nothing to measure.
	fProcFreq1 = 1e-9;
	return;
#else
	int64 freq, start, fin;
	QueryPerformanceFrequency( (_LARGE_INTEGER*) &freq );
	double fTStart, fTFinish, fPassed;
	STime t;
	for(;;)
	{
		DWORD dwStart = GetTickCount();
		GetTime( &t );
		QueryPerformanceCounter( (_LARGE_INTEGER*) &start );
		Sleep( 100 );
		fPassed = GetTimePassed( &t );
		QueryPerformanceCounter( (_LARGE_INTEGER*) &fin );
		DWORD dwFinish = GetTickCount();

		fTStart = double( start );
		fTFinish = double( fin );
		float fTickTime = ( dwFinish - dwStart ) / 1024.0f;
		float fPCTime = (float)( ( fTFinish - fTStart ) / static_cast<double>( freq ) );
		if ( fabs( fTickTime - fPCTime ) < 0.05f )
			break;
	}
	double fProcFreq = (fPassed) * (static_cast<double>( freq )) / (fTFinish-fTStart);
	fProcFreq1 = 1 / fProcFreq;
	//cout << "freq = " << fpProcFreq / 1000000 <<  "Mhz" << endl;
#endif
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// ��� ��������������� ��������� ��� �������������� ������������� HP timer'�
struct SHPTimerInit
{
	SHPTimerInit() { InitHPTimer(); }
};
static SHPTimerInit hptInit;
////////////////////////////////////////////////////////////////////////////////////////////////////
