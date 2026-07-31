#pragma once
// DirectMusic, which GFX/VideoCheck.cpp probes for while checking what the
// machine has installed. Nothing is played through it -- the port's audio is
// Oboe's -- so the interface exists to be asked for and refused, which is what
// the probe already handles for a machine without it.
#include "bk1_win32_types.h"
#include "bk1_com_stream.h"

struct IDirectMusic : public IUnknown
{
    virtual HRESULT STDCALL EnumPort( DWORD nIndex, void *pPortCaps ) = 0;
};

typedef struct IDirectMusic *LPDIRECTMUSIC;

// The class and interface identifiers the probe passes to CoCreateInstance.
// Their values are never interpreted, only compared.
static const CLSID CLSID_DirectMusic =
    { 0x636b9f10, 0x0c7d, 0x11d1, { 0x95, 0xb2, 0x00, 0x20, 0xaf, 0xdc, 0x74, 0x21 } };
static const IID IID_IDirectMusic =
    { 0x6536115a, 0x7b2d, 0x11d2, { 0xba, 0x18, 0x00, 0x00, 0xf8, 0x75, 0xac, 0x12 } };

#define CLSCTX_INPROC_SERVER 0x1
#define CLSCTX_INPROC_HANDLER 0x2
#define CLSCTX_LOCAL_SERVER  0x4
#define CLSCTX_ALL           ( CLSCTX_INPROC_SERVER | CLSCTX_INPROC_HANDLER | CLSCTX_LOCAL_SERVER )

#ifdef __cplusplus
extern "C" {
#endif

// The apartment calls, which have nothing to initialise here.
HRESULT CoInitialize( void *pReserved );
HRESULT CoInitializeEx( void *pReserved, DWORD dwCoInit );
void    CoUninitialize( void );
// There is no class factory to ask, so this refuses, and the probe reads that
// as the component not being installed.
HRESULT CoCreateInstance( const CLSID &clsid, IUnknown *pUnkOuter, DWORD dwClsContext,
                          const IID &iid, void **ppv );

#ifdef __cplusplus
}
#endif
