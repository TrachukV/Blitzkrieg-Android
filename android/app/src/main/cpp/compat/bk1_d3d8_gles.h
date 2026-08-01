#pragma once
// The internals of the Direct3D 8 replacement, shared between its parts.
//
// The renderer this stands in for is a fixed-function 2D blitter: the engine
// selects among five texture operations, hands over vertices that are already
// in screen space, and draws with depth test and write off for the ground.
// None of that needs a 3D pipeline, and all of it fits one small program with
// the stage configuration passed as uniforms.
#include "d3d8.h"

#include <GLES3/gl3.h>

#include <string.h>

#include <map>
#include <string>
#include <vector>

namespace NBk1D3D {

// The most texture stages the engine ever configures. It selects among
// MODULATE, ADD and the two SELECTARGs across at most two stages, so the
// shader carries two and nothing is lost.
const int MAX_STAGES = 2;
const int MAX_TEXTURE_UNITS = 2;

// ---------------------------------------------------------------------------
// The vertex layout, read out of the FVF code the engine sets
// ---------------------------------------------------------------------------
struct SVertexLayout
{
    bool bTransformed;      // XYZRHW: already in screen space
    bool bHasNormal;
    bool bHasDiffuse;
    bool bHasSpecular;
    int  nTexCoords;        // how many texture coordinate pairs

    int  nStride;
    int  nPositionOffset;
    int  nNormalOffset;
    int  nDiffuseOffset;
    int  nSpecularOffset;
    int  nTexCoordOffset[8];

    SVertexLayout()
        : bTransformed( false ), bHasNormal( false ), bHasDiffuse( false ),
          bHasSpecular( false ), nTexCoords( 0 ), nStride( 0 ),
          nPositionOffset( 0 ), nNormalOffset( -1 ), nDiffuseOffset( -1 ),
          nSpecularOffset( -1 )
    {
        for ( int i = 0; i < 8; ++i )
            nTexCoordOffset[i] = -1;
    }
};

// Works out the layout a flexible vertex format describes.
SVertexLayout LayoutFromFVF( DWORD dwFVF );

// ---------------------------------------------------------------------------
// The state the engine sets, kept as it comes in and applied at draw time
// ---------------------------------------------------------------------------
struct SStageState
{
    DWORD dwColorOp;
    DWORD dwColorArg1;
    DWORD dwColorArg2;
    DWORD dwAlphaOp;
    DWORD dwAlphaArg1;
    DWORD dwAlphaArg2;
    DWORD dwTexCoordIndex;
    DWORD dwAddressU;
    DWORD dwAddressV;
    DWORD dwMagFilter;
    DWORD dwMinFilter;

    SStageState()
        : dwColorOp( D3DTOP_DISABLE ), dwColorArg1( D3DTA_TEXTURE ),
          dwColorArg2( D3DTA_CURRENT ), dwAlphaOp( D3DTOP_DISABLE ),
          dwAlphaArg1( D3DTA_TEXTURE ), dwAlphaArg2( D3DTA_CURRENT ),
          dwTexCoordIndex( 0 ), dwAddressU( D3DTADDRESS_WRAP ),
          dwAddressV( D3DTADDRESS_WRAP ), dwMagFilter( D3DTEXF_POINT ),
          dwMinFilter( D3DTEXF_POINT ) {}
};

// ---------------------------------------------------------------------------
// Resources
// ---------------------------------------------------------------------------
struct STextureLevel
{
    UINT              nWidth;
    UINT              nHeight;
    std::vector<BYTE> data;      // the system copy the engine locks and writes
    bool              bDirty;

    STextureLevel() : nWidth( 0 ), nHeight( 0 ), bDirty( false ) {}
};

// A surface is a view onto one level of a texture, or a standalone image the
// engine allocates to read pixels into.
struct STexture;

struct SSurface : public IDirect3DSurface8
{
    LONG      nRefCount;
    STexture *pOwner;            // null when the surface stands alone
    UINT      nLevel;
    UINT      nWidth;
    UINT      nHeight;
    D3DFORMAT format;
    std::vector<BYTE> data;      // used only when standalone

