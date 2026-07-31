#pragma once
// The Direct3D 8 interfaces the engine drives.
//
// These are the COM shapes GFX/GraphicsEngine.cpp and its neighbours are
// written against. What sits under them is not Direct3D: Blitzkrieg uses the
// API as a fixed-function 2D blitter -- no programmable shaders anywhere in the
// tree, world geometry emitted already in screen space as D3DFVF_XYZRHW, ground
// drawn with depth test and write off -- so the implementation is a sprite and
// quad batcher on OpenGL ES that reproduces the handful of fixed-function
// texture stages the engine actually selects.
//
// Only the methods the engine calls are declared. A vtable it never indexes
// does not need entries, and leaving them out keeps what has to be implemented
// visible.
#include "bk1_win32_types.h"
#include "bk1_com_stream.h"
#include "d3d8types.h"

struct IDirect3D8;
struct IDirect3DDevice8;
struct IDirect3DResource8;
struct IDirect3DBaseTexture8;
struct IDirect3DTexture8;
struct IDirect3DSurface8;
struct IDirect3DVertexBuffer8;
struct IDirect3DIndexBuffer8;

typedef IDirect3D8            *LPDIRECT3D8;
typedef IDirect3DDevice8      *LPDIRECT3DDEVICE8;
typedef IDirect3DTexture8     *LPDIRECT3DTEXTURE8;
typedef IDirect3DSurface8     *LPDIRECT3DSURFACE8;
typedef IDirect3DVertexBuffer8 *LPDIRECT3DVERTEXBUFFER8;
typedef IDirect3DIndexBuffer8 *LPDIRECT3DINDEXBUFFER8;

// ---------------------------------------------------------------------------
// Resources
// ---------------------------------------------------------------------------
struct IDirect3DSurface8 : public IUnknown
{
    virtual HRESULT STDCALL GetDesc( D3DSURFACE_DESC *pDesc ) = 0;
    virtual HRESULT STDCALL LockRect( D3DLOCKED_RECT *pLockedRect, const RECT *pRect,
                                      DWORD dwFlags ) = 0;
    virtual HRESULT STDCALL UnlockRect() = 0;
};

struct IDirect3DBaseTexture8 : public IUnknown
{
    virtual DWORD STDCALL GetLevelCount() = 0;
};

struct IDirect3DTexture8 : public IDirect3DBaseTexture8
{
    virtual HRESULT STDCALL GetLevelDesc( UINT nLevel, D3DSURFACE_DESC *pDesc ) = 0;
    virtual HRESULT STDCALL GetSurfaceLevel( UINT nLevel, IDirect3DSurface8 **ppSurface ) = 0;
    virtual HRESULT STDCALL LockRect( UINT nLevel, D3DLOCKED_RECT *pLockedRect,
                                      const RECT *pRect, DWORD dwFlags ) = 0;
    virtual HRESULT STDCALL UnlockRect( UINT nLevel ) = 0;
    virtual HRESULT STDCALL AddDirtyRect( const RECT *pDirtyRect ) = 0;
};

struct IDirect3DVertexBuffer8 : public IUnknown
{
    virtual HRESULT STDCALL Lock( UINT nOffsetToLock, UINT nSizeToLock,
                                  BYTE **ppbData, DWORD dwFlags ) = 0;
    virtual HRESULT STDCALL Unlock() = 0;
};

struct IDirect3DIndexBuffer8 : public IUnknown
{
    virtual HRESULT STDCALL Lock( UINT nOffsetToLock, UINT nSizeToLock,
                                  BYTE **ppbData, DWORD dwFlags ) = 0;
    virtual HRESULT STDCALL Unlock() = 0;
};

// ---------------------------------------------------------------------------
// The device
// ---------------------------------------------------------------------------
struct IDirect3DDevice8 : public IUnknown
{
    // --- lifetime and presentation ---
    virtual HRESULT STDCALL TestCooperativeLevel() = 0;
    virtual UINT    STDCALL GetAvailableTextureMem() = 0;
    virtual HRESULT STDCALL Reset( D3DPRESENT_PARAMETERS *pPresentationParameters ) = 0;
    virtual HRESULT STDCALL Present( const RECT *pSourceRect, const RECT *pDestRect,
                                     HWND hDestWindowOverride, const void *pDirtyRegion ) = 0;
    virtual HRESULT STDCALL GetFrontBuffer( IDirect3DSurface8 *pDestSurface ) = 0;

