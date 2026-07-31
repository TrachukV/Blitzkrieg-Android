#pragma once
// IUnknown and IStream. StreamIO/StreamAdaptor.h implements the whole IStream
// vtable to hand the engine's own streams to COM consumers, so the signatures
// here have to match Windows exactly or the overrides will not bind.
#include "bk1_win32_types.h"
#include "bk1_win32_fileio.h"    // ULARGE_INTEGER
#include "bk1_win32_platform.h"  // LARGE_INTEGER

#ifndef STDCALL
#define STDCALL
#endif

typedef WORD     OLECHAR;
typedef OLECHAR *LPOLESTR;

typedef struct tagSTATSTG {
    LPOLESTR       pwcsName;
    DWORD          type;
    ULARGE_INTEGER cbSize;
    FILETIME       mtime;
    FILETIME       ctime;
    FILETIME       atime;
    DWORD          grfMode;
    DWORD          grfLocksSupported;
    CLSID          clsid;
    DWORD          grfStateBits;
    DWORD          reserved;
} STATSTG;

#define STGTY_STORAGE   1
#define STGTY_STREAM    2
#define STGTY_LOCKBYTES 3
#define STGTY_PROPERTY  4

struct IUnknown
{
    virtual HRESULT STDCALL QueryInterface( REFIID iid, void **ppvObject ) = 0;
    virtual ULONG STDCALL AddRef() = 0;
    virtual ULONG STDCALL Release() = 0;
};

struct ISequentialStream : public IUnknown
{
    virtual HRESULT STDCALL Read( void *pv, ULONG cb, ULONG *pcbRead ) = 0;
    virtual HRESULT STDCALL Write( void const *pv, ULONG cb, ULONG *pcbWritten ) = 0;
};

struct IStream : public ISequentialStream
{
    virtual HRESULT STDCALL Seek( LARGE_INTEGER dlibMove, DWORD dwOrigin,
                                  ULARGE_INTEGER *plibNewPosition ) = 0;
    virtual HRESULT STDCALL SetSize( ULARGE_INTEGER libNewSize ) = 0;
    virtual HRESULT STDCALL CopyTo( IStream *pDst, ULARGE_INTEGER cb,
                                    ULARGE_INTEGER *pcbRead,
                                    ULARGE_INTEGER *pcbWritten ) = 0;
    virtual HRESULT STDCALL Commit( DWORD grfCommitFlags ) = 0;
    virtual HRESULT STDCALL Revert() = 0;
    virtual HRESULT STDCALL LockRegion( ULARGE_INTEGER libOffset, ULARGE_INTEGER cb,
                                        DWORD dwLockType ) = 0;
    virtual HRESULT STDCALL UnlockRegion( ULARGE_INTEGER libOffset, ULARGE_INTEGER cb,
                                          DWORD dwLockType ) = 0;
    virtual HRESULT STDCALL Stat( STATSTG *pStats, DWORD grfStatFlag ) = 0;
    virtual HRESULT STDCALL Clone( IStream **ppstm ) = 0;
};
