#pragma once
// The Direct3D 8 types, enumerations and constants the engine names.
//
// Blitzkrieg uses Direct3D 8 as a fixed-function 2D blitter: there are no
// programmable shaders anywhere in the tree, terrain vertices are emitted
// already in screen space as D3DFVF_XYZRHW, and the ground is drawn with depth
// test and write off. So the replacement under these declarations is a sprite
// and quad batcher on OpenGL ES, not a ported 3D renderer.
//
// The numeric values follow the published API, because some of them leave the
// process: the DXT formats are the FourCC codes stored inside .dds files, and
// the engine's own data refers to formats by number.
#include "bk1_win32_types.h"

// The compressed formats below are FourCC codes, and this header is reached
// without <ddraw.h> in the graphics paths.
#ifndef MAKEFOURCC
#define MAKEFOURCC( ch0, ch1, ch2, ch3 )                                \
    ( (DWORD)(BYTE)( ch0 ) | ( (DWORD)(BYTE)( ch1 ) << 8 ) |            \
      ( (DWORD)(BYTE)( ch2 ) << 16 ) | ( (DWORD)(BYTE)( ch3 ) << 24 ) )
#endif

#define D3D_SDK_VERSION 220

// ---------------------------------------------------------------------------
// Pixel and index formats
// ---------------------------------------------------------------------------
typedef enum _D3DFORMAT {
    D3DFMT_UNKNOWN      = 0,

    D3DFMT_R8G8B8       = 20,
    D3DFMT_A8R8G8B8     = 21,
    D3DFMT_X8R8G8B8     = 22,
    D3DFMT_R5G6B5       = 23,
    D3DFMT_X1R5G5B5     = 24,
    D3DFMT_A1R5G5B5     = 25,
    D3DFMT_A4R4G4B4     = 26,
    D3DFMT_R3G3B2       = 27,
    D3DFMT_A8           = 28,
    D3DFMT_X4R4G4B4     = 30,

    D3DFMT_A8P8         = 40,
    D3DFMT_P8           = 41,
    D3DFMT_L8           = 50,
    D3DFMT_A8L8         = 51,

    D3DFMT_V8U8         = 60,
    D3DFMT_L6V5U5       = 61,
    D3DFMT_X8L8V8U8     = 62,

    // the compressed formats, whose values are the FourCC codes a .dds carries
    D3DFMT_DXT1         = MAKEFOURCC( 'D', 'X', 'T', '1' ),
    D3DFMT_DXT2         = MAKEFOURCC( 'D', 'X', 'T', '2' ),
    D3DFMT_DXT3         = MAKEFOURCC( 'D', 'X', 'T', '3' ),
    D3DFMT_DXT4         = MAKEFOURCC( 'D', 'X', 'T', '4' ),
    D3DFMT_DXT5         = MAKEFOURCC( 'D', 'X', 'T', '5' ),

    D3DFMT_D16_LOCKABLE = 70,
    D3DFMT_D32          = 71,
    D3DFMT_D15S1        = 73,
    D3DFMT_D24S8        = 75,
    D3DFMT_D16          = 80,
    D3DFMT_D24X8        = 77,
    D3DFMT_D24X4S4      = 79,

    D3DFMT_INDEX16      = 101,
    D3DFMT_INDEX32      = 102,

    D3DFMT_FORCE_DWORD  = 0x7fffffff
} D3DFORMAT;