    SSurface();
    ~SSurface();

    HRESULT STDCALL QueryInterface( REFIID, void **ppvObject ) override;
    ULONG   STDCALL AddRef() override;
    ULONG   STDCALL Release() override;
    HRESULT STDCALL GetDesc( D3DSURFACE_DESC *pDesc ) override;
    HRESULT STDCALL LockRect( D3DLOCKED_RECT *pLockedRect, const RECT *pRect,
                              DWORD dwFlags ) override;
    HRESULT STDCALL UnlockRect() override;

    BYTE *Pixels();
    int   Pitch() const;
};

struct STexture : public IDirect3DTexture8
{
    LONG       nRefCount;
    UINT       nWidth;
    UINT       nHeight;
    UINT       nLevels;
    D3DFORMAT  format;
    DWORD      dwUsage;
    std::vector<STextureLevel> levels;
    GLuint     nGLTexture;
    bool       bUploaded;

    STexture();
    ~STexture();

    HRESULT STDCALL QueryInterface( REFIID, void **ppvObject ) override;
    ULONG   STDCALL AddRef() override;
    ULONG   STDCALL Release() override;
    DWORD   STDCALL GetLevelCount() override;
    HRESULT STDCALL GetLevelDesc( UINT nLevel, D3DSURFACE_DESC *pDesc ) override;
    HRESULT STDCALL GetSurfaceLevel( UINT nLevel, IDirect3DSurface8 **ppSurface ) override;
    HRESULT STDCALL LockRect( UINT nLevel, D3DLOCKED_RECT *pLockedRect,
                              const RECT *pRect, DWORD dwFlags ) override;
    HRESULT STDCALL UnlockRect( UINT nLevel ) override;
    HRESULT STDCALL AddDirtyRect( const RECT *pDirtyRect ) override;

    // Sends whatever changed to the GPU. DXT levels go up compressed when the
    // device supports the format and are expanded here when it does not.
    void EnsureUploaded();
    int  BytesPerRow( UINT nLevel ) const;
};

struct SVertexBuffer : public IDirect3DVertexBuffer8
{
    LONG              nRefCount;
    std::vector<BYTE> data;
    DWORD             dwFVF;
    GLuint            nGLBuffer;
    bool              bDirty;

    SVertexBuffer();
    ~SVertexBuffer();

    HRESULT STDCALL QueryInterface( REFIID, void **ppvObject ) override;
    ULONG   STDCALL AddRef() override;
    ULONG   STDCALL Release() override;
    HRESULT STDCALL Lock( UINT nOffsetToLock, UINT nSizeToLock, BYTE **ppbData,
                          DWORD dwFlags ) override;
    HRESULT STDCALL Unlock() override;

    void EnsureUploaded();
};

struct SIndexBuffer : public IDirect3DIndexBuffer8
{
    LONG              nRefCount;
    std::vector<BYTE> data;
    D3DFORMAT         format;      // INDEX16 or INDEX32
    GLuint            nGLBuffer;
    bool              bDirty;

    SIndexBuffer();
    ~SIndexBuffer();

    HRESULT STDCALL QueryInterface( REFIID, void **ppvObject ) override;
    ULONG   STDCALL AddRef() override;
    ULONG   STDCALL Release() override;
    HRESULT STDCALL Lock( UINT nOffsetToLock, UINT nSizeToLock, BYTE **ppbData,
                          DWORD dwFlags ) override;
    HRESULT STDCALL Unlock() override;

    void EnsureUploaded();
};

// ---------------------------------------------------------------------------
// The program that stands in for the fixed-function stages
// ---------------------------------------------------------------------------
struct SProgram
{
    GLuint nProgram;

    // vertex inputs
    GLint  nPositionAttrib;
    GLint  nNormalAttrib;
    GLint  nDiffuseAttrib;
    GLint  nSpecularAttrib;
    GLint  nTexCoordAttrib[MAX_TEXTURE_UNITS];

