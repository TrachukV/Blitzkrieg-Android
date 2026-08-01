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

namespace NBugSlayer {

namespace {

// The engine registers commands here that it wants run if the process is
// dying -- the emergency save is one. The list is kept because registration
// has to work and the engine reasons about it.
std::vector<IBaseCommand *> g_emergencyCommands;

}   // anonymous namespace

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