// ---------------------------------------------------------------------------
// Render state
// ---------------------------------------------------------------------------
typedef enum _D3DRENDERSTATETYPE {
    D3DRS_ZENABLE            = 7,
    D3DRS_FILLMODE           = 8,
    D3DRS_SHADEMODE          = 9,
    D3DRS_ZWRITEENABLE       = 14,
    D3DRS_ALPHATESTENABLE    = 15,
    D3DRS_SRCBLEND           = 19,
    D3DRS_DESTBLEND          = 20,
    D3DRS_CULLMODE           = 22,
    D3DRS_ZFUNC              = 23,
    D3DRS_ALPHAREF           = 24,
    D3DRS_ALPHAFUNC          = 25,
    D3DRS_DITHERENABLE       = 26,
    D3DRS_ALPHABLENDENABLE   = 27,
    D3DRS_FOGENABLE          = 28,
    D3DRS_SPECULARENABLE     = 29,
    D3DRS_FOGCOLOR           = 34,
    D3DRS_FOGTABLEMODE       = 35,
    D3DRS_FOGSTART           = 36,
    D3DRS_FOGEND             = 37,
    D3DRS_FOGDENSITY         = 38,
    D3DRS_STENCILENABLE      = 52,
    D3DRS_STENCILFAIL        = 53,
    D3DRS_STENCILZFAIL       = 54,
    D3DRS_STENCILPASS        = 55,
    D3DRS_STENCILFUNC        = 56,
    D3DRS_STENCILREF         = 57,
    D3DRS_STENCILMASK        = 58,
    D3DRS_STENCILWRITEMASK   = 59,
    D3DRS_TEXTUREFACTOR      = 60,
    D3DRS_LIGHTING           = 137,
    D3DRS_AMBIENT            = 139,
    D3DRS_COLORVERTEX        = 141,
    D3DRS_FORCE_DWORD        = 0x7fffffff
} D3DRENDERSTATETYPE;

// ---------------------------------------------------------------------------
// Texture stage state
// ---------------------------------------------------------------------------
typedef enum _D3DTEXTURESTAGESTATETYPE {
    D3DTSS_COLOROP               = 1,
    D3DTSS_COLORARG1             = 2,
    D3DTSS_COLORARG2             = 3,
    D3DTSS_ALPHAOP               = 4,
    D3DTSS_ALPHAARG1             = 5,
    D3DTSS_ALPHAARG2             = 6,
    D3DTSS_TEXCOORDINDEX         = 11,
    D3DTSS_ADDRESSU              = 13,
    D3DTSS_ADDRESSV              = 14,
    D3DTSS_BORDERCOLOR           = 15,
    D3DTSS_MAGFILTER             = 16,
    D3DTSS_MINFILTER             = 17,
    D3DTSS_MIPFILTER             = 18,
    D3DTSS_MIPMAPLODBIAS         = 19,
    D3DTSS_MAXMIPLEVEL           = 20,
    D3DTSS_MAXANISOTROPY         = 21,
    D3DTSS_TEXTURETRANSFORMFLAGS = 24,
    D3DTSS_FORCE_DWORD           = 0x7fffffff
} D3DTEXTURESTAGESTATETYPE;

// The texture blending operations the engine selects. Only these five appear
// in the tree, which is what makes the fixed-function stage reproducible in a
// single small shader.
typedef enum _D3DTEXTUREOP {
    D3DTOP_DISABLE     = 1,
    D3DTOP_SELECTARG1  = 2,
    D3DTOP_SELECTARG2  = 3,
    D3DTOP_MODULATE    = 4,
    D3DTOP_MODULATE2X  = 5,
    D3DTOP_MODULATE4X  = 6,
    D3DTOP_ADD         = 7,
    D3DTOP_FORCE_DWORD = 0x7fffffff
} D3DTEXTUREOP;

// The arguments those operations take.
#define D3DTA_DIFFUSE     0x00000000
#define D3DTA_CURRENT     0x00000001
#define D3DTA_TEXTURE     0x00000002
#define D3DTA_TFACTOR     0x00000003
#define D3DTA_SPECULAR    0x00000004
#define D3DTA_SELECTMASK  0x0000000f
#define D3DTA_COMPLEMENT  0x00000010
#define D3DTA_ALPHAREPLICATE 0x00000020

typedef enum _D3DTEXTUREFILTERTYPE {
    D3DTEXF_NONE        = 0,
    D3DTEXF_POINT       = 1,
    D3DTEXF_LINEAR      = 2,
    D3DTEXF_ANISOTROPIC = 3,
    D3DTEXF_FORCE_DWORD = 0x7fffffff
} D3DTEXTUREFILTERTYPE;

typedef enum _D3DTEXTUREADDRESS {
    D3DTADDRESS_WRAP        = 1,
    D3DTADDRESS_MIRROR      = 2,
    D3DTADDRESS_CLAMP       = 3,
    D3DTADDRESS_BORDER      = 4,
    D3DTADDRESS_FORCE_DWORD = 0x7fffffff
} D3DTEXTUREADDRESS;