    // uniforms
    GLint  nTransformUniform;      // world * view * projection, or the screen
                                   // mapping for pre-transformed vertices
    GLint  nTransformedUniform;    // whether the position is already in pixels
    GLint  nTextureFactorUniform;
    GLint  nSamplerUniform[MAX_TEXTURE_UNITS];
    GLint  nStageOpUniform;        // colour op per stage
    GLint  nStageArgUniform;       // colour args, packed
    GLint  nAlphaOpUniform;
    GLint  nAlphaArgUniform;
    GLint  nAlphaTestUniform;      // enabled, function, reference
    GLint  nHasTextureUniform;     // whether each stage has a texture at all

    // fixed-function lighting. The engine draws a unit's shadow by drawing the
    // unit again with a black, part-transparent material and every light off,
    // so the material is not decoration here -- without it the shadow comes out
    // as a second solid copy of the vehicle.
    GLint  nLightingUniform;       // lighting on at all
    GLint  nWorldUniform;          // for putting the normal in world space
    GLint  nMatDiffuseUniform;
    GLint  nMatAmbientUniform;
    GLint  nMatEmissiveUniform;
    GLint  nGlobalAmbientUniform;
    GLint  nLightCountUniform;
    GLint  nLightDirUniform;       // towards the light, world space
    GLint  nLightDiffuseUniform;
    GLint  nLightAmbientUniform;

    SProgram() : nProgram( 0 ) {}
};

// What GL was last told.
//
// Every draw used to re-issue the whole pipeline: the program, four stage
// uniforms, the texture factor, the alpha test, two texture binds with four
// parameters each, the transform, the viewport, the attribute pointers -- some
// forty-five calls, whether or not anything had changed. Measured on a mission
// frame that was 180 draws and 29.8 ms of GL out of a 30.5 ms step, with the
// game's own thinking under a millisecond of it. The calls were the frame.
//
// So each one is now compared against what was last sent and skipped when it
// would say the same thing again. Nothing here changes what is drawn; it only
// stops saying it twice.
struct SGLCache
{
    bool   bValid;
    GLuint nProgram;

    bool   bDepthTest;
    GLenum eDepthFunc;
    bool   bDepthMask;

    bool   bBlend;
    GLenum eSrcBlend, eDstBlend;

    int    nCull;                  // 0 none, 1 front CCW, 2 front CW

    bool   bStencil;
    GLenum eStencilFunc;
    GLint  nStencilRef;
    GLuint nStencilMask;
    GLenum eStencilFail, eStencilZFail, eStencilPass;
    GLuint nStencilWrite;

    GLint  colorOps[2], alphaOps[2];
    GLint  colorArgs[4], alphaArgs[4];
    float  fFactor[4];
    float  fAlphaTest[3];
    GLint  nHasTexture[2];
    bool   bSamplersBound;

    GLuint nTexture[MAX_TEXTURE_UNITS];
    GLint  nWrapS[MAX_TEXTURE_UNITS], nWrapT[MAX_TEXTURE_UNITS];
    GLint  nMagFilter[MAX_TEXTURE_UNITS], nMinFilter[MAX_TEXTURE_UNITS];
    GLenum eActiveUnit;

    float  matCombined[16];
    float  matWorld[16];
    GLint  nTransformed;
    float  fViewport[2];
    GLint  nLighting;

    // the vertex layout last bound, and the buffer it pointed into
    GLuint nArrayBuffer;
    int    nStride;
    int    nBaseOffset;
    DWORD  dwLayoutKey;

    SGLCache() { Invalidate(); }
    void Invalidate() { memset( this, 0, sizeof( *this ) ); bValid = false; }
};

// Throws away what GL was last told, so the next draw says all of it again.
//
// Anything that can make the cache disagree with GL has to call this. Deleting
// a texture is the dangerous one: GL hands the same name out again, so a cache
// still holding it will skip the bind for a completely different texture. That
// is not theoretical -- it is why the second mission of a run drew nothing.
void ForgetGLState();

bool BuildProgram( SProgram *pProgram );

}   // namespace NBk1D3D
