#pragma once
// The DirectDraw surface descriptions, which the engine uses as a plain data
// layout: it fills them in to describe an image buffer and hands them to the
// texture compressor. No DirectDraw interface is involved.
#include "bk1_win32_types.h"

#ifndef MAKEFOURCC
#define MAKEFOURCC( ch0, ch1, ch2, ch3 )                                \
    ( (DWORD)(BYTE)( ch0 ) | ( (DWORD)(BYTE)( ch1 ) << 8 ) |            \
      ( (DWORD)(BYTE)( ch2 ) << 16 ) | ( (DWORD)(BYTE)( ch3 ) << 24 ) )
#endif

// --- DDSURFACEDESC::dwFlags ---
#define DDSD_CAPS               0x00000001
#define DDSD_HEIGHT             0x00000002
#define DDSD_WIDTH              0x00000004
#define DDSD_PITCH              0x00000008
#define DDSD_BACKBUFFERCOUNT    0x00000020
#define DDSD_ZBUFFERBITDEPTH    0x00000040
#define DDSD_ALPHABITDEPTH      0x00000080
#define DDSD_LPSURFACE          0x00000800
#define DDSD_PIXELFORMAT        0x00001000
#define DDSD_MIPMAPCOUNT        0x00020000
#define DDSD_LINEARSIZE         0x00080000
#define DDSD_DEPTH              0x00800000

// --- DDPIXELFORMAT::dwFlags ---
#define DDPF_ALPHAPIXELS        0x00000001
#define DDPF_ALPHA              0x00000002
#define DDPF_FOURCC             0x00000004
#define DDPF_RGB                0x00000040
#define DDPF_LUMINANCE          0x00020000

// --- DDSCAPS::dwCaps ---
#define DDSCAPS_COMPLEX         0x00000008
#define DDSCAPS_TEXTURE         0x00001000
#define DDSCAPS_MIPMAP          0x00400000
#define DDSCAPS_PRIMARYSURFACE  0x00000200
#define DDSCAPS_VIDEOMEMORY     0x00004000
#define DDSCAPS_NONLOCALVIDMEM  0x00008000

// --- DDSCAPS2::dwCaps2 ---
#define DDSCAPS2_CUBEMAP            0x00000200
#define DDSCAPS2_CUBEMAP_POSITIVEX  0x00000400
#define DDSCAPS2_CUBEMAP_NEGATIVEX  0x00000800
#define DDSCAPS2_CUBEMAP_POSITIVEY  0x00001000
#define DDSCAPS2_CUBEMAP_NEGATIVEY  0x00002000
#define DDSCAPS2_CUBEMAP_POSITIVEZ  0x00004000
#define DDSCAPS2_CUBEMAP_NEGATIVEZ  0x00008000
#define DDSCAPS2_VOLUME             0x00200000

typedef struct _DDCOLORKEY {
    DWORD dwColorSpaceLowValue;
    DWORD dwColorSpaceHighValue;
} DDCOLORKEY;

typedef struct _DDSCAPS {
    DWORD dwCaps;
} DDSCAPS;

typedef struct _DDSCAPS2 {
    DWORD dwCaps;
    DWORD dwCaps2;
    DWORD dwCaps3;
    DWORD dwCaps4;
} DDSCAPS2;

typedef struct _DDPIXELFORMAT {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwFourCC;
    union {
        DWORD dwRGBBitCount;
        DWORD dwAlphaBitDepth;
        DWORD dwLuminanceBitCount;
    };
    union {
        DWORD dwRBitMask;
        DWORD dwLuminanceBitMask;
    };
    union {
        DWORD dwGBitMask;
    };
    union {
        DWORD dwBBitMask;
    };
    union {
        DWORD dwRGBAlphaBitMask;
        DWORD dwLuminanceAlphaBitMask;
    };
} DDPIXELFORMAT;