// ---------------------------------------------------------------------------
// Blending, comparison, culling
// ---------------------------------------------------------------------------
typedef enum _D3DBLEND {
    D3DBLEND_ZERO            = 1,
    D3DBLEND_ONE             = 2,
    D3DBLEND_SRCCOLOR        = 3,
    D3DBLEND_INVSRCCOLOR     = 4,
    D3DBLEND_SRCALPHA        = 5,
    D3DBLEND_INVSRCALPHA     = 6,
    D3DBLEND_DESTALPHA       = 7,
    D3DBLEND_INVDESTALPHA    = 8,
    D3DBLEND_DESTCOLOR       = 9,
    D3DBLEND_INVDESTCOLOR    = 10,
    D3DBLEND_SRCALPHASAT     = 11,
    D3DBLEND_FORCE_DWORD     = 0x7fffffff
} D3DBLEND;

typedef enum _D3DCMPFUNC {
    D3DCMP_NEVER        = 1,
    D3DCMP_LESS         = 2,
    D3DCMP_EQUAL        = 3,
    D3DCMP_LESSEQUAL    = 4,
    D3DCMP_GREATER      = 5,
    D3DCMP_NOTEQUAL     = 6,
    D3DCMP_GREATEREQUAL = 7,
    D3DCMP_ALWAYS       = 8,
    D3DCMP_FORCE_DWORD  = 0x7fffffff
} D3DCMPFUNC;

typedef enum _D3DSTENCILOP {
    D3DSTENCILOP_KEEP        = 1,
    D3DSTENCILOP_ZERO        = 2,
    D3DSTENCILOP_REPLACE     = 3,
    D3DSTENCILOP_INCRSAT     = 4,
    D3DSTENCILOP_DECRSAT     = 5,
    D3DSTENCILOP_INVERT      = 6,
    D3DSTENCILOP_INCR        = 7,
    D3DSTENCILOP_DECR        = 8,
    D3DSTENCILOP_FORCE_DWORD = 0x7fffffff
} D3DSTENCILOP;

typedef enum _D3DCULL {
    D3DCULL_NONE        = 1,
    D3DCULL_CW          = 2,
    D3DCULL_CCW         = 3,
    D3DCULL_FORCE_DWORD = 0x7fffffff
} D3DCULL;

typedef enum _D3DFILLMODE {
    D3DFILL_POINT       = 1,
    D3DFILL_WIREFRAME   = 2,
    D3DFILL_SOLID       = 3,
    D3DFILL_FORCE_DWORD = 0x7fffffff
} D3DFILLMODE;

typedef enum _D3DZBUFFERTYPE {
    D3DZB_FALSE       = 0,
    D3DZB_TRUE        = 1,
    D3DZB_USEW        = 2,
    D3DZB_FORCE_DWORD = 0x7fffffff
} D3DZBUFFERTYPE;

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------
typedef enum _D3DPRIMITIVETYPE {
    D3DPT_POINTLIST     = 1,
    D3DPT_LINELIST      = 2,
    D3DPT_LINESTRIP     = 3,
    D3DPT_TRIANGLELIST  = 4,
    D3DPT_TRIANGLESTRIP = 5,
    D3DPT_TRIANGLEFAN   = 6,
    D3DPT_FORCE_DWORD   = 0x7fffffff
} D3DPRIMITIVETYPE;

// The flexible vertex format. XYZRHW is the one that matters here: it means
// the vertex is already in screen space, which is how the whole world is
// drawn.
#define D3DFVF_RESERVED0        0x001
#define D3DFVF_POSITION_MASK    0x00E
#define D3DFVF_XYZ              0x002
#define D3DFVF_XYZRHW           0x004
#define D3DFVF_XYZB1            0x006
#define D3DFVF_XYZB2            0x008
#define D3DFVF_XYZB3            0x00a
#define D3DFVF_XYZB4            0x00c
#define D3DFVF_XYZB5            0x00e
#define D3DFVF_NORMAL           0x010
#define D3DFVF_PSIZE            0x020
#define D3DFVF_DIFFUSE          0x040
#define D3DFVF_SPECULAR         0x080
#define D3DFVF_TEXCOUNT_MASK    0xf00
#define D3DFVF_TEXCOUNT_SHIFT   8
#define D3DFVF_TEX0             0x000
#define D3DFVF_TEX1             0x100
#define D3DFVF_TEX2             0x200
#define D3DFVF_TEX3             0x300
#define D3DFVF_TEX4             0x400
#define D3DFVF_TEX5             0x500
#define D3DFVF_TEX6             0x600
#define D3DFVF_TEX7             0x700
#define D3DFVF_TEX8             0x800

