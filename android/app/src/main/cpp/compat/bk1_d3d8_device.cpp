// The Direct3D 8 device, over OpenGL ES.
//
// The engine sets fixed-function state as it goes and then draws; this keeps
// that state as it arrives and applies it at the draw, because GLES has no
// state to set until there is a program bound and a vertex layout to describe.
//
// The engine's own renderer is a 2D blitter -- pre-transformed vertices, depth
// off for the ground, five texture operations -- so nothing here reaches for a
// pipeline that is not needed.
#include "bk1_d3d8_gles.h"

#include <android/log.h>
#include <sys/system_properties.h>

#include <math.h>
#include <string.h>

// How much of a frame goes into talking to GL. The frame breakdown already
// showed the cost sits inside the engine's step rather than at the swap; this
// splits that step again, into the game thinking and the driver being told.
// At global scope because the frame loop reads them.
double g_fGLMs = 0.0;
long long g_nDrawCalls = 0;

namespace NBk1D3D {

namespace {

// ---------------------------------------------------------------------------
// State translation
// ---------------------------------------------------------------------------
GLenum BlendFactor( DWORD dwBlend )
{
    switch ( dwBlend )
    {
    case D3DBLEND_ZERO:         return GL_ZERO;
    case D3DBLEND_ONE:          return GL_ONE;
    case D3DBLEND_SRCCOLOR:     return GL_SRC_COLOR;
    case D3DBLEND_INVSRCCOLOR:  return GL_ONE_MINUS_SRC_COLOR;
    case D3DBLEND_SRCALPHA:     return GL_SRC_ALPHA;
    case D3DBLEND_INVSRCALPHA:  return GL_ONE_MINUS_SRC_ALPHA;
    case D3DBLEND_DESTALPHA:    return GL_DST_ALPHA;
    case D3DBLEND_INVDESTALPHA: return GL_ONE_MINUS_DST_ALPHA;
    case D3DBLEND_DESTCOLOR:    return GL_DST_COLOR;
    case D3DBLEND_INVDESTCOLOR: return GL_ONE_MINUS_DST_COLOR;
    case D3DBLEND_SRCALPHASAT:  return GL_SRC_ALPHA_SATURATE;
    default:                    return GL_ONE;
    }
}

GLenum CompareFunc( DWORD dwFunc )
{
    switch ( dwFunc )
    {
    case D3DCMP_NEVER:        return GL_NEVER;
    case D3DCMP_LESS:         return GL_LESS;
    case D3DCMP_EQUAL:        return GL_EQUAL;
    case D3DCMP_LESSEQUAL:    return GL_LEQUAL;
    case D3DCMP_GREATER:      return GL_GREATER;
    case D3DCMP_NOTEQUAL:     return GL_NOTEQUAL;
    case D3DCMP_GREATEREQUAL: return GL_GEQUAL;
    default:                  return GL_ALWAYS;
    }
}

GLenum StencilOp( DWORD dwOp )
{
    switch ( dwOp )
    {
    case D3DSTENCILOP_ZERO:    return GL_ZERO;
    case D3DSTENCILOP_REPLACE: return GL_REPLACE;
    case D3DSTENCILOP_INCRSAT: return GL_INCR;
    case D3DSTENCILOP_DECRSAT: return GL_DECR;
    case D3DSTENCILOP_INVERT:  return GL_INVERT;
    case D3DSTENCILOP_INCR:    return GL_INCR_WRAP;
    case D3DSTENCILOP_DECR:    return GL_DECR_WRAP;
    default:                   return GL_KEEP;
    }
}

GLenum AddressMode( DWORD dwAddress )
{
    // GLES has no border mode; clamping to the edge is the nearest thing and
    // the engine only uses it on sprites that do not reach their border.
    return ( dwAddress == D3DTADDRESS_WRAP ) ? GL_REPEAT : GL_CLAMP_TO_EDGE;
}

// How many vertices a primitive count covers.
int VertexCount( D3DPRIMITIVETYPE type, UINT nPrimitives )
{
    switch ( type )
    {
    case D3DPT_POINTLIST:     return (int)nPrimitives;
    case D3DPT_LINELIST:      return (int)nPrimitives * 2;
    case D3DPT_LINESTRIP:     return (int)nPrimitives + 1;
    case D3DPT_TRIANGLELIST:  return (int)nPrimitives * 3;
    case D3DPT_TRIANGLESTRIP:
    case D3DPT_TRIANGLEFAN:   return (int)nPrimitives + 2;
    default:                  return 0;
    }
}

GLenum PrimitiveMode( D3DPRIMITIVETYPE type )
{
    switch ( type )
    {
    case D3DPT_POINTLIST:     return GL_POINTS;
    case D3DPT_LINELIST:      return GL_LINES;
    case D3DPT_LINESTRIP:     return GL_LINE_STRIP;
    case D3DPT_TRIANGLESTRIP: return GL_TRIANGLE_STRIP;
    case D3DPT_TRIANGLEFAN:   return GL_TRIANGLE_FAN;
    default:                  return GL_TRIANGLES;
    }
}

void MultiplyMatrix( const D3DMATRIX &a, const D3DMATRIX &b, D3DMATRIX *pOut )
{
    for ( int r = 0; r < 4; ++r )
    {
        for ( int c = 0; c < 4; ++c )
        {
            float f = 0.0f;
            for ( int k = 0; k < 4; ++k )
                f += a.m[r][k] * b.m[k][c];
            pOut->m[r][c] = f;
        }
    }
}

void IdentityMatrix( D3DMATRIX *pOut )
{
    memset( pOut, 0, sizeof( *pOut ) );
    pOut->m[0][0] = pOut->m[1][1] = pOut->m[2][2] = pOut->m[3][3] = 1.0f;
}

// ---------------------------------------------------------------------------
// The device
// ---------------------------------------------------------------------------
struct SDevice : public IDirect3DDevice8
{
    LONG nRefCount;

    D3DPRESENT_PARAMETERS present;
    SProgram              program;
    bool                  bProgramBuilt;

    // fixed-function state, as the engine sets it
    DWORD       renderStates[256];
    SStageState stages[MAX_STAGES];
    STexture   *pStageTexture[MAX_STAGES];

    D3DMATRIX matWorld;
    SGLCache  cache;
    int       nDrawInFrame;
    int       nFrameIndex;

    // fixed-function lighting state
    enum { MAX_LIGHTS = 4 };
    D3DMATERIAL8 material;
    D3DLIGHT8    lights[MAX_LIGHTS];
    bool         bLightEnabled[MAX_LIGHTS];
    D3DMATRIX matView;
    D3DMATRIX matProjection;

    D3DVIEWPORT8 viewport;

    // geometry
    SVertexBuffer *pStream;
    UINT           nStreamStride;
    SIndexBuffer  *pIndices;
    UINT           nBaseVertexIndex;
    DWORD          dwFVF;

    GLuint nVertexArray;

    SDevice();
    ~SDevice();

    // --- IUnknown ---
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

    // --- lifetime ---
    HRESULT STDCALL TestCooperativeLevel() override { return D3D_OK; }
    UINT    STDCALL GetAvailableTextureMem() override { return 256u * 1024u * 1024u; }

    HRESULT STDCALL Reset( D3DPRESENT_PARAMETERS *pParameters ) override
    {
        if ( pParameters != 0 )
        {
            present = *pParameters;
            // The size the engine draws at, not the surface's. They were the
            // same until the menus turned out to be authored at a fixed
            // 1024x768; writing the back buffer's size into the surface's made
            // the scale one to one again and left the picture in a corner.
            Bk1SetPresentSize( (int)present.BackBufferWidth,
                               (int)present.BackBufferHeight );
        }
        return D3D_OK;
    }

    // The swap belongs to the Android layer, which owns the EGL surface: it
    // presents after the engine's frame returns. Here the queue is only
    // flushed so the frame is complete when that happens.
    HRESULT STDCALL Present( const RECT *, const RECT *, HWND, const void * ) override
    {
        glFlush();
        return D3D_OK;
    }

    HRESULT STDCALL GetFrontBuffer( IDirect3DSurface8 *pDestSurface ) override
    {
        // Used for screenshots. The surface is filled from the framebuffer.
        SSurface *pSurface = (SSurface *)pDestSurface;
        if ( pSurface == 0 )
            return D3DERR_INVALIDCALL;
        BYTE *pPixels = pSurface->Pixels();
        if ( pPixels == 0 )
            return D3DERR_INVALIDCALL;

        glReadPixels( 0, 0, (GLsizei)pSurface->nWidth, (GLsizei)pSurface->nHeight,
                      GL_RGBA, GL_UNSIGNED_BYTE, pPixels );
        // GLES reads bottom-up in R,G,B,A; the engine expects top-down in the
        // order a 32-bit ARGB word holds.
        const int nPitch = pSurface->Pitch();
        std::vector<BYTE> row( (size_t)nPitch );
        for ( UINT y = 0; y < pSurface->nHeight / 2; ++y )
        {
            BYTE *pTop = pPixels + (size_t)y * nPitch;
            BYTE *pBottom = pPixels + (size_t)( pSurface->nHeight - 1 - y ) * nPitch;
            memcpy( &row[0], pTop, nPitch );
            memcpy( pTop, pBottom, nPitch );
            memcpy( pBottom, &row[0], nPitch );
        }
        for ( size_t p = 0; p + 3 < (size_t)nPitch * pSurface->nHeight; p += 4 )
        {
            const BYTE r = pPixels[p];
            pPixels[p] = pPixels[p + 2];
            pPixels[p + 2] = r;
        }
        return D3D_OK;
    }