typedef struct _DDSURFACEDESC {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwHeight;
    DWORD dwWidth;
    union {
        LONG  lPitch;
        DWORD dwLinearSize;
    };
    DWORD dwBackBufferCount;
    union {
        DWORD dwMipMapCount;
        DWORD dwZBufferBitDepth;
        DWORD dwRefreshRate;
    };
    DWORD         dwAlphaBitDepth;
    DWORD         dwReserved;
    LPVOID        lpSurface;
    DDCOLORKEY    ddckCKDestOverlay;
    DDCOLORKEY    ddckCKDestBlt;
    DDCOLORKEY    ddckCKSrcOverlay;
    DDCOLORKEY    ddckCKSrcBlt;
    DDPIXELFORMAT ddpfPixelFormat;
    DDSCAPS       ddsCaps;
} DDSURFACEDESC, *LPDDSURFACEDESC;

typedef struct _DDSURFACEDESC2 {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwHeight;
    DWORD dwWidth;
    union {
        LONG  lPitch;
        DWORD dwLinearSize;
    };
    union {
        DWORD dwBackBufferCount;
        DWORD dwDepth;
    };
    union {
        DWORD dwMipMapCount;
        DWORD dwRefreshRate;
        DWORD dwSrcVBHandle;
    };
    DWORD         dwAlphaBitDepth;
    DWORD         dwReserved;
    LPVOID        lpSurface;
    DDCOLORKEY    ddckCKDestOverlay;
    DDCOLORKEY    ddckCKDestBlt;
    DDCOLORKEY    ddckCKSrcOverlay;
    DDCOLORKEY    ddckCKSrcBlt;
    DDPIXELFORMAT ddpfPixelFormat;
    DDSCAPS2      ddsCaps;
    DWORD         dwTextureStage;
} DDSURFACEDESC2, *LPDDSURFACEDESC2;

// The interface VideoCheck asks the machine for while probing. It only checks
// that creating one succeeds and then sets a cooperative level, so the shape
// is all that is needed.
#define DDSCL_NORMAL        0x00000008L
#define DDSCL_FULLSCREEN    0x00000001L
#define DDSCL_EXCLUSIVE     0x00000010L

#include "bk1_com_stream.h"

struct IDirectDrawSurface;

// VideoCheck creates one of these while probing the machine, asks how much
// video memory is free, and lets it go. Nothing is drawn through it -- the
// rendering is Direct3D's -- so these are the five entry points it touches.
struct IDirectDraw : public IUnknown
{
    virtual HRESULT STDCALL SetCooperativeLevel( HWND hWnd, DWORD dwFlags ) = 0;
    virtual HRESULT STDCALL CreateSurface( DDSURFACEDESC *pDesc,
                                           IDirectDrawSurface **ppSurface,
                                           IUnknown *pUnkOuter ) = 0;
    virtual HRESULT STDCALL GetAvailableVidMem( DDSCAPS2 *pCaps, DWORD *pdwTotal,
                                                DWORD *pdwFree ) = 0;
};

struct IDirectDrawSurface : public IUnknown
{
    virtual HRESULT STDCALL GetSurfaceDesc( DDSURFACEDESC *pDesc ) = 0;
};

// The surface interface versions the probe asks a surface to become. Their
// values are compared, never interpreted.
static const IID IID_IDirectDrawSurface3 =
    { 0xda044e00, 0x69b2, 0x11d0, { 0xa1, 0xd5, 0x00, 0xaa, 0x00, 0xb8, 0xdf, 0xbb } };
static const IID IID_IDirectDrawSurface4 =
    { 0x0b2b8630, 0xad35, 0x11d0, { 0x8e, 0xa6, 0x00, 0x60, 0x97, 0x97, 0xea, 0x5b } };
static const IID IID_IDirectDrawSurface7 =
    { 0x06675a80, 0x3b9b, 0x11d2, { 0xb9, 0x2f, 0x00, 0x60, 0x97, 0x97, 0xea, 0x5b } };

typedef struct IDirectDraw *LPDIRECTDRAW;
typedef struct IDirectDrawSurface *LPDIRECTDRAWSURFACE;

#ifdef __cplusplus
extern "C" {
#endif

HRESULT DirectDrawCreate( GUID *pGuid, IDirectDraw **ppDD, IUnknown *pUnkOuter );
HRESULT DirectDrawCreateEx( GUID *pGuid, void **ppDD, const GUID &iid,
                            IUnknown *pUnkOuter );

#ifdef __cplusplus
}
#endif