typedef enum _D3DTRANSFORMSTATETYPE {
    D3DTS_VIEW       = 2,
    D3DTS_PROJECTION = 3,
    D3DTS_TEXTURE0   = 16,
    D3DTS_TEXTURE1   = 17,
    D3DTS_TEXTURE2   = 18,
    D3DTS_TEXTURE3   = 19,
    D3DTS_TEXTURE4   = 20,
    D3DTS_TEXTURE5   = 21,
    D3DTS_TEXTURE6   = 22,
    D3DTS_TEXTURE7   = 23,
    D3DTS_FORCE_DWORD = 0x7fffffff
} D3DTRANSFORMSTATETYPE;

#define D3DTS_WORLD       256
#define D3DTS_WORLDMATRIX( index ) (D3DTRANSFORMSTATETYPE)( (index) + 256 )

// ---------------------------------------------------------------------------
// Resource creation and locking
// ---------------------------------------------------------------------------
typedef enum _D3DPOOL {
    D3DPOOL_DEFAULT     = 0,
    D3DPOOL_MANAGED     = 1,
    D3DPOOL_SYSTEMMEM   = 2,
    D3DPOOL_SCRATCH     = 3,
    D3DPOOL_FORCE_DWORD = 0x7fffffff
} D3DPOOL;

#define D3DUSAGE_RENDERTARGET   0x00000001L
#define D3DUSAGE_DEPTHSTENCIL   0x00000002L
#define D3DUSAGE_WRITEONLY      0x00000008L
#define D3DUSAGE_DYNAMIC        0x00000200L

#define D3DLOCK_READONLY            0x00000010L
#define D3DLOCK_NOSYSLOCK           0x00000800L
#define D3DLOCK_NOOVERWRITE         0x00001000L
#define D3DLOCK_DISCARD             0x00002000L
#define D3DLOCK_NO_DIRTY_UPDATE     0x00008000L

#define D3DCLEAR_TARGET     0x00000001L
#define D3DCLEAR_ZBUFFER    0x00000002L
#define D3DCLEAR_STENCIL    0x00000004L

// ---------------------------------------------------------------------------
// Structures
// ---------------------------------------------------------------------------
typedef struct _D3DMATRIX {
    union {
        struct {
            float _11, _12, _13, _14;
            float _21, _22, _23, _24;
            float _31, _32, _33, _34;
            float _41, _42, _43, _44;
        };
        float m[4][4];
    };
} D3DMATRIX;

typedef struct _D3DVIEWPORT8 {
    DWORD X;
    DWORD Y;
    DWORD Width;
    DWORD Height;
    float MinZ;
    float MaxZ;
} D3DVIEWPORT8;

typedef struct _D3DCOLORVALUE {
    float r, g, b, a;
} D3DCOLORVALUE;

typedef struct _D3DVECTOR {
    float x, y, z;
} D3DVECTOR;

typedef struct _D3DMATERIAL8 {
    D3DCOLORVALUE Diffuse;
    D3DCOLORVALUE Ambient;
    D3DCOLORVALUE Specular;
    D3DCOLORVALUE Emissive;
    float         Power;
} D3DMATERIAL8;

typedef enum _D3DLIGHTTYPE {
    D3DLIGHT_POINT       = 1,
    D3DLIGHT_SPOT        = 2,
    D3DLIGHT_DIRECTIONAL = 3,
    D3DLIGHT_FORCE_DWORD = 0x7fffffff
} D3DLIGHTTYPE;