    // Android manages display gamma; the engine's ramp is accepted and kept so
    // a read returns what was written.
    D3DGAMMARAMP gammaRamp;
    void STDCALL SetGammaRamp( DWORD, const D3DGAMMARAMP *pRamp ) override
    {
        if ( pRamp != 0 )
            gammaRamp = *pRamp;
    }
    void STDCALL GetGammaRamp( D3DGAMMARAMP *pRamp ) override
    {
        if ( pRamp != 0 )
            *pRamp = gammaRamp;
    }

    // --- resources ---
    HRESULT STDCALL CreateTexture( UINT nWidth, UINT nHeight, UINT nLevels, DWORD dwUsage,
                                   D3DFORMAT format, D3DPOOL,
                                   IDirect3DTexture8 **ppTexture ) override;
    HRESULT STDCALL CreateVertexBuffer( UINT nLength, DWORD, DWORD dwVertexFVF, D3DPOOL,
                                        IDirect3DVertexBuffer8 **ppBuffer ) override;
    HRESULT STDCALL CreateIndexBuffer( UINT nLength, DWORD, D3DFORMAT format, D3DPOOL,
                                       IDirect3DIndexBuffer8 **ppBuffer ) override;
    HRESULT STDCALL CreateDepthStencilSurface( UINT nWidth, UINT nHeight, D3DFORMAT format,
                                               D3DMULTISAMPLE_TYPE,
                                               IDirect3DSurface8 **ppSurface ) override;
    HRESULT STDCALL CreateImageSurface( UINT nWidth, UINT nHeight, D3DFORMAT format,
                                        IDirect3DSurface8 **ppSurface ) override;

    HRESULT STDCALL CopyRects( IDirect3DSurface8 *pSource, const RECT *pSourceRects,
                               UINT nRects, IDirect3DSurface8 *pDest,
                               const POINT *pDestPoints ) override;
    HRESULT STDCALL UpdateTexture( IDirect3DBaseTexture8 *pSource,
                                   IDirect3DBaseTexture8 *pDest ) override;

    // --- render targets ---
    // Rendering goes to the surface the Android layer made current. The engine
    // asks for the target so it can put it back afterwards, which costs
    // nothing to answer.
    HRESULT STDCALL GetRenderTarget( IDirect3DSurface8 **ppRenderTarget ) override
    {
        if ( ppRenderTarget != 0 )
            *ppRenderTarget = 0;
        return D3D_OK;
    }
    HRESULT STDCALL GetDepthStencilSurface( IDirect3DSurface8 **ppSurface ) override
    {
        if ( ppSurface != 0 )
            *ppSurface = 0;
        return D3D_OK;
    }
    HRESULT STDCALL SetRenderTarget( IDirect3DSurface8 *, IDirect3DSurface8 * ) override
    {
        return D3D_OK;
    }

    // --- the frame ---
    HRESULT STDCALL BeginScene() override
    {
        EnsureProgram();
        // The frame boundary, so a trace can name a draw by its place in the
        // frame. Sampling every N-th draw compares whatever happened to land
        // there, and the engine draws terrain and meshes in a fixed order --
        // so one run's sample was terrain and another's a mesh, which carry
        // different world matrices by design. Comparing draw 0 with draw 0
        // compares the same pass.
        nDrawInFrame = 0;
        ++nFrameIndex;
        return D3D_OK;
    }
    HRESULT STDCALL EndScene() override { return D3D_OK; }
    HRESULT STDCALL Clear( DWORD nCount, const void *pRects, DWORD dwFlags,
                           D3DCOLOR color, float fZ, DWORD dwStencil ) override;

    // --- state ---
    HRESULT STDCALL SetTransform( D3DTRANSFORMSTATETYPE state,
                                  const D3DMATRIX *pMatrix ) override;
    HRESULT STDCALL SetViewport( const D3DVIEWPORT8 *pViewport ) override;
    // The engine lights its meshes with one directional sun, and draws their
    // shadows by drawing the mesh again with every light off and a material
    // that is black with a little alpha. Answering D3D_OK and doing nothing
    // here is what put a second, fully lit copy of every vehicle on the
    // ground beside it.
    HRESULT STDCALL SetMaterial( const D3DMATERIAL8 *pMaterial ) override
    {
        if ( pMaterial != 0 )
            material = *pMaterial;
        return D3D_OK;
    }

    HRESULT STDCALL SetLight( DWORD nIndex, const D3DLIGHT8 *pLight ) override
    {
        if ( pLight != 0 && nIndex < MAX_LIGHTS )
            lights[nIndex] = *pLight;
        return D3D_OK;
    }

    HRESULT STDCALL LightEnable( DWORD nIndex, BOOL bEnable ) override
    {
        if ( nIndex < MAX_LIGHTS )
            bLightEnabled[nIndex] = bEnable != FALSE;
        return D3D_OK;
    }
    HRESULT STDCALL SetRenderState( D3DRENDERSTATETYPE state, DWORD dwValue ) override;
    HRESULT STDCALL SetTexture( DWORD nStage, IDirect3DBaseTexture8 *pTexture ) override;
    HRESULT STDCALL SetTextureStageState( DWORD nStage, D3DTEXTURESTAGESTATETYPE type,
                                          DWORD dwValue ) override;

    // --- geometry ---
    HRESULT STDCALL SetStreamSource( UINT, IDirect3DVertexBuffer8 *pStreamData,
                                     UINT nStride ) override
    {
        pStream = (SVertexBuffer *)pStreamData;
        nStreamStride = nStride;
        return D3D_OK;
    }
    HRESULT STDCALL SetIndices( IDirect3DIndexBuffer8 *pIndexData,
                                UINT nBaseIndex ) override
    {
        pIndices = (SIndexBuffer *)pIndexData;
        nBaseVertexIndex = nBaseIndex;
        return D3D_OK;
    }
    HRESULT STDCALL SetVertexShader( DWORD dwHandle ) override
    {
        // An FVF code, never a shader: there are none in the tree.
        dwFVF = dwHandle;
        return D3D_OK;
    }
    HRESULT STDCALL DrawPrimitive( D3DPRIMITIVETYPE type, UINT nStartVertex,
                                   UINT nPrimitiveCount ) override;
    HRESULT STDCALL DrawIndexedPrimitive( D3DPRIMITIVETYPE type, UINT nMinIndex,
                                          UINT nNumVertices, UINT nStartIndex,
                                          UINT nPrimitiveCount ) override;

