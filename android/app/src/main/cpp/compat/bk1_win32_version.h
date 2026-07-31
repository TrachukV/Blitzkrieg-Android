#pragma once
// version.dll's file-version reader.
//
// The engine asks a file what version it is, and compares the answer: in
// multiplayer, so that two machines running different builds do not try to
// play together, and at startup, to put a version in the log and on screen.
//
// On Windows the answer comes from a resource compiled into the .exe. There is
// no such resource here -- an Android package carries its version in its
// manifest -- so the port answers with its own version, the one in
// app/build.gradle, and answers it for any file it is asked about. That is the
// truthful answer to the question the engine is really asking, which is "what
// build is this", and it keeps the multiplayer comparison meaningful: two
// devices running the same package agree, two running different ones do not.
#include "bk1_win32_types.h"

// The port's version, as the four numbers Windows would have carried. Change
// these together with versionName in app/build.gradle.
#define BK1_VERSION_MAJOR   0
#define BK1_VERSION_MINOR   1
#define BK1_VERSION_BUILD   0
#define BK1_VERSION_REVISION 0

// VS_FIXEDFILEINFO::dwFileFlags and friends. The engine reads the version
// numbers and nothing else, but the structure has to be the shape it expects.
#define VS_FF_DEBUG         0x00000001
#define VS_FF_PRERELEASE    0x00000002
#define VS_FFI_SIGNATURE    0xFEEF04BD
#define VS_FFI_STRUCVERSION 0x00010000
#define VOS_NT_WINDOWS32    0x00040004
#define VFT_APP             0x00000001

typedef struct tagVS_FIXEDFILEINFO {
    DWORD dwSignature;
    DWORD dwStrucVersion;
    DWORD dwFileVersionMS;
    DWORD dwFileVersionLS;
    DWORD dwProductVersionMS;
    DWORD dwProductVersionLS;
    DWORD dwFileFlagsMask;
    DWORD dwFileFlags;
    DWORD dwFileOS;
    DWORD dwFileType;
    DWORD dwFileSubtype;
    DWORD dwFileDateMS;
    DWORD dwFileDateLS;
} VS_FIXEDFILEINFO;

#ifdef __cplusplus
extern "C" {
#endif

DWORD GetFileVersionInfoSizeA( const char *pszFileName, DWORD *pdwHandle );
BOOL  GetFileVersionInfoA( const char *pszFileName, DWORD dwHandle, DWORD dwLen,
                           void *pData );
BOOL  VerQueryValueA( const void *pBlock, const char *pszSubBlock,
                      void **ppBuffer, UINT *puLen );

#ifdef __cplusplus
}
#endif

#ifndef GetFileVersionInfoSize
#define GetFileVersionInfoSize GetFileVersionInfoSizeA
#endif
#ifndef GetFileVersionInfo
#define GetFileVersionInfo GetFileVersionInfoA
#endif
#ifndef VerQueryValue
#define VerQueryValue VerQueryValueA
#endif