typedef struct _D3DLIGHT8 {
    D3DLIGHTTYPE  Type;
    D3DCOLORVALUE Diffuse;
    D3DCOLORVALUE Specular;
    D3DCOLORVALUE Ambient;
    D3DVECTOR     Position;
    D3DVECTOR     Direction;
    float         Range;
    float         Falloff;
    float         Attenuation0;
    float         Attenuation1;
    float         Attenuation2;
    float         Theta;
    float         Phi;
} D3DLIGHT8;

typedef struct _D3DDISPLAYMODE {
    UINT      Width;
    UINT      Height;
    UINT      RefreshRate;
    D3DFORMAT Format;
} D3DDISPLAYMODE;

typedef enum _D3DSWAPEFFECT {
    D3DSWAPEFFECT_DISCARD     = 1,
    D3DSWAPEFFECT_FLIP        = 2,
    D3DSWAPEFFECT_COPY        = 3,
    D3DSWAPEFFECT_COPY_VSYNC  = 4,
    D3DSWAPEFFECT_FORCE_DWORD = 0x7fffffff
} D3DSWAPEFFECT;

typedef enum _D3DMULTISAMPLE_TYPE {
    D3DMULTISAMPLE_NONE         = 0,
    D3DMULTISAMPLE_2_SAMPLES    = 2,
    D3DMULTISAMPLE_FORCE_DWORD  = 0x7fffffff
} D3DMULTISAMPLE_TYPE;

typedef struct _D3DPRESENT_PARAMETERS_ {
    UINT                BackBufferWidth;
    UINT                BackBufferHeight;
    D3DFORMAT           BackBufferFormat;
    UINT                BackBufferCount;
    D3DMULTISAMPLE_TYPE MultiSampleType;
    D3DSWAPEFFECT       SwapEffect;
    HWND                hDeviceWindow;
    BOOL                Windowed;
    BOOL                EnableAutoDepthStencil;
    D3DFORMAT           AutoDepthStencilFormat;
    DWORD               Flags;
    UINT                FullScreen_RefreshRateInHz;
    UINT                FullScreen_PresentationInterval;
} D3DPRESENT_PARAMETERS;

#define D3DPRESENT_INTERVAL_DEFAULT 0x00000000L
#define D3DPRESENT_INTERVAL_ONE     0x00000001L
#define D3DPRESENT_INTERVAL_IMMEDIATE 0x80000000L

typedef struct _D3DGAMMARAMP {
    WORD red[256];
    WORD green[256];
    WORD blue[256];
} D3DGAMMARAMP;

typedef struct _D3DSURFACE_DESC {
    D3DFORMAT           Format;
    DWORD               Type;
    DWORD               Usage;
    D3DPOOL             Pool;
    UINT                Size;
    D3DMULTISAMPLE_TYPE MultiSampleType;
    UINT                Width;
    UINT                Height;
} D3DSURFACE_DESC;

typedef struct _D3DLOCKED_RECT {
    INT   Pitch;
    void *pBits;
} D3DLOCKED_RECT;

typedef struct _D3DADAPTER_IDENTIFIER8 {
    char  Driver[512];
    char  Description[512];
    DWORD DriverVersionLowPart;
    DWORD DriverVersionHighPart;
    DWORD VendorId;
    DWORD DeviceId;
    DWORD SubSysId;
    DWORD Revision;
    GUID  DeviceIdentifier;
    DWORD WHQLLevel;
} D3DADAPTER_IDENTIFIER8;

typedef enum _D3DDEVTYPE {
    D3DDEVTYPE_HAL         = 1,
    D3DDEVTYPE_REF         = 2,
    D3DDEVTYPE_SW          = 3,
    D3DDEVTYPE_FORCE_DWORD = 0x7fffffff
} D3DDEVTYPE;

#define D3DADAPTER_DEFAULT              0
#define D3DCREATE_SOFTWARE_VERTEXPROCESSING 0x00000020L
#define D3DCREATE_HARDWARE_VERTEXPROCESSING 0x00000040L
#define D3DCREATE_MIXED_VERTEXPROCESSING    0x00000080L
#define D3DCREATE_FPU_PRESERVE              0x00000002L
#define D3DENUM_NO_WHQL_LEVEL               0x00000002L