    // --- internals ---
    void EnsureProgram();
    void ApplyState();
    void BindVertexLayout( const SVertexLayout &layout, int nBaseOffset );
};

// The live device, so that a texture being destroyed can reach the cache that
// may still be holding its name. One device exists at a time here.
static SDevice *g_pLiveDevice = 0;

SDevice::SDevice()
    : nRefCount( 1 ), bProgramBuilt( false ), pStream( 0 ), nStreamStride( 0 ),
      material(), 
      pIndices( 0 ), nBaseVertexIndex( 0 ), dwFVF( 0 ), nVertexArray( 0 )
{
    g_pLiveDevice = this;
    nDrawInFrame = 0;
    nFrameIndex = 0;
    memset( &present, 0, sizeof( present ) );
    memset( renderStates, 0, sizeof( renderStates ) );
    memset( pStageTexture, 0, sizeof( pStageTexture ) );
    memset( &gammaRamp, 0, sizeof( gammaRamp ) );
    IdentityMatrix( &matWorld );
    IdentityMatrix( &matView );
    IdentityMatrix( &matProjection );
    memset( &viewport, 0, sizeof( viewport ) );

    // The defaults Direct3D starts a device with, so that state the engine
    // never sets still behaves as it did.
    renderStates[D3DRS_ZENABLE] = D3DZB_TRUE;
    renderStates[D3DRS_ZWRITEENABLE] = TRUE;
    renderStates[D3DRS_ZFUNC] = D3DCMP_LESSEQUAL;
    renderStates[D3DRS_ALPHABLENDENABLE] = FALSE;
    renderStates[D3DRS_SRCBLEND] = D3DBLEND_ONE;
    renderStates[D3DRS_DESTBLEND] = D3DBLEND_ZERO;
    renderStates[D3DRS_ALPHATESTENABLE] = FALSE;
    renderStates[D3DRS_ALPHAFUNC] = D3DCMP_ALWAYS;
    renderStates[D3DRS_ALPHAREF] = 0;
    renderStates[D3DRS_CULLMODE] = D3DCULL_CCW;
    // Direct3D starts with lighting on and no ambient; the engine turns it off
    // for everything that is not a mesh, so the default has to be the real one.
    memset( &material, 0, sizeof( material ) );
    memset( lights, 0, sizeof( lights ) );
    for ( int i = 0; i < MAX_LIGHTS; ++i )
        bLightEnabled[i] = false;

    renderStates[D3DRS_LIGHTING] = TRUE;
    renderStates[D3DRS_AMBIENT] = 0;
    renderStates[D3DRS_STENCILENABLE] = FALSE;
    renderStates[D3DRS_STENCILFUNC] = D3DCMP_ALWAYS;
    renderStates[D3DRS_STENCILMASK] = 0xFFFFFFFF;
    renderStates[D3DRS_STENCILWRITEMASK] = 0xFFFFFFFF;
    renderStates[D3DRS_TEXTUREFACTOR] = 0xFFFFFFFF;
}

SDevice::~SDevice()
{
    if ( nVertexArray != 0 )
        glDeleteVertexArrays( 1, &nVertexArray );
    if ( program.nProgram != 0 )
        glDeleteProgram( program.nProgram );
}

void SDevice::EnsureProgram()
{
    if ( bProgramBuilt )
        return;
    bProgramBuilt = true;               // one attempt; a failure is logged once
    BuildProgram( &program );
    if ( nVertexArray == 0 )
        glGenVertexArrays( 1, &nVertexArray );
}

// ---------------------------------------------------------------------------
// Resource creation
// ---------------------------------------------------------------------------
HRESULT STDCALL SDevice::CreateTexture( UINT nWidth, UINT nHeight, UINT nLevels,
                                        DWORD dwUsage, D3DFORMAT format, D3DPOOL,
                                        IDirect3DTexture8 **ppTexture )
{
    if ( ppTexture == 0 || nWidth == 0 || nHeight == 0 )
        return D3DERR_INVALIDCALL;

    STexture *pTexture = new STexture();
    pTexture->nWidth = nWidth;
    pTexture->nHeight = nHeight;
    pTexture->format = format;
    pTexture->dwUsage = dwUsage;

    // Zero levels means the full chain, as Direct3D reads it.
    if ( nLevels == 0 )
    {
        nLevels = 1;
        UINT w = nWidth, h = nHeight;
        while ( w > 1 || h > 1 )
        {
            w = w > 1 ? w / 2 : 1;
            h = h > 1 ? h / 2 : 1;
            ++nLevels;
        }
    }
    pTexture->nLevels = nLevels;
    pTexture->levels.resize( nLevels );

    UINT w = nWidth, h = nHeight;
    for ( UINT i = 0; i < nLevels; ++i )
    {
        pTexture->levels[i].nWidth = w;
        pTexture->levels[i].nHeight = h;
        const int nBytes = ( format == D3DFMT_DXT1 || format == D3DFMT_DXT2 ||
                             format == D3DFMT_DXT3 || format == D3DFMT_DXT4 ||
                             format == D3DFMT_DXT5 )
                               ? ( (int)( ( w + 3 ) / 4 ) * (int)( ( h + 3 ) / 4 ) *
                                   ( format == D3DFMT_DXT1 ? 8 : 16 ) )
                               : (int)( w * h * 4 );
        pTexture->levels[i].data.assign( (size_t)nBytes, 0 );
        w = w > 1 ? w / 2 : 1;
        h = h > 1 ? h / 2 : 1;
    }

    *ppTexture = pTexture;
    return D3D_OK;
}

HRESULT STDCALL SDevice::CreateVertexBuffer( UINT nLength, DWORD, DWORD dwVertexFVF,
                                             D3DPOOL, IDirect3DVertexBuffer8 **ppBuffer )
{
    if ( ppBuffer == 0 )
        return D3DERR_INVALIDCALL;
    SVertexBuffer *pBuffer = new SVertexBuffer();
    pBuffer->data.assign( nLength, 0 );
    pBuffer->dwFVF = dwVertexFVF;
    *ppBuffer = pBuffer;
    return D3D_OK;
}

HRESULT STDCALL SDevice::CreateIndexBuffer( UINT nLength, DWORD, D3DFORMAT format,
                                            D3DPOOL, IDirect3DIndexBuffer8 **ppBuffer )
{
    if ( ppBuffer == 0 )
        return D3DERR_INVALIDCALL;
    SIndexBuffer *pBuffer = new SIndexBuffer();
    pBuffer->data.assign( nLength, 0 );
    pBuffer->format = format;
    *ppBuffer = pBuffer;
    return D3D_OK;
}

HRESULT STDCALL SDevice::CreateDepthStencilSurface( UINT nWidth, UINT nHeight,
                                                    D3DFORMAT format,
                                                    D3DMULTISAMPLE_TYPE,
                                                    IDirect3DSurface8 **ppSurface )
{
    // The depth buffer belongs to the EGL surface the Android layer created;
    // the engine only holds this to put it back as the target.
    if ( ppSurface == 0 )
        return D3DERR_INVALIDCALL;
    SSurface *pSurface = new SSurface();
    pSurface->nWidth = nWidth;
    pSurface->nHeight = nHeight;
    pSurface->format = format;
    *ppSurface = pSurface;
    return D3D_OK;
}

HRESULT STDCALL SDevice::CreateImageSurface( UINT nWidth, UINT nHeight, D3DFORMAT format,
                                             IDirect3DSurface8 **ppSurface )
{
    if ( ppSurface == 0 )
        return D3DERR_INVALIDCALL;
    SSurface *pSurface = new SSurface();
    pSurface->nWidth = nWidth;
    pSurface->nHeight = nHeight;
    pSurface->format = format;
    pSurface->data.assign( (size_t)nWidth * nHeight * 4, 0 );
    *ppSurface = pSurface;
    return D3D_OK;
}

HRESULT STDCALL SDevice::CopyRects( IDirect3DSurface8 *pSource, const RECT *pSourceRects,
                                    UINT nRects, IDirect3DSurface8 *pDest,
                                    const POINT *pDestPoints )
{
    SSurface *pFrom = (SSurface *)pSource;
    SSurface *pTo = (SSurface *)pDest;
    if ( pFrom == 0 || pTo == 0 )
        return D3DERR_INVALIDCALL;

    BYTE *pSrcBits = pFrom->Pixels();
    BYTE *pDstBits = pTo->Pixels();
    if ( pSrcBits == 0 || pDstBits == 0 )
        return D3DERR_INVALIDCALL;

    const int nSrcPitch = pFrom->Pitch();
    const int nDstPitch = pTo->Pitch();
    const int nBytesPerPixel = 4;

    // No rectangles means the whole surface, as Direct3D reads it.
    if ( nRects == 0 || pSourceRects == 0 )
    {
        const UINT nHeight = pFrom->nHeight < pTo->nHeight ? pFrom->nHeight : pTo->nHeight;
        const int nRowBytes = nSrcPitch < nDstPitch ? nSrcPitch : nDstPitch;
        for ( UINT y = 0; y < nHeight; ++y )
            memcpy( pDstBits + (size_t)y * nDstPitch, pSrcBits + (size_t)y * nSrcPitch,
                    nRowBytes );
    }
    else
    {
        for ( UINT i = 0; i < nRects; ++i )
        {
            const RECT &rc = pSourceRects[i];
            const int nX = ( pDestPoints != 0 ) ? (int)pDestPoints[i].x : (int)rc.left;
            const int nY = ( pDestPoints != 0 ) ? (int)pDestPoints[i].y : (int)rc.top;
            const int nWidth = (int)( rc.right - rc.left );
            for ( int y = 0; y < (int)( rc.bottom - rc.top ); ++y )
            {
                memcpy( pDstBits + (size_t)( nY + y ) * nDstPitch + (size_t)nX * nBytesPerPixel,
                        pSrcBits + (size_t)( rc.top + y ) * nSrcPitch +
                            (size_t)rc.left * nBytesPerPixel,
                        (size_t)nWidth * nBytesPerPixel );
            }
        }
    }

    if ( pTo->pOwner != 0 )
        pTo->pOwner->bUploaded = false;
    return D3D_OK;
}

HRESULT STDCALL SDevice::UpdateTexture( IDirect3DBaseTexture8 *pSource,
                                        IDirect3DBaseTexture8 *pDest )
{
    STexture *pFrom = (STexture *)pSource;
    STexture *pTo = (STexture *)pDest;
    if ( pFrom == 0 || pTo == 0 )
        return D3DERR_INVALIDCALL;

    const size_t nLevels = pFrom->levels.size() < pTo->levels.size()
                               ? pFrom->levels.size() : pTo->levels.size();
    for ( size_t i = 0; i < nLevels; ++i )
    {
        const size_t n = pFrom->levels[i].data.size() < pTo->levels[i].data.size()
                             ? pFrom->levels[i].data.size() : pTo->levels[i].data.size();
        if ( n > 0 )
            memcpy( &pTo->levels[i].data[0], &pFrom->levels[i].data[0], n );
    }
    pTo->bUploaded = false;
    return D3D_OK;
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
HRESULT STDCALL SDevice::Clear( DWORD, const void *, DWORD dwFlags, D3DCOLOR color,
                                float fZ, DWORD dwStencil )
{
    GLbitfield nMask = 0;
    if ( dwFlags & D3DCLEAR_TARGET )
    {
        // D3DCOLOR is A,R,G,B packed into a word.
        const float fA = (float)( ( color >> 24 ) & 0xFF ) / 255.0f;
        const float fR = (float)( ( color >> 16 ) & 0xFF ) / 255.0f;
        const float fG = (float)( ( color >> 8 ) & 0xFF ) / 255.0f;
        const float fB = (float)( color & 0xFF ) / 255.0f;
        glClearColor( fR, fG, fB, fA );
        nMask |= GL_COLOR_BUFFER_BIT;
    }
    if ( dwFlags & D3DCLEAR_ZBUFFER )
    {
        glClearDepthf( fZ );
        // A depth clear is refused when writing is off, so it is turned on for
        // the clear and put back.
        glDepthMask( GL_TRUE );
        nMask |= GL_DEPTH_BUFFER_BIT;
    }
    if ( dwFlags & D3DCLEAR_STENCIL )
    {
        glClearStencil( (GLint)dwStencil );
        nMask |= GL_STENCIL_BUFFER_BIT;
    }
    if ( nMask != 0 )
        glClear( nMask );
    if ( dwFlags & D3DCLEAR_ZBUFFER )
        glDepthMask( renderStates[D3DRS_ZWRITEENABLE] ? GL_TRUE : GL_FALSE );
    return D3D_OK;
}

HRESULT STDCALL SDevice::SetTransform( D3DTRANSFORMSTATETYPE state,
                                       const D3DMATRIX *pMatrix )
{
    if ( pMatrix == 0 )
        return D3DERR_INVALIDCALL;
    if ( state == D3DTS_VIEW )
        matView = *pMatrix;
    else if ( state == D3DTS_PROJECTION )
        matProjection = *pMatrix;
    else if ( state >= (D3DTRANSFORMSTATETYPE)256 )
        matWorld = *pMatrix;        // D3DTS_WORLDMATRIX( n ); the engine uses one
    return D3D_OK;
}

// The engine's viewport, placed on the surface.
//
// It works in its own coordinates -- 1024x768 for the menus, whatever a mission
// asks for -- and the device scales that onto the surface. So a viewport the
// engine sets cannot go to GL as it stands: it has to be scaled and offset the
// same way the picture is, or the two disagree and half the frame lands off
// screen. GL also counts y upward where Direct3D counts down, which is the
// other half of this conversion.
void ApplyViewport( const D3DVIEWPORT8 &vp )
{
    int nRectX = 0, nRectY = 0, nRectWidth = 0, nRectHeight = 0;
    Bk1GetPresentRect( &nRectX, &nRectY, &nRectWidth, &nRectHeight );
    int nEngineWidth = 0, nEngineHeight = 0;
    Bk1GetPresentSize( &nEngineWidth, &nEngineHeight );
    if ( nEngineWidth <= 0 || nEngineHeight <= 0 || nRectWidth <= 0 || nRectHeight <= 0 )
        return;

    const double dScaleX = (double)nRectWidth / nEngineWidth;
    const double dScaleY = (double)nRectHeight / nEngineHeight;
    const int nWidth = (int)( vp.Width * dScaleX + 0.5 );
    const int nHeight = (int)( vp.Height * dScaleY + 0.5 );
    const int nX = nRectX + (int)( vp.X * dScaleX + 0.5 );
    // Direct3D measures the top edge from the top; GL measures the bottom edge
    // from the bottom.
    const int nY = nRectY +
        (int)( ( nEngineHeight - (int)vp.Y - (int)vp.Height ) * dScaleY + 0.5 );
    {
        static int nShown = 0;
        if ( nShown < 4 )
        {
            ++nShown;
            __android_log_print( ANDROID_LOG_INFO, "Blitzkrieg.gfx",
                                 "viewport: engine %ux%u at %u,%u -> surface %dx%d at %d,%d "
                                 "(present %dx%d, rect %dx%d at %d,%d)",
                                 vp.Width, vp.Height, vp.X, vp.Y,
                                 nWidth, nHeight, nX, nY,
                                 nEngineWidth, nEngineHeight,
                                 nRectWidth, nRectHeight, nRectX, nRectY );
        }
    }
    glViewport( nX, nY, nWidth, nHeight );
}

HRESULT STDCALL SDevice::SetViewport( const D3DVIEWPORT8 *pViewport )
{
    if ( pViewport == 0 )
        return D3DERR_INVALIDCALL;
    viewport = *pViewport;
    ApplyViewport( viewport );
    glDepthRangef( viewport.MinZ, viewport.MaxZ );
    return D3D_OK;
}

HRESULT STDCALL SDevice::SetRenderState( D3DRENDERSTATETYPE state, DWORD dwValue )
{
    if ( (DWORD)state < 256 )
        renderStates[state] = dwValue;
    return D3D_OK;
}

HRESULT STDCALL SDevice::SetTexture( DWORD nStage, IDirect3DBaseTexture8 *pTexture )
{
    if ( nStage < MAX_STAGES )
        pStageTexture[nStage] = (STexture *)pTexture;
    return D3D_OK;
}

HRESULT STDCALL SDevice::SetTextureStageState( DWORD nStage,
                                               D3DTEXTURESTAGESTATETYPE type,
                                               DWORD dwValue )
{
    if ( nStage >= MAX_STAGES )
        return D3D_OK;
    SStageState &stage = stages[nStage];
    switch ( type )
    {
    case D3DTSS_COLOROP:       stage.dwColorOp = dwValue; break;
    case D3DTSS_COLORARG1:     stage.dwColorArg1 = dwValue; break;
    case D3DTSS_COLORARG2:     stage.dwColorArg2 = dwValue; break;
    case D3DTSS_ALPHAOP:       stage.dwAlphaOp = dwValue; break;
    case D3DTSS_ALPHAARG1:     stage.dwAlphaArg1 = dwValue; break;
    case D3DTSS_ALPHAARG2:     stage.dwAlphaArg2 = dwValue; break;
    case D3DTSS_TEXCOORDINDEX: stage.dwTexCoordIndex = dwValue; break;
    case D3DTSS_ADDRESSU:      stage.dwAddressU = dwValue; break;
    case D3DTSS_ADDRESSV:      stage.dwAddressV = dwValue; break;
    case D3DTSS_MAGFILTER:     stage.dwMagFilter = dwValue; break;
    case D3DTSS_MINFILTER:     stage.dwMinFilter = dwValue; break;
    default: break;
    }
    return D3D_OK;
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
static double GLNowMs()
{
    timespec ts;
    clock_gettime( CLOCK_MONOTONIC, &ts );
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

// Adds its lifetime to the GL account.
struct SGLAccount
{
    double fEnter;
    SGLAccount() : fEnter( GLNowMs() ) {}
    ~SGLAccount() { g_fGLMs += GLNowMs() - fEnter; }
};

void SDevice::ApplyState()
{

    const bool bFresh = !cache.bValid;
    cache.bValid = true;

    // depth
    {
        const bool bTest = renderStates[D3DRS_ZENABLE] != D3DZB_FALSE;
        if ( bFresh || bTest != cache.bDepthTest )
        {
            if ( bTest ) glEnable( GL_DEPTH_TEST ); else glDisable( GL_DEPTH_TEST );
            cache.bDepthTest = bTest;
        }
        if ( bTest )
        {
            const GLenum eFunc = CompareFunc( renderStates[D3DRS_ZFUNC] );
            if ( bFresh || eFunc != cache.eDepthFunc )
            {
                glDepthFunc( eFunc );
                cache.eDepthFunc = eFunc;
            }
        }
        const bool bMask = renderStates[D3DRS_ZWRITEENABLE] != 0;
        if ( bFresh || bMask != cache.bDepthMask )
        {
            glDepthMask( bMask ? GL_TRUE : GL_FALSE );
            cache.bDepthMask = bMask;
        }
    }

    // blending
    {
        const bool bBlend = renderStates[D3DRS_ALPHABLENDENABLE] != 0;
        if ( bFresh || bBlend != cache.bBlend )
        {
            if ( bBlend ) glEnable( GL_BLEND ); else glDisable( GL_BLEND );
            cache.bBlend = bBlend;
        }
        if ( bBlend )
        {
            const GLenum eSrc = BlendFactor( renderStates[D3DRS_SRCBLEND] );
            const GLenum eDst = BlendFactor( renderStates[D3DRS_DESTBLEND] );
            if ( bFresh || eSrc != cache.eSrcBlend || eDst != cache.eDstBlend )
            {
                glBlendFunc( eSrc, eDst );
                cache.eSrcBlend = eSrc;
                cache.eDstBlend = eDst;
            }
        }
    }

    // culling. Direct3D names the winding it discards; GLES names the face.
    {
        const int nCull = ( renderStates[D3DRS_CULLMODE] == D3DCULL_NONE ) ? 0
                        : ( renderStates[D3DRS_CULLMODE] == D3DCULL_CW ) ? 1 : 2;
        if ( bFresh || nCull != cache.nCull )
        {
            if ( nCull == 0 )
                glDisable( GL_CULL_FACE );
            else
            {
                glEnable( GL_CULL_FACE );
                glFrontFace( nCull == 1 ? GL_CCW : GL_CW );
                glCullFace( GL_BACK );
            }
            cache.nCull = nCull;
        }
    }

    // stencil
    {
        const bool bStencil = renderStates[D3DRS_STENCILENABLE] != 0;
        if ( bFresh || bStencil != cache.bStencil )
        {
            if ( bStencil ) glEnable( GL_STENCIL_TEST ); else glDisable( GL_STENCIL_TEST );
            cache.bStencil = bStencil;
        }
        if ( bStencil )
        {
            const GLenum eFunc = CompareFunc( renderStates[D3DRS_STENCILFUNC] );
            const GLint  nRef = (GLint)renderStates[D3DRS_STENCILREF];
            const GLuint nMask = (GLuint)renderStates[D3DRS_STENCILMASK];
            if ( bFresh || eFunc != cache.eStencilFunc || nRef != cache.nStencilRef ||
                 nMask != cache.nStencilMask )
            {
                glStencilFunc( eFunc, nRef, nMask );
                cache.eStencilFunc = eFunc;
                cache.nStencilRef = nRef;
                cache.nStencilMask = nMask;
            }
            const GLenum eFail = StencilOp( renderStates[D3DRS_STENCILFAIL] );
            const GLenum eZFail = StencilOp( renderStates[D3DRS_STENCILZFAIL] );
            const GLenum ePass = StencilOp( renderStates[D3DRS_STENCILPASS] );
            if ( bFresh || eFail != cache.eStencilFail || eZFail != cache.eStencilZFail ||
                 ePass != cache.eStencilPass )
            {
                glStencilOp( eFail, eZFail, ePass );
                cache.eStencilFail = eFail;
                cache.eStencilZFail = eZFail;
                cache.eStencilPass = ePass;
            }
            const GLuint nWrite = (GLuint)renderStates[D3DRS_STENCILWRITEMASK];
            if ( bFresh || nWrite != cache.nStencilWrite )
            {
                glStencilMask( nWrite );
                cache.nStencilWrite = nWrite;
            }
        }
    }

    // the texture stages and their samplers
    if ( bFresh || program.nProgram != cache.nProgram )
    {
        glUseProgram( program.nProgram );
        cache.nProgram = program.nProgram;
    }

    GLint colorOps[2] = { (GLint)stages[0].dwColorOp, (GLint)stages[1].dwColorOp };
    GLint alphaOps[2] = { (GLint)stages[0].dwAlphaOp, (GLint)stages[1].dwAlphaOp };
    GLint colorArgs[4] = { (GLint)stages[0].dwColorArg1, (GLint)stages[0].dwColorArg2,
                           (GLint)stages[1].dwColorArg1, (GLint)stages[1].dwColorArg2 };
    GLint alphaArgs[4] = { (GLint)stages[0].dwAlphaArg1, (GLint)stages[0].dwAlphaArg2,
                           (GLint)stages[1].dwAlphaArg1, (GLint)stages[1].dwAlphaArg2 };
    if ( bFresh || memcmp( colorOps, cache.colorOps, sizeof( colorOps ) ) != 0 )
    {
        glUniform2iv( program.nStageOpUniform, 1, colorOps );
        memcpy( cache.colorOps, colorOps, sizeof( colorOps ) );
    }
    if ( bFresh || memcmp( alphaOps, cache.alphaOps, sizeof( alphaOps ) ) != 0 )
    {
        glUniform2iv( program.nAlphaOpUniform, 1, alphaOps );
        memcpy( cache.alphaOps, alphaOps, sizeof( alphaOps ) );
    }
    if ( bFresh || memcmp( colorArgs, cache.colorArgs, sizeof( colorArgs ) ) != 0 )
    {
        glUniform4iv( program.nStageArgUniform, 1, colorArgs );
        memcpy( cache.colorArgs, colorArgs, sizeof( colorArgs ) );
    }
    if ( bFresh || memcmp( alphaArgs, cache.alphaArgs, sizeof( alphaArgs ) ) != 0 )
    {
        glUniform4iv( program.nAlphaArgUniform, 1, alphaArgs );
        memcpy( cache.alphaArgs, alphaArgs, sizeof( alphaArgs ) );
    }

    const DWORD dwFactor = renderStates[D3DRS_TEXTUREFACTOR];
    const float fFactor[4] = {
        (float)( ( dwFactor >> 16 ) & 0xFF ) / 255.0f,
        (float)( ( dwFactor >> 8 ) & 0xFF ) / 255.0f,
        (float)( dwFactor & 0xFF ) / 255.0f,
        (float)( ( dwFactor >> 24 ) & 0xFF ) / 255.0f };
    if ( bFresh || memcmp( fFactor, cache.fFactor, sizeof( fFactor ) ) != 0 )
    {
        glUniform4f( program.nTextureFactorUniform,
                     fFactor[0], fFactor[1], fFactor[2], fFactor[3] );
        memcpy( cache.fFactor, fFactor, sizeof( fFactor ) );
    }

    const float fAlphaTest[3] = {
        renderStates[D3DRS_ALPHATESTENABLE] ? 1.0f : 0.0f,
        (float)renderStates[D3DRS_ALPHAFUNC],
        (float)renderStates[D3DRS_ALPHAREF] / 255.0f };
    if ( bFresh || memcmp( fAlphaTest, cache.fAlphaTest, sizeof( fAlphaTest ) ) != 0 )
    {
        glUniform3f( program.nAlphaTestUniform,
                     fAlphaTest[0], fAlphaTest[1], fAlphaTest[2] );
        memcpy( cache.fAlphaTest, fAlphaTest, sizeof( fAlphaTest ) );
    }

    for ( int i = 0; i < MAX_TEXTURE_UNITS; ++i )
    {
        // Texture parameters belong to the texture object, not the unit, so
        // they only have to be set when this texture is new to us -- and the
        // active unit only has to move when a call below actually needs it.
        GLuint nTexture = 0;
        if ( pStageTexture[i] != 0 )
        {
            pStageTexture[i]->EnsureUploaded();
            nTexture = pStageTexture[i]->nGLTexture;
        }
        const GLint nWrapS = pStageTexture[i] != 0 ? AddressMode( stages[i].dwAddressU ) : 0;
        const GLint nWrapT = pStageTexture[i] != 0 ? AddressMode( stages[i].dwAddressV ) : 0;
        GLint nMag = 0, nMin = 0;
        if ( pStageTexture[i] != 0 )
        {
            nMag = ( stages[i].dwMagFilter == D3DTEXF_POINT ) ? GL_NEAREST : GL_LINEAR;
            // A single-level texture must not ask for a mip filter.
            const bool bMipped = pStageTexture[i]->levels.size() > 1;
            if ( stages[i].dwMinFilter == D3DTEXF_POINT )
                nMin = bMipped ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST;
            else
                nMin = bMipped ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR;
        }

        const bool bBindChanged = bFresh || nTexture != cache.nTexture[i];
        const bool bParamsChanged = pStageTexture[i] != 0 &&
                                    ( bFresh || bBindChanged ||
                                      nWrapS != cache.nWrapS[i] || nWrapT != cache.nWrapT[i] ||
                                      nMag != cache.nMagFilter[i] || nMin != cache.nMinFilter[i] );
        if ( bBindChanged || bParamsChanged )
        {
            const GLenum eUnit = (GLenum)( GL_TEXTURE0 + i );
            if ( bFresh || eUnit != cache.eActiveUnit )
            {
                glActiveTexture( eUnit );
                cache.eActiveUnit = eUnit;
            }
            if ( bBindChanged )
            {
                glBindTexture( GL_TEXTURE_2D, nTexture );
                cache.nTexture[i] = nTexture;
            }
            if ( bParamsChanged )
            {
                glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, nWrapS );
                glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, nWrapT );
                glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, nMag );
                glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, nMin );
                cache.nWrapS[i] = nWrapS;
                cache.nWrapT[i] = nWrapT;
                cache.nMagFilter[i] = nMag;
                cache.nMinFilter[i] = nMin;
            }
        }
    }
    // The sampler uniforms never change: unit i is always sampler i.
    if ( !cache.bSamplersBound )
    {
        for ( int i = 0; i < MAX_TEXTURE_UNITS; ++i )
            glUniform1i( program.nSamplerUniform[i], i );
        cache.bSamplersBound = true;
    }
    if ( program.nHasTextureUniform >= 0 )
    {
        const GLint nHas[2] = { pStageTexture[0] != 0 ? 1 : 0,
                                pStageTexture[1] != 0 ? 1 : 0 };
        if ( bFresh || nHas[0] != cache.nHasTexture[0] || nHas[1] != cache.nHasTexture[1] )
        {
            glUniform2i( program.nHasTextureUniform, nHas[0], nHas[1] );
            cache.nHasTexture[0] = nHas[0];
            cache.nHasTexture[1] = nHas[1];
        }
    }

