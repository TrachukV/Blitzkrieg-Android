#pragma once
// Stands in for the Windows sockets header.
//
// WinSock was modelled on BSD sockets, so nearly everything the engine calls
// already exists on Android under the same name and signature. What differs is
// the handful of places Windows went its own way: a socket is a HANDLE-like
// SOCKET rather than a file descriptor, errors come from WSAGetLastError
// rather than errno, closing is closesocket, non-blocking mode is set with
// ioctlsocket, and the library needs starting and stopping.
#include "bk1_win32_types.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

// A socket is a file descriptor here, so the sentinel is -1 rather than the
// unsigned ~0 Windows uses. Code that compares against INVALID_SOCKET keeps
// working; code that assumes the Windows value would not, and none does.
typedef int SOCKET;
#define INVALID_SOCKET  ( (SOCKET)-1 )
#define SOCKET_ERROR    ( -1 )

typedef struct sockaddr     SOCKADDR;
typedef struct sockaddr    *PSOCKADDR, *LPSOCKADDR;
typedef struct sockaddr_in  SOCKADDR_IN;
typedef struct sockaddr_in *PSOCKADDR_IN, *LPSOCKADDR_IN;
typedef struct hostent      HOSTENT;
typedef struct hostent     *PHOSTENT, *LPHOSTENT;
typedef struct servent      SERVENT;
typedef struct in_addr      IN_ADDR;
typedef struct timeval      TIMEVAL;
typedef fd_set              FD_SET_TYPE;

// The WinSock error names, mapped onto the errno values that carry the same
// meaning, so that comparisons against them behave.
#define WSAEWOULDBLOCK   EWOULDBLOCK
#define WSAEINPROGRESS   EINPROGRESS
#define WSAEALREADY      EALREADY
#define WSAENOTSOCK      ENOTSOCK
#define WSAEADDRINUSE    EADDRINUSE
#define WSAEADDRNOTAVAIL EADDRNOTAVAIL
#define WSAENETDOWN      ENETDOWN
#define WSAENETUNREACH   ENETUNREACH
#define WSAECONNABORTED  ECONNABORTED
#define WSAECONNRESET    ECONNRESET
#define WSAENOBUFS       ENOBUFS
#define WSAEISCONN       EISCONN
#define WSAENOTCONN      ENOTCONN
#define WSAETIMEDOUT     ETIMEDOUT
#define WSAECONNREFUSED  ECONNREFUSED
#define WSAEHOSTUNREACH  EHOSTUNREACH
#define WSAEMSGSIZE      EMSGSIZE

typedef struct WSAData {
    WORD           wVersion;
    WORD           wHighVersion;
    char           szDescription[257];
    char           szSystemStatus[129];
    unsigned short iMaxSockets;
    unsigned short iMaxUdpDg;
    char          *lpVendorInfo;
} WSADATA, *LPWSADATA;

#define MAKEWORD_WSA( low, high ) ( (WORD)( ( (BYTE)(low) ) | ( (WORD)( (BYTE)(high) ) << 8 ) ) )

#ifdef __cplusplus

// The library needs no starting here; the call is answered so that startup
// code which checks its result proceeds.
inline int WSAStartup( WORD wVersionRequested, LPWSADATA pData )
{
    if ( pData != 0 )
    {
        __builtin_memset( pData, 0, sizeof( *pData ) );
        pData->wVersion = wVersionRequested;
        pData->wHighVersion = wVersionRequested;
        pData->iMaxSockets = 0;
        pData->iMaxUdpDg = 0;
    }
    return 0;
}

inline int WSACleanup() { return 0; }

inline int WSAGetLastError() { return errno; }
inline void WSASetLastError( int nError ) { errno = nError; }

inline int closesocket( SOCKET s ) { return close( s ); }

// Windows' ioctlsocket only ever gets FIONBIO from this engine, which is
// O_NONBLOCK here.
#ifndef FIONBIO
#define FIONBIO 0x5421
#endif

// WinSock passes the address length as an int, POSIX as a socklen_t, and on
// this platform those are different types. These overloads take the engine's
// spelling and forward; they do not hide the POSIX ones, which take the other
// type.
inline int recvfrom( SOCKET s, void *pBuffer, size_t nLength, int nFlags,
                     struct sockaddr *pFrom, int *pnFromLen )
{
    socklen_t nLen = ( pnFromLen != 0 ) ? (socklen_t)*pnFromLen : 0;
    const int nResult = (int)::recvfrom( s, pBuffer, nLength, nFlags, pFrom,
                                         ( pnFromLen != 0 ) ? &nLen : 0 );
    if ( pnFromLen != 0 )
        *pnFromLen = (int)nLen;
    return nResult;
}

inline int getsockname( SOCKET s, struct sockaddr *pName, int *pnNameLen )
{
    socklen_t nLen = ( pnNameLen != 0 ) ? (socklen_t)*pnNameLen : 0;
    const int nResult = ::getsockname( s, pName, ( pnNameLen != 0 ) ? &nLen : 0 );
    if ( pnNameLen != 0 )
        *pnNameLen = (int)nLen;
    return nResult;
}

inline int getpeername( SOCKET s, struct sockaddr *pName, int *pnNameLen )
{
    socklen_t nLen = ( pnNameLen != 0 ) ? (socklen_t)*pnNameLen : 0;
    const int nResult = ::getpeername( s, pName, ( pnNameLen != 0 ) ? &nLen : 0 );
    if ( pnNameLen != 0 )
        *pnNameLen = (int)nLen;
    return nResult;
}

inline SOCKET accept( SOCKET s, struct sockaddr *pAddr, int *pnAddrLen )
{
    socklen_t nLen = ( pnAddrLen != 0 ) ? (socklen_t)*pnAddrLen : 0;
    const SOCKET result = ::accept( s, pAddr, ( pnAddrLen != 0 ) ? &nLen : 0 );
    if ( pnAddrLen != 0 )
        *pnAddrLen = (int)nLen;
    return result;
}

inline int ioctlsocket( SOCKET s, long nCommand, unsigned long *pArgument )
{
    if ( nCommand != FIONBIO || pArgument == 0 )
        return SOCKET_ERROR;

    const int nFlags = fcntl( s, F_GETFL, 0 );
    if ( nFlags < 0 )
        return SOCKET_ERROR;
    const int nNew = ( *pArgument != 0 ) ? ( nFlags | O_NONBLOCK )
                                         : ( nFlags & ~O_NONBLOCK );
    return ( fcntl( s, F_SETFL, nNew ) < 0 ) ? SOCKET_ERROR : 0;
}

// DWORD is not unsigned long on this platform, and the engine passes both.
inline int ioctlsocket( SOCKET s, long nCommand, unsigned int *pArgument )
{
    unsigned long nValue = ( pArgument != 0 ) ? *pArgument : 0;
    const int nResult = ioctlsocket( s, nCommand, ( pArgument != 0 ) ? &nValue : 0 );
    if ( pArgument != 0 )
        *pArgument = (unsigned int)nValue;
    return nResult;
}

#endif   // __cplusplus