// The capability bits the engine tests before choosing a path.
#define D3DPTEXTURECAPS_POW2            0x00000002L
#define D3DPTEXTURECAPS_SQUAREONLY      0x00000020L
#define D3DPTEXTURECAPS_NONPOW2CONDITIONAL 0x00000100L
#define D3DPTFILTERCAPS_MINFLINEAR      0x00000200L
#define D3DPTFILTERCAPS_MAGFLINEAR      0x02000000L
#define D3DTEXOPCAPS_MODULATE           0x00000010L
#define D3DTEXOPCAPS_ADD                0x00000040L
#define D3DPBLENDCAPS_SRCALPHA          0x00000010L
#define D3DPRASTERCAPS_FOGTABLE         0x00000100L
#define D3DDEVCAPS_HWTRANSFORMANDLIGHT  0x00010000L
#define D3DPRESENT_RATE_DEFAULT         0x00000000

typedef struct _D3DCAPS8 {
    D3DDEVTYPE DeviceType;
    UINT       AdapterOrdinal;
    DWORD      Caps;
    DWORD      Caps2;
    DWORD      Caps3;
    DWORD      PresentationIntervals;
    DWORD      DevCaps;
    DWORD      PrimitiveMiscCaps;
    DWORD      RasterCaps;
    DWORD      ZCmpCaps;
    DWORD      SrcBlendCaps;
    DWORD      DestBlendCaps;
    DWORD      AlphaCmpCaps;
    DWORD      ShadeCaps;
    DWORD      TextureCaps;
    DWORD      TextureFilterCaps;
    DWORD      CubeTextureFilterCaps;
    DWORD      VolumeTextureFilterCaps;
    DWORD      TextureAddressCaps;
    DWORD      VolumeTextureAddressCaps;
    DWORD      LineCaps;
    DWORD      MaxTextureWidth;
    DWORD      MaxTextureHeight;
    DWORD      MaxVolumeExtent;
    DWORD      MaxTextureRepeat;
    DWORD      MaxTextureAspectRatio;
    DWORD      MaxAnisotropy;
    float      MaxVertexW;
    float      GuardBandLeft;
    float      GuardBandTop;
    float      GuardBandRight;
    float      GuardBandBottom;
    float      ExtentsAdjust;
    DWORD      StencilCaps;
    DWORD      FVFCaps;
    DWORD      TextureOpCaps;
    DWORD      MaxTextureBlendStages;
    DWORD      MaxSimultaneousTextures;
    DWORD      VertexProcessingCaps;
    DWORD      MaxActiveLights;
    DWORD      MaxUserClipPlanes;
    DWORD      MaxVertexBlendMatrices;
    DWORD      MaxVertexBlendMatrixIndex;
    float      MaxPointSize;
    DWORD      MaxPrimitiveCount;
    DWORD      MaxVertexIndex;
    DWORD      MaxStreams;
    DWORD      MaxStreamStride;
    DWORD      VertexShaderVersion;
    DWORD      MaxVertexShaderConst;
    DWORD      PixelShaderVersion;
    float      MaxPixelShaderValue;
} D3DCAPS8;

// The results the engine tests for.
#define D3D_OK                    S_OK
#define D3DERR_DEVICELOST         ( (HRESULT)0x88760868L )
#define D3DERR_DEVICENOTRESET     ( (HRESULT)0x88760869L )
#define D3DERR_INVALIDCALL        ( (HRESULT)0x8876086CL )
#define D3DERR_NOTAVAILABLE       ( (HRESULT)0x8876086AL )
#define D3DERR_OUTOFVIDEOMEMORY   ( (HRESULT)0x8876017CL )

typedef DWORD D3DCOLOR;

#define D3DCOLOR_ARGB( a, r, g, b ) \
    ( (D3DCOLOR)( ( ( (a) & 0xff ) << 24 ) | ( ( (r) & 0xff ) << 16 ) | \
                  ( ( (g) & 0xff ) << 8 ) | ( (b) & 0xff ) ) )
#define D3DCOLOR_RGBA( r, g, b, a ) D3DCOLOR_ARGB( a, r, g, b )
#define D3DCOLOR_XRGB( r, g, b )    D3DCOLOR_ARGB( 0xff, r, g, b )