    // the transform, and the viewport the pre-transformed path maps against
    D3DMATRIX matWorldView, matCombined;
    MultiplyMatrix( matWorld, matView, &matWorldView );
    MultiplyMatrix( matWorldView, matProjection, &matCombined );
    // Direct3D stores a matrix by rows and multiplies a row vector on the left;
    // GLSL stores one by columns and multiplies a column vector. The matrix the
    // shader needs is the transpose of Direct3D's -- and a transpose stored by
    // columns is the original stored by rows, which is what is already in
    // memory here. So it goes up as it lies, and transposing it as well was
    // undoing exactly the thing that makes it correct: it put the translation
    // into the w column, w then varied per vertex, and every quad came out as a
    // projective wedge.
    {
        // What the draws are actually being handed. A frame that is busy and
        // black is either drawing somewhere off screen or drawing nothing that
        // survives the transform, and these two numbers tell which.
        const int nThisDraw = nDrawInFrame++;
        // Off unless asked for, like the frame dump. This is the tool the next
        // attempt at the black-mission bug starts from, so it stays in the
        // build; it just does not run in anyone's game.
        //   adb shell setprop debug.blitzkrieg.draws 1
        static int nWanted = -1;
        if ( nWanted < 0 )
        {
            char szValue[PROP_VALUE_MAX] = { 0 };
            __system_property_get( "debug.blitzkrieg.draws", szValue );
            nWanted = ( szValue[0] != 0 && szValue[0] != '0' ) ? 1 : 0;
        }
        // A spread of fixed positions, so the world pass is caught as well as the
        // interface one that opens every frame.
        const bool bSample = ( nThisDraw == 0 ) || ( nThisDraw == 20 ) ||
                             ( nThisDraw == 60 ) || ( nThisDraw == 100 ) ||
                             ( nThisDraw == 140 );
        if ( nWanted != 0 && ( nFrameIndex % 300 ) == 0 && bSample )
        {
            // The three separately: a sign that flips in the combined matrix
            // came from one of them, and knowing which is the whole question.
            __android_log_print( ANDROID_LOG_INFO, "Blitzkrieg.gfx",
                "draw %d: combined %.5f %.5f | depth %s func %u write %u | "
                "blend %s %u/%u | alphatest %u ref %u | cull %u | tex0 %s",
                nThisDraw,
                matCombined.m[0][0], matCombined.m[1][1],
                renderStates[D3DRS_ZENABLE] != D3DZB_FALSE ? "on" : "off",
                (unsigned)renderStates[D3DRS_ZFUNC],
                (unsigned)renderStates[D3DRS_ZWRITEENABLE],
                renderStates[D3DRS_ALPHABLENDENABLE] ? "on" : "off",
                (unsigned)renderStates[D3DRS_SRCBLEND],
                (unsigned)renderStates[D3DRS_DESTBLEND],
                (unsigned)renderStates[D3DRS_ALPHATESTENABLE],
                (unsigned)renderStates[D3DRS_ALPHAREF],
                (unsigned)renderStates[D3DRS_CULLMODE],
                pStageTexture[0] != 0 ? "bound" : "none" );
            if ( pStageTexture[0] != 0 )
            {
                // What the bound texture actually holds. Every state matches
                // between a mission that draws and one that is black, so the
                // difference has to be in the data, and a texture that uploaded
                // as nothing would look exactly like this.
                const STexture *pTex = pStageTexture[0];
                __android_log_print( ANDROID_LOG_INFO, "Blitzkrieg.gfx",
                    "    tex0: %ux%u, %u levels, level0 %zu bytes, gl name %u, uploaded %d",
                    pTex->levels.empty() ? 0u : pTex->levels[0].nWidth,
                    pTex->levels.empty() ? 0u : pTex->levels[0].nHeight,
                    (unsigned)pTex->levels.size(),
                    pTex->levels.empty() ? (size_t)0 : pTex->levels[0].data.size(),
                    pTex->nGLTexture, pTex->bUploaded ? 1 : 0 );
            }
        }
    }
    if ( bFresh || memcmp( &matCombined.m[0][0], cache.matCombined, sizeof( cache.matCombined ) ) != 0 )
    {
        glUniformMatrix4fv( program.nTransformUniform, 1, GL_FALSE, &matCombined.m[0][0] );
        memcpy( cache.matCombined, &matCombined.m[0][0], sizeof( cache.matCombined ) );
    }

