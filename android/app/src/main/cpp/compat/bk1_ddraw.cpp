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

// The surface the probe creates so that it has something to ask for the later
// interface versions. It is never drawn to -- the rendering is Direct3D's --
// so it carries a description and nothing else.
struct SDirectDrawSurface : public IDirectDrawSurface4
{
    LONG          nRefCount;
    DDSURFACEDESC desc;

    SDirectDrawSurface() : nRefCount( 1 ) { memset( &desc, 0, sizeof( desc ) ); }

    HRESULT STDCALL QueryInterface( REFIID riid, void **ppvObject ) override
    {
        if ( ppvObject == 0 )
            return E_INVALIDARG;
        // Every version this thing was ever asked to become is a version whose
        // capabilities are present here, so each one is granted -- which is
        // what tells the probe it is looking at DirectX 6 or better.
        if ( memcmp( &riid, &IID_IUnknown, sizeof( IID ) ) == 0 ||
             memcmp( &riid, &IID_IDirectDrawSurface3, sizeof( IID ) ) == 0 ||
             memcmp( &riid, &IID_IDirectDrawSurface4, sizeof( IID ) ) == 0 ||
             memcmp( &riid, &IID_IDirectDrawSurface7, sizeof( IID ) ) == 0 )
        {
            *ppvObject = this;
            ++nRefCount;
            return S_OK;
        }
        *ppvObject = 0;
        return E_NOINTERFACE;
    }
    ULONG STDCALL AddRef() override { return (ULONG)++nRefCount; }
    ULONG STDCALL Release() override
    {
        const LONG n = --nRefCount;
        if ( n <= 0 )
            delete this;
        return (ULONG)n;
    }

    HRESULT STDCALL GetSurfaceDesc( DDSURFACEDESC *pDesc ) override
    {
        if ( pDesc == 0 )
            return E_INVALIDARG;
        *pDesc = desc;
        return S_OK;
    }
};

struct SDirectDraw : public IDirectDraw7
{
    LONG nRefCount;

    SDirectDraw() : nRefCount( 1 ) {}

    HRESULT STDCALL QueryInterface( REFIID riid, void **ppvObject ) override
    {
        if ( ppvObject == 0 )
            return E_INVALIDARG;
        // As above: the probe reads a granted interface as a DirectX version,
        // and there is no version of this API whose absence would mean
        // anything here.
        if ( memcmp( &riid, &IID_IUnknown, sizeof( IID ) ) == 0 ||
             memcmp( &riid, &IID_IDirectDraw2, sizeof( IID ) ) == 0 ||
             memcmp( &riid, &IID_IDirectDraw4, sizeof( IID ) ) == 0 ||
             memcmp( &riid, &IID_IDirectDraw7, sizeof( IID ) ) == 0 )
        {
            *ppvObject = this;
            ++nRefCount;
            return S_OK;
        }
        *ppvObject = 0;
        return E_NOINTERFACE;
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

    // The probe needs a surface in hand before it can ask for the later
    // surface versions; refusing here would stop it early and have it report a
    // DirectX older than anything this port provides.
    HRESULT STDCALL CreateSurface( DDSURFACEDESC *pDesc,
                                   IDirectDrawSurface **ppSurface,
                                   IUnknown * ) override
    {
        if ( ppSurface == 0 )
            return E_INVALIDARG;
        SDirectDrawSurface *pNew = new SDirectDrawSurface();
        if ( pDesc != 0 )
            pNew->desc = *pDesc;
        *ppSurface = pNew;
        return S_OK;
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