    // --- gamma ---
    virtual void STDCALL SetGammaRamp( DWORD dwFlags, const D3DGAMMARAMP *pRamp ) = 0;
    virtual void STDCALL GetGammaRamp( D3DGAMMARAMP *pRamp ) = 0;

    // --- resource creation ---
    virtual HRESULT STDCALL CreateTexture( UINT nWidth, UINT nHeight, UINT nLevels,
                                           DWORD dwUsage, D3DFORMAT format, D3DPOOL pool,
                                           IDirect3DTexture8 **ppTexture ) = 0;
    virtual HRESULT STDCALL CreateVertexBuffer( UINT nLength, DWORD dwUsage, DWORD dwFVF,
                                                D3DPOOL pool,
                                                IDirect3DVertexBuffer8 **ppVertexBuffer ) = 0;
    virtual HRESULT STDCALL CreateIndexBuffer( UINT nLength, DWORD dwUsage, D3DFORMAT format,
                                               D3DPOOL pool,
                                               IDirect3DIndexBuffer8 **ppIndexBuffer ) = 0;
    virtual HRESULT STDCALL CreateDepthStencilSurface( UINT nWidth, UINT nHeight,
                                                       D3DFORMAT format,
                                                       D3DMULTISAMPLE_TYPE multiSample,
                                                       IDirect3DSurface8 **ppSurface ) = 0;
    virtual HRESULT STDCALL CreateImageSurface( UINT nWidth, UINT nHeight, D3DFORMAT format,
                                                IDirect3DSurface8 **ppSurface ) = 0;

    // --- copying ---
    virtual HRESULT STDCALL CopyRects( IDirect3DSurface8 *pSourceSurface,
                                       const RECT *pSourceRectsArray, UINT nRects,
                                       IDirect3DSurface8 *pDestinationSurface,
                                       const POINT *pDestPointsArray ) = 0;
    virtual HRESULT STDCALL UpdateTexture( IDirect3DBaseTexture8 *pSourceTexture,
                                           IDirect3DBaseTexture8 *pDestinationTexture ) = 0;

    // --- render targets ---
    virtual HRESULT STDCALL GetRenderTarget( IDirect3DSurface8 **ppRenderTarget ) = 0;
    virtual HRESULT STDCALL GetDepthStencilSurface( IDirect3DSurface8 **ppZStencilSurface ) = 0;
    virtual HRESULT STDCALL SetRenderTarget( IDirect3DSurface8 *pRenderTarget,
                                             IDirect3DSurface8 *pNewZStencil ) = 0;

    // --- the frame ---
    virtual HRESULT STDCALL BeginScene() = 0;
    virtual HRESULT STDCALL EndScene() = 0;
    virtual HRESULT STDCALL Clear( DWORD nCount, const void *pRects, DWORD dwFlags,
                                   D3DCOLOR color, float fZ, DWORD dwStencil ) = 0;

    // --- fixed-function state ---
    virtual HRESULT STDCALL SetTransform( D3DTRANSFORMSTATETYPE state,
                                          const D3DMATRIX *pMatrix ) = 0;
    virtual HRESULT STDCALL SetViewport( const D3DVIEWPORT8 *pViewport ) = 0;
    virtual HRESULT STDCALL SetMaterial( const D3DMATERIAL8 *pMaterial ) = 0;
    virtual HRESULT STDCALL SetLight( DWORD nIndex, const D3DLIGHT8 *pLight ) = 0;
    virtual HRESULT STDCALL LightEnable( DWORD nIndex, BOOL bEnable ) = 0;
    virtual HRESULT STDCALL SetRenderState( D3DRENDERSTATETYPE state, DWORD dwValue ) = 0;
    virtual HRESULT STDCALL SetTexture( DWORD nStage, IDirect3DBaseTexture8 *pTexture ) = 0;
    virtual HRESULT STDCALL SetTextureStageState( DWORD nStage,
                                                  D3DTEXTURESTAGESTATETYPE type,
                                                  DWORD dwValue ) = 0;