    // Lighting. The same convention as the combined matrix above: the D3D
    // matrix goes up unchanged and GLSL reads it as its own transpose, which is
    // exactly what turns Direct3D's row-vector multiply into GLSL's column one.
    {
        const bool bLighting = renderStates[D3DRS_LIGHTING] != 0;
        const GLint nLighting = bLighting ? 1 : 0;
        if ( program.nLightingUniform >= 0 &&
             ( bFresh || nLighting != cache.nLighting ) )
        {
            glUniform1i( program.nLightingUniform, nLighting );
            cache.nLighting = nLighting;
        }
        if ( bLighting )
        {
            if ( program.nWorldUniform >= 0 &&
                 ( bFresh || memcmp( &matWorld.m[0][0], cache.matWorld,
                                     sizeof( cache.matWorld ) ) != 0 ) )
            {
                glUniformMatrix4fv( program.nWorldUniform, 1, GL_FALSE, &matWorld.m[0][0] );
                memcpy( cache.matWorld, &matWorld.m[0][0], sizeof( cache.matWorld ) );
            }
            if ( program.nMatDiffuseUniform >= 0 )
                glUniform4f( program.nMatDiffuseUniform, material.Diffuse.r, material.Diffuse.g,
                             material.Diffuse.b, material.Diffuse.a );
            if ( program.nMatAmbientUniform >= 0 )
                glUniform4f( program.nMatAmbientUniform, material.Ambient.r, material.Ambient.g,
                             material.Ambient.b, material.Ambient.a );
            if ( program.nMatEmissiveUniform >= 0 )
                glUniform4f( program.nMatEmissiveUniform, material.Emissive.r, material.Emissive.g,
                             material.Emissive.b, material.Emissive.a );
            if ( program.nGlobalAmbientUniform >= 0 )
            {
                const DWORD dwAmbient = renderStates[D3DRS_AMBIENT];
                glUniform4f( program.nGlobalAmbientUniform,
                             ( ( dwAmbient >> 16 ) & 0xFF ) / 255.0f,
                             ( ( dwAmbient >> 8 ) & 0xFF ) / 255.0f,
                             ( dwAmbient & 0xFF ) / 255.0f,
                             ( ( dwAmbient >> 24 ) & 0xFF ) / 255.0f );
            }

            // Only the directional lights: they are the only kind this engine
            // creates for meshes, and a point light without a position in the
            // shader would be worse than none.
            float fDir[MAX_LIGHTS * 3] = { 0.0f };
            float fDiffuse[MAX_LIGHTS * 4] = { 0.0f };
            float fAmbient[MAX_LIGHTS * 4] = { 0.0f };
            int nCount = 0;
            for ( int i = 0; i < MAX_LIGHTS; ++i )
            {
                if ( !bLightEnabled[i] || lights[i].Type != D3DLIGHT_DIRECTIONAL )
                    continue;
                // D3DLIGHT8::Direction is the way the light travels; N.L wants
                // the way back to it.
                float x = -lights[i].Direction.x;
                float y = -lights[i].Direction.y;
                float z = -lights[i].Direction.z;
                const float fLen = sqrtf( x * x + y * y + z * z );
                if ( fLen > 0.0f ) { x /= fLen; y /= fLen; z /= fLen; }
                fDir[nCount * 3 + 0] = x;
                fDir[nCount * 3 + 1] = y;
                fDir[nCount * 3 + 2] = z;
                fDiffuse[nCount * 4 + 0] = lights[i].Diffuse.r;
                fDiffuse[nCount * 4 + 1] = lights[i].Diffuse.g;
                fDiffuse[nCount * 4 + 2] = lights[i].Diffuse.b;
                fDiffuse[nCount * 4 + 3] = lights[i].Diffuse.a;
                fAmbient[nCount * 4 + 0] = lights[i].Ambient.r;
                fAmbient[nCount * 4 + 1] = lights[i].Ambient.g;
                fAmbient[nCount * 4 + 2] = lights[i].Ambient.b;
                fAmbient[nCount * 4 + 3] = lights[i].Ambient.a;
                ++nCount;
            }
            if ( program.nLightCountUniform >= 0 )
                glUniform1i( program.nLightCountUniform, nCount );
            if ( program.nLightDirUniform >= 0 )
                glUniform3fv( program.nLightDirUniform, MAX_LIGHTS, fDir );
            if ( program.nLightDiffuseUniform >= 0 )
                glUniform4fv( program.nLightDiffuseUniform, MAX_LIGHTS, fDiffuse );
            if ( program.nLightAmbientUniform >= 0 )
                glUniform4fv( program.nLightAmbientUniform, MAX_LIGHTS, fAmbient );
        }
    }

