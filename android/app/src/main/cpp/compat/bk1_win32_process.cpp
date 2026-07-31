// Process creation, declared in bk1_win32_process.h, over fork and exec.
#include "bk1_win32_process.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <string>
#include <vector>

namespace {

// A child process behind the same waitable handle as an event or a thread, so
// WaitForSingleObject and CloseHandle work on it unchanged.
struct SProcessHandle : NBk1Win32::SHandleBase
{
    pid_t pid;
    int   nExitCode;
    bool  bReaped;

    explicit SProcessHandle( pid_t p ) : pid( p ), nExitCode( STILL_ACTIVE ), bReaped( false ) {}

    ~SProcessHandle() override
    {
        // Reap the child if the caller never waited, so it does not linger.
        if ( !bReaped && pid > 0 )
        {
            int nStatus = 0;
            waitpid( pid, &nStatus, WNOHANG );
        }
    }

    DWORD Wait( DWORD dwMillis ) override
    {
        if ( bReaped )
            return WAIT_OBJECT_0;

        if ( dwMillis == INFINITE )
        {
            int nStatus = 0;
            if ( waitpid( pid, &nStatus, 0 ) < 0 )
                return WAIT_FAILED;
            nExitCode = WIFEXITED( nStatus ) ? WEXITSTATUS( nStatus ) : -1;
            bReaped = true;
            return WAIT_OBJECT_0;
        }

        // Poll, since waitpid has no timeout of its own. Ten milliseconds is
        // fine here: the callers that pass a timeout are waiting on a tool
        // that takes far longer than that.
        DWORD dwWaited = 0;
        for ( ;; )
        {
            int nStatus = 0;
            const pid_t r = waitpid( pid, &nStatus, WNOHANG );
            if ( r < 0 )
                return WAIT_FAILED;
            if ( r > 0 )
            {
                nExitCode = WIFEXITED( nStatus ) ? WEXITSTATUS( nStatus ) : -1;
                bReaped = true;
                return WAIT_OBJECT_0;
            }
            if ( dwWaited >= dwMillis )
                return WAIT_TIMEOUT;
            usleep( 10000 );
            dwWaited += 10;
        }
    }
};

// Splits a Windows command line the way CommandLineToArgv does: quotes group,
// a doubled quote inside a quoted run is a literal one.
void SplitCommandLine( const char *pszCommandLine, std::vector<std::string> *pArgs )
{
    if ( pszCommandLine == 0 )
        return;

    std::string szCurrent;
    bool bInQuotes = false;
    bool bHave = false;

    for ( const char *p = pszCommandLine; *p != 0; ++p )
    {
        if ( *p == '"' )
        {
            if ( bInQuotes && p[1] == '"' )
            {
                szCurrent.push_back( '"' );
                ++p;
            }
            else
            {
                bInQuotes = !bInQuotes;
            }
            bHave = true;
            continue;
        }
        if ( !bInQuotes && ( *p == ' ' || *p == '\t' ) )
        {
            if ( bHave )
            {
                pArgs->push_back( szCurrent );
                szCurrent.clear();
                bHave = false;
            }
            continue;
        }
        szCurrent.push_back( *p );
        bHave = true;
    }
    if ( bHave )
        pArgs->push_back( szCurrent );
}

}   // anonymous namespace

extern "C" {

BOOL CreateProcessA( const char *pszApplicationName, char *pszCommandLine,
                     void *, void *, BOOL, DWORD, void *,
                     const char *pszCurrentDirectory,
                     STARTUPINFOA *, PROCESS_INFORMATION *pProcessInformation )
{
    if ( pProcessInformation == 0 )
        return FALSE;

    std::vector<std::string> args;
    SplitCommandLine( pszCommandLine, &args );

    // Windows takes the executable from the application name when it is given,
    // and otherwise from the first token of the command line.
    std::string szExe;
    if ( pszApplicationName != 0 && pszApplicationName[0] != 0 )
    {
        szExe = pszApplicationName;
        // argv[0] is the program name; the command line's first token is it too
        if ( args.empty() )
            args.push_back( szExe );
    }
    else
    {
        if ( args.empty() )
            return FALSE;
        szExe = args[0];
    }

    std::vector<char *> argv;
    argv.reserve( args.size() + 1 );
    for ( size_t i = 0; i < args.size(); ++i )
        argv.push_back( const_cast<char *>( args[i].c_str() ) );
    argv.push_back( 0 );

    const pid_t pid = fork();
    if ( pid < 0 )
        return FALSE;

    if ( pid == 0 )
    {
        if ( pszCurrentDirectory != 0 && pszCurrentDirectory[0] != 0 )
        {
            if ( chdir( pszCurrentDirectory ) != 0 )
                _exit( 127 );
        }
        execv( szExe.c_str(), &argv[0] );
        _exit( 127 );                       // exec failed; the parent sees it
    }

    SProcessHandle *pHandle = new SProcessHandle( pid );
    memset( pProcessInformation, 0, sizeof( *pProcessInformation ) );
    pProcessInformation->hProcess = reinterpret_cast<HANDLE>( pHandle );
    pProcessInformation->hThread = 0;       // no separate thread handle here
    pProcessInformation->dwProcessId = (DWORD)pid;
    return TRUE;
}

BOOL GetExitCodeProcess( HANDLE hProcess, DWORD *pdwExitCode )
{
    SProcessHandle *p =
        dynamic_cast<SProcessHandle *>( NBk1Win32::FromHandle( hProcess ) );
    if ( p == 0 || pdwExitCode == 0 )
        return FALSE;

    if ( !p->bReaped )
    {
        int nStatus = 0;
        const pid_t r = waitpid( p->pid, &nStatus, WNOHANG );
        if ( r > 0 )
        {
            p->nExitCode = WIFEXITED( nStatus ) ? WEXITSTATUS( nStatus ) : -1;
            p->bReaped = true;
        }
    }
    *pdwExitCode = p->bReaped ? (DWORD)p->nExitCode : (DWORD)STILL_ACTIVE;
    return TRUE;
}

BOOL TerminateProcess( HANDLE hProcess, UINT )
{
    SProcessHandle *p =
        dynamic_cast<SProcessHandle *>( NBk1Win32::FromHandle( hProcess ) );
    if ( p == 0 )
        return FALSE;
    return ( kill( p->pid, SIGKILL ) == 0 ) ? TRUE : FALSE;
}

DWORD GetCurrentProcessId( void )
{
    return (DWORD)getpid();
}

}   // extern "C"
