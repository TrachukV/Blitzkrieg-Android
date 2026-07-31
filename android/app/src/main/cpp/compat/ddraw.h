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