    // The engine draws at its own size and this puts the result on the surface,
    // keeping its shape. Everything downstream is in engine coordinates, so
    // this is the only place that knows about the scale -- along with
    // Bk1SurfaceToEngine, which undoes it for touch.
    {
        // The engine's viewport if it has set one, and the whole frame if it
        // has not -- either way through the same mapping.
        D3DVIEWPORT8 vp = viewport;
        if ( vp.Width == 0 || vp.Height == 0 )
        {
            int nEngineWidth = 0, nEngineHeight = 0;
            Bk1GetPresentSize( &nEngineWidth, &nEngineHeight );
            vp.X = 0;
            vp.Y = 0;
            vp.Width = (DWORD)nEngineWidth;
            vp.Height = (DWORD)nEngineHeight;
        }
        ApplyViewport( vp );
    }

    // Looked up once. glGetUniformLocation is a string lookup and this was
    // doing one on every draw.
    int nClientWidth = 0, nClientHeight = 0;
    Bk1GetPresentSize( &nClientWidth, &nClientHeight );
    const float fViewport[2] = {
        viewport.Width != 0 ? (float)viewport.Width : (float)nClientWidth,
        viewport.Height != 0 ? (float)viewport.Height : (float)nClientHeight };
    if ( bFresh || fViewport[0] != cache.fViewport[0] ||
         fViewport[1] != cache.fViewport[1] )
    {
        static GLint nViewportSize = -2;
        static GLuint nForProgram = 0;
        if ( nViewportSize == -2 || nForProgram != program.nProgram )
        {
            nViewportSize = glGetUniformLocation( program.nProgram, "uViewportSize" );
            nForProgram = program.nProgram;
        }
        glUniform2f( nViewportSize, fViewport[0], fViewport[1] );
        cache.fViewport[0] = fViewport[0];
        cache.fViewport[1] = fViewport[1];
    }
}

// Bring-up diagnostic: the first few draws, with what they were given. A
// picture that is wrong says nothing about which of stride, layout or position
// is wrong; this says it directly.
void ReportDraw( const char *pszWhat, D3DPRIMITIVETYPE type, UINT nPrimitives,
                 DWORD dwFVF, int nStride, const SVertexLayout &layout,
                 const void *pVertices )
{
    static int nReported = 0;
    if ( nReported >= 40 )
        return;
    ++nReported;
    __android_log_print( ANDROID_LOG_INFO, "Blitzkrieg.gfx",
                         "%s type=%d prims=%u fvf=0x%x stride=%d layoutStride=%d "
                         "xyzrhw=%d diffuse=%d texcoords=%d",
                         pszWhat, (int)type, nPrimitives, (unsigned)dwFVF, nStride,
                         layout.nStride, layout.bTransformed ? 1 : 0,
                         layout.bHasDiffuse ? 1 : 0, layout.nTexCoords );
    if ( pVertices != 0 && !layout.bTransformed )
    {
        const unsigned char *p = (const unsigned char *)pVertices;
        const int n = ( nStride > 0 ) ? nStride : layout.nStride;
        for ( int i = 0; i < 3 && n > 0; ++i )
        {
            const float *pf = (const float *)( p + (size_t)i * n );
            __android_log_print( ANDROID_LOG_INFO, "Blitzkrieg.gfx",
                                 "   v%d  %.2f %.2f %.2f", i, pf[0], pf[1], pf[2] );
        }
    }
    if ( pVertices != 0 && layout.bTransformed )
    {
        const unsigned char *p = (const unsigned char *)pVertices;
        const int n = ( nStride > 0 ) ? nStride : layout.nStride;
        for ( int i = 0; i < 3 && n > 0; ++i )
        {
            const float *pf = (const float *)( p + (size_t)i * n );
            __android_log_print( ANDROID_LOG_INFO, "Blitzkrieg.gfx",
                                 "   v%d  %.1f %.1f %.3f rhw %.3f", i,
                                 pf[0], pf[1], pf[2], pf[3] );
        }
    }
}

void SDevice::BindVertexLayout( const SVertexLayout &layout, int nBaseOffset )
{
    const GLsizei nStride = (GLsizei)( nStreamStride != 0 ? nStreamStride : layout.nStride );

    if ( program.nPositionAttrib >= 0 )
    {
        glEnableVertexAttribArray( program.nPositionAttrib );
        glVertexAttribPointer( program.nPositionAttrib, layout.bTransformed ? 4 : 3,
                               GL_FLOAT, GL_FALSE, nStride,
                               (const void *)(size_t)( nBaseOffset + layout.nPositionOffset ) );
    }

    if ( program.nNormalAttrib >= 0 )
    {
        if ( layout.bHasNormal )
        {
            glEnableVertexAttribArray( program.nNormalAttrib );
            glVertexAttribPointer( program.nNormalAttrib, 3, GL_FLOAT, GL_FALSE,
                                   nStride,
                                   (const void *)(size_t)( nBaseOffset + layout.nNormalOffset ) );
        }
        else
        {
            glDisableVertexAttribArray( program.nNormalAttrib );
            glVertexAttrib3f( program.nNormalAttrib, 0.0f, 0.0f, 1.0f );
        }
    }

    // The colours arrive as a packed word, which is B, G, R, A in memory --
    // what a D3DCOLOR is. GLES 3 has no BGRA vertex format, so the bytes go up
    // in memory order and the vertex shader puts the channels right.
    if ( program.nDiffuseAttrib >= 0 )
    {
        if ( layout.bHasDiffuse )
        {
            glEnableVertexAttribArray( program.nDiffuseAttrib );
            glVertexAttribPointer( program.nDiffuseAttrib, 4, GL_UNSIGNED_BYTE,
                                   GL_TRUE, nStride,
                                   (const void *)(size_t)( nBaseOffset + layout.nDiffuseOffset ) );
        }
        else
        {
            glDisableVertexAttribArray( program.nDiffuseAttrib );
            glVertexAttrib4f( program.nDiffuseAttrib, 1.0f, 1.0f, 1.0f, 1.0f );
        }
    }

    if ( program.nSpecularAttrib >= 0 )
    {
        if ( layout.bHasSpecular )
        {
            glEnableVertexAttribArray( program.nSpecularAttrib );
            glVertexAttribPointer( program.nSpecularAttrib, 4, GL_UNSIGNED_BYTE,
                                   GL_TRUE, nStride,
                                   (const void *)(size_t)( nBaseOffset + layout.nSpecularOffset ) );
        }
        else
        {
            glDisableVertexAttribArray( program.nSpecularAttrib );
            glVertexAttrib4f( program.nSpecularAttrib, 0.0f, 0.0f, 0.0f, 0.0f );
        }
    }

    for ( int i = 0; i < MAX_TEXTURE_UNITS; ++i )
    {
        if ( program.nTexCoordAttrib[i] < 0 )
            continue;
        // Which coordinate set a stage reads is its own state, not its index.
        const int nSet = (int)( stages[i].dwTexCoordIndex & 7 );
        if ( nSet < layout.nTexCoords && layout.nTexCoordOffset[nSet] >= 0 )
        {
            glEnableVertexAttribArray( program.nTexCoordAttrib[i] );
            glVertexAttribPointer( program.nTexCoordAttrib[i], 2, GL_FLOAT, GL_FALSE,
                                   nStride,
                                   (const void *)(size_t)( nBaseOffset +
                                                           layout.nTexCoordOffset[nSet] ) );
        }
        else
        {
            glDisableVertexAttribArray( program.nTexCoordAttrib[i] );
            glVertexAttrib2f( program.nTexCoordAttrib[i], 0.0f, 0.0f );
        }
    }

    const GLint nTransformed = layout.bTransformed ? 1 : 0;
    if ( !cache.bValid || nTransformed != cache.nTransformed )
    {
        glUniform1i( program.nTransformedUniform, nTransformed );
        cache.nTransformed = nTransformed;
    }
}

HRESULT STDCALL SDevice::DrawPrimitive( D3DPRIMITIVETYPE type, UINT nStartVertex,
                                        UINT nPrimitiveCount )
{
    SGLAccount account;
    ++g_nDrawCalls;
    EnsureProgram();
    if ( program.nProgram == 0 || pStream == 0 )
        return D3DERR_INVALIDCALL;

    ApplyState();
    glBindVertexArray( nVertexArray );
    pStream->EnsureUploaded();

    const SVertexLayout layout = LayoutFromFVF( dwFVF != 0 ? dwFVF : pStream->dwFVF );
    const int nStride = (int)( nStreamStride != 0 ? nStreamStride : (UINT)layout.nStride );
    BindVertexLayout( layout, (int)nStartVertex * nStride );

    ReportDraw( "array", type, nPrimitiveCount, dwFVF != 0 ? dwFVF : pStream->dwFVF,
                nStride, layout,
                pStream->data.empty() ? 0 : &pStream->data[0] );

    glDrawArrays( PrimitiveMode( type ), 0, (GLsizei)VertexCount( type, nPrimitiveCount ) );
    return D3D_OK;
}

HRESULT STDCALL SDevice::DrawIndexedPrimitive( D3DPRIMITIVETYPE type, UINT,
                                               UINT, UINT nStartIndex,
                                               UINT nPrimitiveCount )
{
    SGLAccount account;
    ++g_nDrawCalls;
    EnsureProgram();
    if ( program.nProgram == 0 || pStream == 0 || pIndices == 0 )
        return D3DERR_INVALIDCALL;

    ApplyState();
    glBindVertexArray( nVertexArray );
    pStream->EnsureUploaded();
    pIndices->EnsureUploaded();

    const SVertexLayout layout = LayoutFromFVF( dwFVF != 0 ? dwFVF : pStream->dwFVF );
    const int nStride = (int)( nStreamStride != 0 ? nStreamStride : (UINT)layout.nStride );
    // Direct3D adds the base vertex index to every index; GLES has no such
    // offset before 3.2, so it is folded into the attribute pointers.
    BindVertexLayout( layout, (int)nBaseVertexIndex * nStride );

    {
        // The triangle as it is actually assembled: the indices the draw uses
        // and the positions they point at. Buffer order says nothing here --
        // an indexed draw can take any six vertices out of thousands.
        static int nTraced = 0;
        if ( nTraced < 3 && !pIndices->data.empty() && !pStream->data.empty() )
        {
            ++nTraced;
            const bool bWide16 = ( pIndices->format != D3DFMT_INDEX32 );
            for ( UINT i = 0; i < nPrimitiveCount * 3 && i < 6; ++i )
            {
                const size_t nAt = (size_t)nStartIndex + i;
                unsigned int nIndex = 0;
                if ( bWide16 )
                {
                    if ( ( nAt + 1 ) * 2 > pIndices->data.size() ) break;
                    nIndex = *(const unsigned short *)( &pIndices->data[nAt * 2] );
                }
                else
                {
                    if ( ( nAt + 1 ) * 4 > pIndices->data.size() ) break;
                    nIndex = *(const unsigned int *)( &pIndices->data[nAt * 4] );
                }
                const size_t nOffset = ( (size_t)nBaseVertexIndex + nIndex ) * nStride;
                if ( nOffset + 12 > pStream->data.size() )
                {
                    __android_log_print( ANDROID_LOG_WARN, "Blitzkrieg.gfx",
                                         "   i%u -> %u PAST THE BUFFER", i, nIndex );
                    continue;
                }
                const float *pf = (const float *)( &pStream->data[nOffset] );
                __android_log_print( ANDROID_LOG_INFO, "Blitzkrieg.gfx",
                                     "   i%u -> %u  %.1f %.1f %.2f", i, nIndex,
                                     pf[0], pf[1], pf[2] );
            }
        }
    }

    ReportDraw( "indexed", type, nPrimitiveCount, dwFVF != 0 ? dwFVF : pStream->dwFVF,
                nStride, layout,
                pStream->data.empty() ? 0 : &pStream->data[0] );

    const bool bWide = ( pIndices->format == D3DFMT_INDEX32 );
    const size_t nIndexSize = bWide ? 4 : 2;
    glDrawElements( PrimitiveMode( type ), (GLsizei)VertexCount( type, nPrimitiveCount ),
                    bWide ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT,
                    (const void *)( (size_t)nStartIndex * nIndexSize ) );
    return D3D_OK;
}

// ---------------------------------------------------------------------------
// The factory
// ---------------------------------------------------------------------------
struct SDirect3D : public IDirect3D8
{
    LONG nRefCount;

