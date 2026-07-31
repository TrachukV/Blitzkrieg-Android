#pragma once
// The Win32 threading, synchronisation and timing calls the engine makes.
//
// Modelled on the equivalent layer in the Blitzkrieg 2 Android port: handles
// are heap objects behind the opaque HANDLE typedef, and the primitives sit on
// the C++11 threading library rather than on pthreads directly.
#include "bk1_win32_types.h"

#include <dlfcn.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#define INFINITE          0xFFFFFFFF
#define WAIT_OBJECT_0     0x00000000
#define WAIT_TIMEOUT      0x00000102
#define WAIT_FAILED       0xFFFFFFFF
#define INVALID_HANDLE_VALUE ( (HANDLE)(long)-1 )

typedef DWORD ( *LPTHREAD_START_ROUTINE )( LPVOID );

typedef union _LARGE_INTEGER {
    struct { DWORD LowPart; LONG HighPart; };
    long long QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

// ---------------------------------------------------------------------------
// Critical sections. Recursive, as the Win32 originals are.
// ---------------------------------------------------------------------------
typedef struct _BK1_CRITICAL_SECTION {
    std::recursive_mutex mutex;
} CRITICAL_SECTION, *LPCRITICAL_SECTION;

inline void InitializeCriticalSection( LPCRITICAL_SECTION pSect ) { (void)pSect; }
inline void DeleteCriticalSection( LPCRITICAL_SECTION pSect ) { (void)pSect; }
inline void EnterCriticalSection( LPCRITICAL_SECTION pSect ) { pSect->mutex.lock(); }
inline void LeaveCriticalSection( LPCRITICAL_SECTION pSect ) { pSect->mutex.unlock(); }
inline BOOL TryEnterCriticalSection( LPCRITICAL_SECTION pSect )
{
    return pSect->mutex.try_lock() ? TRUE : FALSE;
}

// ---------------------------------------------------------------------------
// Waitable handles
// ---------------------------------------------------------------------------
namespace NBk1Win32 {

struct SHandleBase
{
    virtual ~SHandleBase() {}
    virtual DWORD Wait( DWORD dwMillis ) = 0;
};

struct SEventHandle : SHandleBase
{
    std::mutex mutex;
    std::condition_variable cond;
    bool bManualReset;
    bool bSignalled;

    SEventHandle( bool _bManualReset, bool _bInitial )
        : bManualReset( _bManualReset ), bSignalled( _bInitial ) {}

    void Set()
    {
        std::lock_guard<std::mutex> lock( mutex );
        bSignalled = true;
        if ( bManualReset )
            cond.notify_all();
        else
            cond.notify_one();
    }

    void Reset()
    {
        std::lock_guard<std::mutex> lock( mutex );
        bSignalled = false;
    }

    DWORD Wait( DWORD dwMillis ) override
    {
        std::unique_lock<std::mutex> lock( mutex );
        if ( dwMillis == INFINITE )
            cond.wait( lock, [this] { return bSignalled; } );
        else if ( !cond.wait_for( lock, std::chrono::milliseconds( dwMillis ),
                                  [this] { return bSignalled; } ) )
            return WAIT_TIMEOUT;
        if ( !bManualReset )
            bSignalled = false;      // auto-reset consumes the signal
        return WAIT_OBJECT_0;
    }
};

struct SThreadHandle : SHandleBase
{
    std::thread thread;

    explicit SThreadHandle( std::thread &&t ) : thread( std::move( t ) ) {}

    ~SThreadHandle() override
    {
        if ( thread.joinable() )
            thread.detach();
    }

    DWORD Wait( DWORD ) override
    {
        if ( thread.joinable() )
            thread.join();
        return WAIT_OBJECT_0;
    }
};

inline SHandleBase *FromHandle( HANDLE h )
{
    if ( h == 0 || h == INVALID_HANDLE_VALUE )
        return 0;
    return reinterpret_cast<SHandleBase *>( h );
}

}   // namespace NBk1Win32

inline HANDLE CreateEvent( LPVOID, BOOL bManualReset, BOOL bInitialState, LPCSTR )
{
    return reinterpret_cast<HANDLE>(
        new NBk1Win32::SEventHandle( bManualReset != FALSE, bInitialState != FALSE ) );
}

inline BOOL SetEvent( HANDLE h )
{
    NBk1Win32::SEventHandle *p =
        dynamic_cast<NBk1Win32::SEventHandle *>( NBk1Win32::FromHandle( h ) );
    if ( p == 0 )
        return FALSE;
    p->Set();
    return TRUE;
}

inline BOOL ResetEvent( HANDLE h )
{
    NBk1Win32::SEventHandle *p =
        dynamic_cast<NBk1Win32::SEventHandle *>( NBk1Win32::FromHandle( h ) );
    if ( p == 0 )
        return FALSE;
    p->Reset();
    return TRUE;
}

inline DWORD WaitForSingleObject( HANDLE h, DWORD dwMillis )
{
    NBk1Win32::SHandleBase *p = NBk1Win32::FromHandle( h );
    return p == 0 ? WAIT_FAILED : p->Wait( dwMillis );
}

inline BOOL CloseHandle( HANDLE h )
{
    NBk1Win32::SHandleBase *p = NBk1Win32::FromHandle( h );
    if ( p == 0 )
        return FALSE;
    delete p;
    return TRUE;
}

inline HANDLE CreateThread( LPVOID, size_t, LPTHREAD_START_ROUTINE pStart,
                            LPVOID pParam, DWORD, DWORD *pThreadId )
{
    if ( pStart == 0 )
        return 0;
    std::thread worker( [pStart, pParam] { pStart( pParam ); } );
    if ( pThreadId != 0 )
        *pThreadId = 0;
    return reinterpret_cast<HANDLE>( new NBk1Win32::SThreadHandle( std::move( worker ) ) );
}

// ---------------------------------------------------------------------------
// Dynamic loading. Misc/Win32Helper.h's CDLLHandle wraps these; the engine
// used them for the renderer and sound DLLs, which the port replaces with
// linked-in code, so loads are expected to fail and be handled.
// ---------------------------------------------------------------------------
inline HMODULE LoadLibraryA( const char *pszName )
{
    return pszName == 0 ? 0 : (HMODULE)dlopen( pszName, RTLD_NOW | RTLD_LOCAL );
}

inline BOOL FreeLibrary( HMODULE hModule )
{
    return ( hModule != 0 && dlclose( hModule ) == 0 ) ? TRUE : FALSE;
}

inline void *GetProcAddress( HMODULE hModule, const char *pszName )
{
    return ( hModule == 0 || pszName == 0 ) ? 0 : dlsym( hModule, pszName );
}

#define LoadLibrary LoadLibraryA

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------
inline DWORD GetTickCount()
{
    const std::chrono::steady_clock::duration now =
        std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<DWORD>(
        std::chrono::duration_cast<std::chrono::milliseconds>( now ).count() );
}

inline void Sleep( DWORD dwMillis )
{
    std::this_thread::sleep_for( std::chrono::milliseconds( dwMillis ) );
}

// The engine only ever uses these as a monotonic pair, so nanoseconds serve.
inline BOOL QueryPerformanceFrequency( LARGE_INTEGER *pFreq )
{
    if ( pFreq == 0 )
        return FALSE;
    pFreq->QuadPart = 1000000000LL;
    return TRUE;
}

inline BOOL QueryPerformanceCounter( LARGE_INTEGER *pCount )
{
    if ( pCount == 0 )
        return FALSE;
    const std::chrono::steady_clock::duration now =
        std::chrono::steady_clock::now().time_since_epoch();
    pCount->QuadPart =
        std::chrono::duration_cast<std::chrono::nanoseconds>( now ).count();
    return TRUE;
}