    // --- geometry ---
    virtual HRESULT STDCALL SetStreamSource( UINT nStreamNumber,
                                             IDirect3DVertexBuffer8 *pStreamData,
                                             UINT nStride ) = 0;
    virtual HRESULT STDCALL SetIndices( IDirect3DIndexBuffer8 *pIndexData,
                                        UINT nBaseVertexIndex ) = 0;
    // The engine passes an FVF code here, never a shader handle: there are no
    // programmable shaders in the tree.
    virtual HRESULT STDCALL SetVertexShader( DWORD dwHandle ) = 0;
    virtual HRESULT STDCALL DrawPrimitive( D3DPRIMITIVETYPE type, UINT nStartVertex,
                                           UINT nPrimitiveCount ) = 0;
    virtual HRESULT STDCALL DrawIndexedPrimitive( D3DPRIMITIVETYPE type, UINT nMinIndex,
                                                  UINT nNumVertices, UINT nStartIndex,
                                                  UINT nPrimitiveCount ) = 0;
};

// ---------------------------------------------------------------------------
// The factory
// ---------------------------------------------------------------------------
struct IDirect3D8 : public IUnknown
{
    virtual UINT    STDCALL GetAdapterCount() = 0;
    virtual HRESULT STDCALL GetAdapterIdentifier( UINT nAdapter, DWORD dwFlags,
                                                  D3DADAPTER_IDENTIFIER8 *pIdentifier ) = 0;
    virtual UINT    STDCALL GetAdapterModeCount( UINT nAdapter ) = 0;
    virtual HRESULT STDCALL EnumAdapterModes( UINT nAdapter, UINT nMode,
                                              D3DDISPLAYMODE *pMode ) = 0;
    virtual HRESULT STDCALL GetAdapterDisplayMode( UINT nAdapter, D3DDISPLAYMODE *pMode ) = 0;
    virtual HRESULT STDCALL CheckDeviceType( UINT nAdapter, D3DDEVTYPE checkType,
                                             D3DFORMAT displayFormat,
                                             D3DFORMAT backBufferFormat,
                                             BOOL bWindowed ) = 0;
    virtual HRESULT STDCALL CheckDeviceFormat( UINT nAdapter, D3DDEVTYPE deviceType,
                                               D3DFORMAT adapterFormat, DWORD dwUsage,
                                               DWORD dwResourceType,
                                               D3DFORMAT checkFormat ) = 0;
    virtual HRESULT STDCALL CheckDepthStencilMatch( UINT nAdapter, D3DDEVTYPE deviceType,
                                                    D3DFORMAT adapterFormat,
                                                    D3DFORMAT renderTargetFormat,
                                                    D3DFORMAT depthStencilFormat ) = 0;
    virtual HRESULT STDCALL GetDeviceCaps( UINT nAdapter, D3DDEVTYPE deviceType,
                                           D3DCAPS8 *pCaps ) = 0;
    virtual HRESULT STDCALL CreateDevice( UINT nAdapter, D3DDEVTYPE deviceType,
                                          HWND hFocusWindow, DWORD dwBehaviorFlags,
                                          D3DPRESENT_PARAMETERS *pPresentationParameters,
                                          IDirect3DDevice8 **ppReturnedDeviceInterface ) = 0;
};

// The resource type codes CheckDeviceFormat takes.
#define D3DRTYPE_SURFACE       1
#define D3DRTYPE_VOLUME        2
#define D3DRTYPE_TEXTURE       3
#define D3DRTYPE_VOLUMETEXTURE 4
#define D3DRTYPE_CUBETEXTURE   5
#define D3DRTYPE_VERTEXBUFFER  6
#define D3DRTYPE_INDEXBUFFER   7

#ifdef __cplusplus
extern "C" {
#endif

IDirect3D8 *Direct3DCreate8( UINT nSDKVersion );

#ifdef __cplusplus
}
#endif