    SDirect3D() : nRefCount( 1 ) {}

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

    UINT STDCALL GetAdapterCount() override { return 1; }

    HRESULT STDCALL GetAdapterIdentifier( UINT, DWORD,
                                          D3DADAPTER_IDENTIFIER8 *pIdentifier ) override
    {
        if ( pIdentifier == 0 )
            return D3DERR_INVALIDCALL;
        memset( pIdentifier, 0, sizeof( *pIdentifier ) );
        const char *pszRenderer = (const char *)glGetString( GL_RENDERER );
        snprintf( pIdentifier->Description, sizeof( pIdentifier->Description ), "%s",
                  pszRenderer != 0 ? pszRenderer : "OpenGL ES" );
        snprintf( pIdentifier->Driver, sizeof( pIdentifier->Driver ), "OpenGL ES" );
        return D3D_OK;
    }

    // One mode, which is the surface the activity gave us.
    // The display modes a card would offer.
    //
    // There is no mode to set here: the surface is whatever size Android gave
    // it, and the engine draws its 1024x768 frame onto that. But the options
    // screen is not asking what can be set -- it is asking what to list, and
    // then it looks up the *current* setting's index in that list. The game
    // ships with "1024x768x32" as its GFX.Mode default, so a list holding only
    // the real surface size can never contain the value being looked for; the
    // search returns -1 and CUIOption::ChangeSelection indexes an array at -1.
    // That was a hard crash walking into Options, at a fault address 24 bytes
    // below zero -- one element back from the start.
    //
    // So the port answers the way a PC of that era did, with the standard 4:3
    // ladder the game was written against, plus the surface's own size at the
    // end for anyone who wants it. Every entry is 32-bit: the renderer has one
    // backbuffer format and reporting 16-bit modes it cannot give would be a
    // lie the options screen would then offer to the player.
    struct SMode { UINT nWidth; UINT nHeight; };
    static const SMode *StandardModes( int *pnCount )
    {
        static const SMode modes[] = {
            {  640,  480 }, {  800,  600 }, { 1024,  768 }, { 1152,  864 },
            { 1280,  960 }, { 1280, 1024 }, { 1600, 1200 },
        };
        *pnCount = (int)( sizeof( modes ) / sizeof( modes[0] ) );
        return modes;
    }

