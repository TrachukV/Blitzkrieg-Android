// The DirectDraw object VideoCheck probes with.
//
// The port draws through Direct3D's replacement, not through this; the engine
// creates one only to ask the machine how much video memory it has. That
// question has no answer on Android -- the GPU shares the system's memory and
// the amount an application may use is not fixed -- so it reports what the
// device actually has to work with, which is what the caller does arithmetic
// against.
#include "ddraw.h"

#include <string.h>
#include <unistd.h>

namespace {

struct SDirectDraw : public IDirectDraw
{
    LONG nRefCount;

    SDirectDraw() : nRefCount( 1 ) {}

    HRESULT STDCALL QueryInterface( REFIID, void **ppvObject ) override
    {
        if ( ppvObject == 0 )
            return E_INVALIDARG;
        *ppvObject = this;
        ++nRefCount;
        return S_OK;
    }
    ULONG STDCALL AddRef() override { return (ULONG)++nRefCount; }
    ULONG STDCALL Release() override
    {
        const LONG n = --nRefCount;
        if ( n <= 0 )
            delete this;
        return (ULONG)n;
    }

    HRESULT STDCALL SetCooperativeLevel( HWND, DWORD ) override { return S_OK; }

    // The engine only ever asks for a primary surface here and does not draw
    // to it, so there is nothing to hand back.
    HRESULT STDCALL CreateSurface( DDSURFACEDESC *, IDirectDrawSurface **ppSurface,
                                   IUnknown * ) override
    {
        if ( ppSurface != 0 )
            *ppSurface = 0;
        return E_FAIL;
    }

    HRESULT STDCALL GetAvailableVidMem( DDSCAPS2 *, DWORD *pdwTotal,
                                        DWORD *pdwFree ) override
    {
        // Physical memory is the honest bound on Android: the GPU shares it,
        // and the engine uses this figure to decide texture budgets.
        const long nPages = sysconf( _SC_PHYS_PAGES );
        const long nPageSize = sysconf( _SC_PAGESIZE );
        unsigned long long nBytes = ( nPages > 0 && nPageSize > 0 )
                                        ? (unsigned long long)nPages * nPageSize
                                        : 0;
        // Reported in the range a DWORD can hold, since that is what the
        // caller reads it into.
        if ( nBytes > 0xFFFFFFFFull )
            nBytes = 0xFFFFFFFFull;
        if ( pdwTotal != 0 )
            *pdwTotal = (DWORD)nBytes;
        if ( pdwFree != 0 )
            *pdwFree = (DWORD)( nBytes / 2 );
        return S_OK;
    }
};

}   // anonymous namespace

extern "C" {

HRESULT DirectDrawCreate( GUID *, IDirectDraw **ppDD, IUnknown * )
{
    if ( ppDD == 0 )
        return E_INVALIDARG;
    *ppDD = new SDirectDraw();
    return S_OK;
}

HRESULT DirectDrawCreateEx( GUID *, void **ppDD, const GUID &, IUnknown * )
{
    if ( ppDD == 0 )
        return E_INVALIDARG;
    *ppDD = new SDirectDraw();
    return S_OK;
}

}   // extern "C"
