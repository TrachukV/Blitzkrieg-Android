// The five entry points the game uses out of BugSlay.
//
// That module is a Windows crash handler: structured exception handling, a
// DbgHelp stack walk, a scan of the process's loaded modules, and a dialog
// offering to send a report. Almost none of it has an Android equivalent, and
// what does exist -- tombstones, the platform crash reporter -- already does
// the job better than a 2003 reimplementation would.
//
// So the module is not built. What is built is this: the parts the engine
// actually calls, and nothing else.
#include "bk1_win32_types.h"

#include <android/log.h>

#include <stdio.h>
#include <stdlib.h>

#include <vector>

#define LOG_TAG "Blitzkrieg"

// The engine spells this "interface", which is its own macro for struct.
struct IBaseCommand;

// From BugSlay/BugSlayer.h, which is not included here because almost all of
// it is Windows crash handling; only the report result matters.
// From BugSlayer.h. Declared here rather than including that header, because
// almost all of it is Windows crash handling that this file exists to avoid.
struct _EXCEPTION_POINTERS;
typedef LONG (STDCALL *PFNCHFILTFN)( struct _EXCEPTION_POINTERS *pExPtrs );

enum EBSUReport
{
    BSU_ABORT,
    BSU_DEBUG,
    BSU_IGNORE,
    BSU_CONTINUE,
};

namespace NBugSlayer {

namespace {

// The engine registers commands here that it wants run if the process is
// dying -- the emergency save is one. The list is kept because registration
// has to work and the engine reasons about it.
std::vector<IBaseCommand *> g_emergencyCommands;

}   // anonymous namespace

// The assert report, and the reason the port needs it.
//
// NI_ASSERT_TF( condition, text, statement ) is not only a diagnostic. Its
// third argument is the engine's error handling -- "return 0" when a file is
// missing, "return false" when a format is wrong -- and the macro runs it when
// this function answers BSU_CONTINUE.
//
// Without _DO_ASSERT the whole macro expands to {} and that statement is
// discarded with it. Every handled error in the engine then becomes an
// unhandled one: NDB::OpenDataTable checks its stream for null, is stripped of
// its return, and dereferences it one frame later. That is exactly how a
// missing consts.xml turned into
//
//     signal 11, CDataTableXML::Open+136, fault addr 0x0
//
// The comment above those macros in BugSlayer.h says they are "meant to be
// enabled in release builds", and the shipped game was built that way. So the
// port defines _DO_ASSERT too, and this is what the macro calls.
//
// BSU_CONTINUE is the only answer that makes sense here. Windows put up a
// dialog offering to break into a debugger, ignore, or abort; there is nobody
// to ask on a phone, and continuing is what runs the engine's own recovery.
EBSUReport STDCALL ReportAssert( const char *pszCondition, const char *pszDescription,
                                 const char *pszFileName, int nLineNumber, bool )
{
    __android_log_print( ANDROID_LOG_WARN, LOG_TAG, "assert: %s%s%s (%s:%d)",
                         ( pszCondition != 0 ) ? pszCondition : "",
                         ( pszDescription != 0 && pszDescription[0] != 0 ) ? " -- " : "",
                         ( pszDescription != 0 ) ? pszDescription : "",
                         ( pszFileName != 0 ) ? pszFileName : "?", nLineNumber );
    return BSU_CONTINUE;
}

EBSUReport STDCALL ReportAssertHR( HRESULT result, const char *pszDescription,
                                   const char *pszFileName, int nLineNumber, bool )
{
    __android_log_print( ANDROID_LOG_WARN, LOG_TAG, "assert: hr 0x%08lx%s%s (%s:%d)",
                         (unsigned long)result,
                         ( pszDescription != 0 && pszDescription[0] != 0 ) ? " -- " : "",
                         ( pszDescription != 0 ) ? pszDescription : "",
                         ( pszFileName != 0 ) ? pszFileName : "?", nLineNumber );
    return BSU_CONTINUE;
}

// Kept, not run.
//
// On Windows these fired from an exception filter, on a thread that had
// already stopped doing anything else. The Android equivalent would be a
// SIGSEGV handler, and running the engine's save path from one is not
// something that can be made safe: it allocates, it takes locks the crashing
// thread may already hold, and it opens files. A hang there loses the crash
// report as well as the save.
//
// Saying that plainly is better than a handler that appears to protect the
// player's game and deadlocks instead. The autosave the engine already keeps
// is what covers this case.
void STDCALL AddEmergencyCommand( IBaseCommand *pCommand )
{
    if ( pCommand != 0 )
        g_emergencyCommands.push_back( pCommand );
}

void STDCALL RemoveAllEmergencyCommands()
{
    g_emergencyCommands.clear();
}

// Windows walked its own heap. Linux keeps the same numbers in /proc, and the
// engine only ever logs them.
void STDCALL MemSystemDumpStats()
{
    FILE *pFile = fopen( "/proc/self/statm", "r" );
    if ( pFile == 0 )
        return;

    unsigned long nTotalPages = 0, nResidentPages = 0;
    if ( fscanf( pFile, "%lu %lu", &nTotalPages, &nResidentPages ) == 2 )
    {
        const unsigned long nPageSize = 4096;
        __android_log_print( ANDROID_LOG_INFO, LOG_TAG,
                             "memory: %lu MB mapped, %lu MB resident",
                             nTotalPages * nPageSize / ( 1024 * 1024 ),
                             nResidentPages * nPageSize / ( 1024 * 1024 ) );
    }
    fclose( pFile );
}

// A raw allocator the engine uses where its own must not be reentered.
void* __cdecl FastDumbAlloc( int nSize )
{
    return ( nSize > 0 ) ? malloc( (size_t)nSize ) : 0;
}

bool __cdecl FastDumbFree( void *pData )
{
    if ( pData == 0 )
        return false;
    free( pData );
    return true;
}

}   // namespace NBugSlayer

// The crash filter the assert's abort path clears before it exits. Windows
// installed a structured-exception filter; there is none here, and Android's
// own crash reporting is what handles a fault. The signature matches
// BugSlayer.h exactly so the engine's calls resolve.
BOOL STDCALL SetCrashHandlerFilter( PFNCHFILTFN )
{
    return TRUE;
}