    // The surface's size, if it is not already one of the standard ones.
    bool NativeModeIsExtra( UINT *pnWidth, UINT *pnHeight ) const
    {
        int nWidth = 0, nHeight = 0;
        Bk1GetClientSize( &nWidth, &nHeight );
        if ( nWidth < 640 || nHeight < 480 )
            return false;
        int nStandard = 0;
        const SMode *pModes = StandardModes( &nStandard );
        for ( int i = 0; i < nStandard; ++i )
        {
            if ( pModes[i].nWidth == (UINT)nWidth && pModes[i].nHeight == (UINT)nHeight )
                return false;
        }
        *pnWidth = (UINT)nWidth;
        *pnHeight = (UINT)nHeight;
        return true;
    }

    UINT STDCALL GetAdapterModeCount( UINT ) override
    {
        int nStandard = 0;
        StandardModes( &nStandard );
        UINT nWidth = 0, nHeight = 0;
        return (UINT)nStandard + ( NativeModeIsExtra( &nWidth, &nHeight ) ? 1 : 0 );
    }

    HRESULT STDCALL EnumAdapterModes( UINT, UINT nMode, D3DDISPLAYMODE *pMode ) override
    {
        if ( pMode == 0 )
            return D3DERR_INVALIDCALL;
        int nStandard = 0;
        const SMode *pModes = StandardModes( &nStandard );
        pMode->RefreshRate = 60;
        pMode->Format = D3DFMT_X8R8G8B8;
        if ( (int)nMode < nStandard )
        {
            pMode->Width = pModes[nMode].nWidth;
            pMode->Height = pModes[nMode].nHeight;
            return D3D_OK;
        }
        UINT nWidth = 0, nHeight = 0;
        if ( (int)nMode == nStandard && NativeModeIsExtra( &nWidth, &nHeight ) )
        {
            pMode->Width = nWidth;
            pMode->Height = nHeight;
            return D3D_OK;
        }
        return D3DERR_INVALIDCALL;
    }

    HRESULT STDCALL GetAdapterDisplayMode( UINT, D3DDISPLAYMODE *pMode ) override
    {
        if ( pMode == 0 )
            return D3DERR_INVALIDCALL;
        int nWidth = 0, nHeight = 0;
        Bk1GetClientSize( &nWidth, &nHeight );
        pMode->Width = (UINT)nWidth;
        pMode->Height = (UINT)nHeight;
        pMode->RefreshRate = 60;
        pMode->Format = D3DFMT_X8R8G8B8;
        return D3D_OK;
    }

    HRESULT STDCALL CheckDeviceType( UINT, D3DDEVTYPE, D3DFORMAT, D3DFORMAT,
                                     BOOL ) override
    {
        return D3D_OK;
    }

    HRESULT STDCALL CheckDeviceFormat( UINT, D3DDEVTYPE, D3DFORMAT, DWORD,
                                       DWORD, D3DFORMAT checkFormat ) override
    {
        // The formats the texture path can take, compressed or expanded.
        switch ( checkFormat )
        {
        case D3DFMT_A8R8G8B8:
        case D3DFMT_X8R8G8B8:
        case D3DFMT_R8G8B8:
        case D3DFMT_R5G6B5:
        case D3DFMT_A1R5G5B5:
        case D3DFMT_X1R5G5B5:
        case D3DFMT_A4R4G4B4:
        case D3DFMT_X4R4G4B4:
        case D3DFMT_A8:
        case D3DFMT_L8:
        case D3DFMT_DXT1:
        case D3DFMT_DXT2:
        case D3DFMT_DXT3:
        case D3DFMT_DXT4:
        case D3DFMT_DXT5:
        case D3DFMT_D16:
        case D3DFMT_D24S8:
        case D3DFMT_D24X8:
        case D3DFMT_INDEX16:
        case D3DFMT_INDEX32:
            return D3D_OK;
        default:
            return D3DERR_NOTAVAILABLE;
        }
    }

    HRESULT STDCALL CheckDepthStencilMatch( UINT, D3DDEVTYPE, D3DFORMAT, D3DFORMAT,
                                            D3DFORMAT ) override
    {
        return D3D_OK;
    }

    HRESULT STDCALL GetDeviceCaps( UINT, D3DDEVTYPE, D3DCAPS8 *pCaps ) override;

    HRESULT STDCALL CreateDevice( UINT, D3DDEVTYPE, HWND, DWORD,
                                  D3DPRESENT_PARAMETERS *pParameters,
                                  IDirect3DDevice8 **ppDevice ) override
    {
        if ( ppDevice == 0 )
            return D3DERR_INVALIDCALL;
        SDevice *pDevice = new SDevice();
        if ( pParameters != 0 )
        {
            pDevice->present = *pParameters;
            if ( pParameters->BackBufferWidth != 0 && pParameters->BackBufferHeight != 0 )
                Bk1SetPresentSize( (int)pParameters->BackBufferWidth,
                                   (int)pParameters->BackBufferHeight );
        }
        *ppDevice = pDevice;
        return D3D_OK;
    }
};

HRESULT STDCALL SDirect3D::GetDeviceCaps( UINT, D3DDEVTYPE, D3DCAPS8 *pCaps )
{
    if ( pCaps == 0 )
        return D3DERR_INVALIDCALL;
    memset( pCaps, 0, sizeof( *pCaps ) );

    pCaps->DeviceType = D3DDEVTYPE_HAL;
    pCaps->Caps2 = D3DCAPS2_FULLSCREENGAMMA;
    pCaps->DevCaps = D3DDEVCAPS_HWRASTERIZATION | D3DDEVCAPS_HWTRANSFORMANDLIGHT |
                     D3DDEVCAPS_TEXTUREVIDEOMEMORY;
    pCaps->PresentationIntervals = D3DPRESENT_INTERVAL_ONE | D3DPRESENT_INTERVAL_IMMEDIATE;

    // What the engine asks about before choosing a path.
    pCaps->TextureCaps = D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
    pCaps->TextureFilterCaps = D3DPTFILTERCAPS_MINFLINEAR | D3DPTFILTERCAPS_MAGFLINEAR;
    pCaps->TextureOpCaps = D3DTEXOPCAPS_MODULATE | D3DTEXOPCAPS_ADD;
    pCaps->SrcBlendCaps = D3DPBLENDCAPS_SRCALPHA;
    pCaps->DestBlendCaps = D3DPBLENDCAPS_SRCALPHA;
    pCaps->MaxTextureBlendStages = MAX_STAGES;
    pCaps->MaxSimultaneousTextures = MAX_TEXTURE_UNITS;
    pCaps->MaxStreams = 1;

    GLint nMaxTexture = 2048;
    glGetIntegerv( GL_MAX_TEXTURE_SIZE, &nMaxTexture );
    pCaps->MaxTextureWidth = (DWORD)nMaxTexture;
    pCaps->MaxTextureHeight = (DWORD)nMaxTexture;
    pCaps->MaxTextureAspectRatio = (DWORD)nMaxTexture;
    pCaps->MaxPrimitiveCount = 0xFFFF;
    pCaps->MaxVertexIndex = 0xFFFF;
    pCaps->MaxActiveLights = 8;

    // No programmable shaders are claimed, because the engine has none and
    // asking would only make it look for a path that does not exist.
    pCaps->VertexShaderVersion = 0;
    pCaps->PixelShaderVersion = 0;
    return D3D_OK;
}

}   // anonymous namespace

// Outside the anonymous namespace: the resources call this by name, and the
// device it reaches is the file-scope one above.
void ForgetGLState()
{
    if ( g_pLiveDevice != 0 )
        g_pLiveDevice->cache.Invalidate();
}
}   // namespace NBk1D3D

extern "C" IDirect3D8 *Direct3DCreate8( UINT )
{
    return new NBk1D3D::SDirect3D();
}
