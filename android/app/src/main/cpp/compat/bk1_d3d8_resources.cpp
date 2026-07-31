// Textures, surfaces and buffers for the Direct3D 8 replacement, and the
// program that stands in for its fixed-function texture stages.
//
// The engine works the way a 2D blitter does: it locks a texture level, writes
// pixels into the system copy, unlocks, and expects the result on screen. So
// each resource keeps that system copy and marks itself dirty; the upload
// happens once, at the draw that needs it.
#include "bk1_d3d8_gles.h"
#include "s3tc.h"

#include <stdio.h>
#include <string.h>

namespace NBk1D3D {

// ---------------------------------------------------------------------------
// Vertex layout
// ---------------------------------------------------------------------------
SVertexLayout LayoutFromFVF( DWORD dwFVF )
{
    SVertexLayout layout;
    int nOffset = 0;

    const DWORD dwPosition = dwFVF & D3DFVF_POSITION_MASK;
    layout.nPositionOffset = nOffset;
    if ( dwPosition == D3DFVF_XYZRHW )
    {
        // x, y, z and the reciprocal of w: the vertex is already in screen
        // space, which is how the whole world is drawn here.
        layout.bTransformed = true;
        nOffset += 16;
    }
    else
    {
        nOffset += 12;
        // the blend weights, when the format carries any
        if ( dwPosition == D3DFVF_XYZB1 ) nOffset += 4;
        else if ( dwPosition == D3DFVF_XYZB2 ) nOffset += 8;
        else if ( dwPosition == D3DFVF_XYZB3 ) nOffset += 12;
        else if ( dwPosition == D3DFVF_XYZB4 ) nOffset += 16;
        else if ( dwPosition == D3DFVF_XYZB5 ) nOffset += 20;
    }

    if ( dwFVF & D3DFVF_NORMAL )
    {
        layout.bHasNormal = true;
        layout.nNormalOffset = nOffset;
        nOffset += 12;
    }
    if ( dwFVF & D3DFVF_PSIZE )
        nOffset += 4;
    if ( dwFVF & D3DFVF_DIFFUSE )
    {
        layout.bHasDiffuse = true;
        layout.nDiffuseOffset = nOffset;
        nOffset += 4;
    }
    if ( dwFVF & D3DFVF_SPECULAR )
    {
        layout.bHasSpecular = true;
        layout.nSpecularOffset = nOffset;
        nOffset += 4;
    }

    layout.nTexCoords = (int)( ( dwFVF & D3DFVF_TEXCOUNT_MASK ) >> D3DFVF_TEXCOUNT_SHIFT );
    for ( int i = 0; i < layout.nTexCoords && i < 8; ++i )
    {
        layout.nTexCoordOffset[i] = nOffset;
        nOffset += 8;               // two floats a set, which is all the engine uses
    }

    layout.nStride = nOffset;
    return layout;
}

// ---------------------------------------------------------------------------
// Format helpers
// ---------------------------------------------------------------------------
namespace {

bool IsCompressed( D3DFORMAT format )
{
    return format == D3DFMT_DXT1 || format == D3DFMT_DXT2 || format == D3DFMT_DXT3 ||
           format == D3DFMT_DXT4 || format == D3DFMT_DXT5;
}

int BytesPerPixel( D3DFORMAT format )
{
    switch ( format )
    {
    case D3DFMT_A8R8G8B8:
    case D3DFMT_X8R8G8B8: return 4;
    case D3DFMT_R8G8B8:   return 3;
    case D3DFMT_R5G6B5:
    case D3DFMT_X1R5G5B5:
    case D3DFMT_A1R5G5B5:
    case D3DFMT_A4R4G4B4:
    case D3DFMT_X4R4G4B4: return 2;
    case D3DFMT_A8:
    case D3DFMT_L8:       return 1;
    default:              return 4;
    }
}

int CompressedBytes( D3DFORMAT format, UINT nWidth, UINT nHeight )
{
    const int nBlocks = (int)( ( nWidth + 3 ) / 4 ) * (int)( ( nHeight + 3 ) / 4 );
    return nBlocks * ( format == D3DFMT_DXT1 ? 8 : 16 );
}

// Expands one of the engine's pixel formats into the 8-bit RGBA that GLES
// takes. Direct3D names its channels in the order a 32-bit word reads them,
// which is B, G, R, A in memory, and GLES wants R, G, B, A.
void ExpandToRGBA( D3DFORMAT format, const BYTE *pSrc, int nSrcPitch,
                   UINT nWidth, UINT nHeight, std::vector<BYTE> *pOut )
{
    pOut->resize( (size_t)nWidth * nHeight * 4 );
    BYTE *pDst = pOut->empty() ? 0 : &( *pOut )[0];
    if ( pDst == 0 )
        return;

    for ( UINT y = 0; y < nHeight; ++y )
    {
        const BYTE *pRow = pSrc + (size_t)y * nSrcPitch;
        for ( UINT x = 0; x < nWidth; ++x )
        {
            BYTE r = 0, g = 0, b = 0, a = 255;
            switch ( format )
            {
            case D3DFMT_A8R8G8B8:
            case D3DFMT_X8R8G8B8:
                b = pRow[x * 4 + 0];
                g = pRow[x * 4 + 1];
                r = pRow[x * 4 + 2];
                a = ( format == D3DFMT_A8R8G8B8 ) ? pRow[x * 4 + 3] : 255;
                break;
            case D3DFMT_R8G8B8:
                b = pRow[x * 3 + 0];
                g = pRow[x * 3 + 1];
                r = pRow[x * 3 + 2];
                break;
            case D3DFMT_R5G6B5:
                {
                    const unsigned v = pRow[x * 2] | ( pRow[x * 2 + 1] << 8 );
                    r = (BYTE)( ( ( v >> 11 ) & 0x1F ) * 255 / 31 );
                    g = (BYTE)( ( ( v >> 5 ) & 0x3F ) * 255 / 63 );
                    b = (BYTE)( ( v & 0x1F ) * 255 / 31 );
                }
                break;
            case D3DFMT_A1R5G5B5:
            case D3DFMT_X1R5G5B5:
                {
                    const unsigned v = pRow[x * 2] | ( pRow[x * 2 + 1] << 8 );
                    r = (BYTE)( ( ( v >> 10 ) & 0x1F ) * 255 / 31 );
                    g = (BYTE)( ( ( v >> 5 ) & 0x1F ) * 255 / 31 );
                    b = (BYTE)( ( v & 0x1F ) * 255 / 31 );
                    a = ( format == D3DFMT_A1R5G5B5 && ( v & 0x8000 ) == 0 ) ? 0 : 255;
                }
                break;
            case D3DFMT_A4R4G4B4:
            case D3DFMT_X4R4G4B4:
                {
                    const unsigned v = pRow[x * 2] | ( pRow[x * 2 + 1] << 8 );
                    r = (BYTE)( ( ( v >> 8 ) & 0x0F ) * 17 );
                    g = (BYTE)( ( ( v >> 4 ) & 0x0F ) * 17 );
                    b = (BYTE)( ( v & 0x0F ) * 17 );
                    a = ( format == D3DFMT_A4R4G4B4 ) ? (BYTE)( ( ( v >> 12 ) & 0x0F ) * 17 ) : 255;
                }
                break;
            case D3DFMT_A8:
                r = g = b = 255;
                a = pRow[x];
                break;
            case D3DFMT_L8:
                r = g = b = pRow[x];
                break;
            default:
                break;
            }
            BYTE *p = pDst + ( (size_t)y * nWidth + x ) * 4;
            p[0] = r;
            p[1] = g;
            p[2] = b;
            p[3] = a;
        }
    }
}

// Whether the running device can take DXT directly. Asked once.
bool DeviceTakesDXT()
{
    static int nAnswer = -1;
    if ( nAnswer < 0 )
    {
        nAnswer = 0;
        const char *pszExtensions = (const char *)glGetString( GL_EXTENSIONS );
        if ( pszExtensions != 0 &&
             ( strstr( pszExtensions, "GL_EXT_texture_compression_s3tc" ) != 0 ||
               strstr( pszExtensions, "GL_EXT_texture_compression_dxt1" ) != 0 ) )
            nAnswer = 1;
    }
    return nAnswer != 0;
}

GLenum CompressedGLFormat( D3DFORMAT format )
{
    // The two DXT3 and DXT5 spellings differ only in premultiplication, which
    // is the content's business, not the format's.
    switch ( format )
    {
    case D3DFMT_DXT1: return 0x83F1;    // GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
    case D3DFMT_DXT2:
    case D3DFMT_DXT3: return 0x83F2;    // GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
    case D3DFMT_DXT4:
    case D3DFMT_DXT5: return 0x83F3;    // GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
    default:          return 0;
    }
}

}   // anonymous namespace

// ---------------------------------------------------------------------------
// Surface
// ---------------------------------------------------------------------------
SSurface::SSurface()
    : nRefCount( 1 ), pOwner( 0 ), nLevel( 0 ), nWidth( 0 ), nHeight( 0 ),
      format( D3DFMT_UNKNOWN ) {}

SSurface::~SSurface() {}

HRESULT STDCALL SSurface::QueryInterface( REFIID, void **ppvObject )
{
    if ( ppvObject == 0 )
        return E_INVALIDARG;
    *ppvObject = this;
    ++nRefCount;
    return S_OK;
}

ULONG STDCALL SSurface::AddRef() { return (ULONG)++nRefCount; }

ULONG STDCALL SSurface::Release()
{
    const LONG n = --nRefCount;
    if ( n <= 0 )
        delete this;
    return (ULONG)n;
}

BYTE *SSurface::Pixels()
{
    if ( pOwner != 0 && nLevel < pOwner->levels.size() )
    {
        std::vector<BYTE> &level = pOwner->levels[nLevel].data;
        return level.empty() ? 0 : &level[0];
    }
    return data.empty() ? 0 : &data[0];
}

int SSurface::Pitch() const
{
    if ( pOwner != 0 )
        return pOwner->BytesPerRow( nLevel );
    return (int)nWidth * BytesPerPixel( format );
}

HRESULT STDCALL SSurface::GetDesc( D3DSURFACE_DESC *pDesc )
{
    if ( pDesc == 0 )
        return E_INVALIDARG;
    memset( pDesc, 0, sizeof( *pDesc ) );
    pDesc->Format = format;
    pDesc->Type = D3DRTYPE_SURFACE;
    pDesc->Pool = D3DPOOL_MANAGED;
    pDesc->Width = nWidth;
    pDesc->Height = nHeight;
    pDesc->Size = (UINT)( Pitch() * (int)nHeight );
    return D3D_OK;
}

HRESULT STDCALL SSurface::LockRect( D3DLOCKED_RECT *pLockedRect, const RECT *pRect,
                                    DWORD dwFlags )
{
    if ( pLockedRect == 0 )
        return E_INVALIDARG;
    BYTE *pBits = Pixels();
    if ( pBits == 0 )
        return D3DERR_INVALIDCALL;

    pLockedRect->Pitch = Pitch();
    // A rectangle asks for a window into the surface; the pointer moves to
    // its corner and the pitch stays the whole row, as Direct3D reports it.
    if ( pRect != 0 )
        pBits += (size_t)pRect->top * pLockedRect->Pitch +
                 (size_t)pRect->left * BytesPerPixel( format );
    pLockedRect->pBits = pBits;

    // Anything but a read-only lock is a write the GPU has not seen.
    if ( pOwner != 0 && ( dwFlags & D3DLOCK_READONLY ) == 0 )
        pOwner->levels[nLevel].bDirty = true;
    return D3D_OK;
}

HRESULT STDCALL SSurface::UnlockRect()
{
    if ( pOwner != 0 )
        pOwner->bUploaded = false;
    return D3D_OK;
}

// ---------------------------------------------------------------------------
// Texture
// ---------------------------------------------------------------------------
STexture::STexture()
    : nRefCount( 1 ), nWidth( 0 ), nHeight( 0 ), nLevels( 0 ),
      format( D3DFMT_UNKNOWN ), dwUsage( 0 ), nGLTexture( 0 ), bUploaded( false ) {}

STexture::~STexture()
{
    if ( nGLTexture != 0 )
        glDeleteTextures( 1, &nGLTexture );
}

HRESULT STDCALL STexture::QueryInterface( REFIID, void **ppvObject )
{
    if ( ppvObject == 0 )
        return E_INVALIDARG;
    *ppvObject = this;
    ++nRefCount;
    return S_OK;
}

ULONG STDCALL STexture::AddRef() { return (ULONG)++nRefCount; }

ULONG STDCALL STexture::Release()
{
    const LONG n = --nRefCount;
    if ( n <= 0 )
        delete this;
    return (ULONG)n;
}

DWORD STDCALL STexture::GetLevelCount() { return nLevels; }

int STexture::BytesPerRow( UINT nLevel ) const
{
    if ( nLevel >= levels.size() )
        return 0;
    const STextureLevel &level = levels[nLevel];
    if ( IsCompressed( format ) )
    {
        // A compressed level is addressed by block rows, which is what the
        // engine writes into when it hands over a .dds level.
        return (int)( ( level.nWidth + 3 ) / 4 ) * ( format == D3DFMT_DXT1 ? 8 : 16 );
    }
    return (int)level.nWidth * BytesPerPixel( format );
}

HRESULT STDCALL STexture::GetLevelDesc( UINT nLevel, D3DSURFACE_DESC *pDesc )
{
    if ( pDesc == 0 || nLevel >= levels.size() )
        return E_INVALIDARG;
    memset( pDesc, 0, sizeof( *pDesc ) );
    pDesc->Format = format;
    pDesc->Type = D3DRTYPE_TEXTURE;
    pDesc->Usage = dwUsage;
    pDesc->Pool = D3DPOOL_MANAGED;
    pDesc->Width = levels[nLevel].nWidth;
    pDesc->Height = levels[nLevel].nHeight;
    pDesc->Size = (UINT)levels[nLevel].data.size();
    return D3D_OK;
}

HRESULT STDCALL STexture::GetSurfaceLevel( UINT nLevel, IDirect3DSurface8 **ppSurface )
{
    if ( ppSurface == 0 || nLevel >= levels.size() )
        return E_INVALIDARG;
    SSurface *pSurface = new SSurface();
    pSurface->pOwner = this;
    pSurface->nLevel = nLevel;
    pSurface->nWidth = levels[nLevel].nWidth;
    pSurface->nHeight = levels[nLevel].nHeight;
    pSurface->format = format;
    AddRef();                       // the surface keeps the texture alive
    *ppSurface = pSurface;
    return D3D_OK;
}

HRESULT STDCALL STexture::LockRect( UINT nLevel, D3DLOCKED_RECT *pLockedRect,
                                    const RECT *pRect, DWORD dwFlags )
{
    if ( pLockedRect == 0 || nLevel >= levels.size() )
        return E_INVALIDARG;
    STextureLevel &level = levels[nLevel];
    if ( level.data.empty() )
        return D3DERR_INVALIDCALL;

    pLockedRect->Pitch = BytesPerRow( nLevel );
    BYTE *pBits = &level.data[0];
    if ( pRect != 0 && !IsCompressed( format ) )
        pBits += (size_t)pRect->top * pLockedRect->Pitch +
                 (size_t)pRect->left * BytesPerPixel( format );
    pLockedRect->pBits = pBits;

    if ( ( dwFlags & D3DLOCK_READONLY ) == 0 )
    {
        level.bDirty = true;
        bUploaded = false;
    }
    return D3D_OK;
}

HRESULT STDCALL STexture::UnlockRect( UINT ) { return D3D_OK; }

HRESULT STDCALL STexture::AddDirtyRect( const RECT * )
{
    // The whole level is re-sent; a partial upload would save bandwidth the
    // engine is not short of here, and the bookkeeping would be a place to be
    // wrong.
    bUploaded = false;
    return D3D_OK;
}

void STexture::EnsureUploaded()
{
    if ( bUploaded )
        return;

    if ( nGLTexture == 0 )
        glGenTextures( 1, &nGLTexture );
    glBindTexture( GL_TEXTURE_2D, nGLTexture );
    glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );

    for ( UINT i = 0; i < levels.size(); ++i )
    {
        STextureLevel &level = levels[i];
        if ( level.data.empty() )
            continue;

        if ( IsCompressed( format ) )
        {
            if ( DeviceTakesDXT() )
            {
                glCompressedTexImage2D( GL_TEXTURE_2D, (GLint)i,
                                        CompressedGLFormat( format ),
                                        (GLsizei)level.nWidth, (GLsizei)level.nHeight,
                                        0, (GLsizei)level.data.size(), &level.data[0] );
            }
            else
            {
                // No hardware support: expand through the decoder, which is
                // the same one the image tools use.
                DDSURFACEDESC src;
                memset( &src, 0, sizeof( src ) );
                src.dwSize = sizeof( src );
                src.dwWidth = level.nWidth;
                src.dwHeight = level.nHeight;
                src.lpSurface = &level.data[0];
                src.ddpfPixelFormat.dwFlags = DDPF_FOURCC;
                src.ddpfPixelFormat.dwFourCC = (DWORD)format;

                std::vector<BYTE> expanded( (size_t)S3TCgetDecodeSize( &src ) );
                if ( !expanded.empty() )
                {
                    S3TCdecode( &src, 0, &expanded[0] );
                    // the decoder writes B, G, R, A; GLES wants R, G, B, A
                    for ( size_t p = 0; p + 3 < expanded.size(); p += 4 )
                    {
                        const BYTE b = expanded[p];
                        expanded[p] = expanded[p + 2];
                        expanded[p + 2] = b;
                    }
                    glTexImage2D( GL_TEXTURE_2D, (GLint)i, GL_RGBA,
                                  (GLsizei)level.nWidth, (GLsizei)level.nHeight, 0,
                                  GL_RGBA, GL_UNSIGNED_BYTE, &expanded[0] );
                }
            }
        }
        else
        {
            std::vector<BYTE> rgba;
            ExpandToRGBA( format, &level.data[0], BytesPerRow( i ),
                          level.nWidth, level.nHeight, &rgba );
            if ( !rgba.empty() )
            {
                glTexImage2D( GL_TEXTURE_2D, (GLint)i, GL_RGBA,
                              (GLsizei)level.nWidth, (GLsizei)level.nHeight, 0,
                              GL_RGBA, GL_UNSIGNED_BYTE, &rgba[0] );
            }
        }
        level.bDirty = false;
    }

    // A texture with one level must say so, or sampling with a mip filter
    // reads levels that were never given.
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0 );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL,
                     (GLint)( levels.empty() ? 0 : levels.size() - 1 ) );
    bUploaded = true;
}

// ---------------------------------------------------------------------------
// Vertex buffer
// ---------------------------------------------------------------------------
SVertexBuffer::SVertexBuffer()
    : nRefCount( 1 ), dwFVF( 0 ), nGLBuffer( 0 ), bDirty( true ) {}

SVertexBuffer::~SVertexBuffer()
{
    if ( nGLBuffer != 0 )
        glDeleteBuffers( 1, &nGLBuffer );
}

HRESULT STDCALL SVertexBuffer::QueryInterface( REFIID, void **ppvObject )
{
    if ( ppvObject == 0 )
        return E_INVALIDARG;
    *ppvObject = this;
    ++nRefCount;
    return S_OK;
}

ULONG STDCALL SVertexBuffer::AddRef() { return (ULONG)++nRefCount; }

ULONG STDCALL SVertexBuffer::Release()
{
    const LONG n = --nRefCount;
    if ( n <= 0 )
        delete this;
    return (ULONG)n;
}

HRESULT STDCALL SVertexBuffer::Lock( UINT nOffsetToLock, UINT nSizeToLock,
                                     BYTE **ppbData, DWORD )
{
    if ( ppbData == 0 || nOffsetToLock > data.size() )
        return E_INVALIDARG;
    (void)nSizeToLock;              // the whole buffer is re-sent on unlock
    *ppbData = data.empty() ? 0 : &data[nOffsetToLock];
    return D3D_OK;
}

HRESULT STDCALL SVertexBuffer::Unlock()
{
    bDirty = true;
    return D3D_OK;
}

void SVertexBuffer::EnsureUploaded()
{
    if ( nGLBuffer == 0 )
        glGenBuffers( 1, &nGLBuffer );
    glBindBuffer( GL_ARRAY_BUFFER, nGLBuffer );
    if ( bDirty && !data.empty() )
    {
        glBufferData( GL_ARRAY_BUFFER, (GLsizeiptr)data.size(), &data[0], GL_DYNAMIC_DRAW );
        bDirty = false;
    }
}

// ---------------------------------------------------------------------------
// Index buffer
// ---------------------------------------------------------------------------
SIndexBuffer::SIndexBuffer()
    : nRefCount( 1 ), format( D3DFMT_INDEX16 ), nGLBuffer( 0 ), bDirty( true ) {}

SIndexBuffer::~SIndexBuffer()
{
    if ( nGLBuffer != 0 )
        glDeleteBuffers( 1, &nGLBuffer );
}

HRESULT STDCALL SIndexBuffer::QueryInterface( REFIID, void **ppvObject )
{
    if ( ppvObject == 0 )
        return E_INVALIDARG;
    *ppvObject = this;
    ++nRefCount;
    return S_OK;
}

ULONG STDCALL SIndexBuffer::AddRef() { return (ULONG)++nRefCount; }

ULONG STDCALL SIndexBuffer::Release()
{
    const LONG n = --nRefCount;
    if ( n <= 0 )
        delete this;
    return (ULONG)n;
}

HRESULT STDCALL SIndexBuffer::Lock( UINT nOffsetToLock, UINT nSizeToLock,
                                    BYTE **ppbData, DWORD )
{
    if ( ppbData == 0 || nOffsetToLock > data.size() )
        return E_INVALIDARG;
    (void)nSizeToLock;
    *ppbData = data.empty() ? 0 : &data[nOffsetToLock];
    return D3D_OK;
}

HRESULT STDCALL SIndexBuffer::Unlock()
{
    bDirty = true;
    return D3D_OK;
}

void SIndexBuffer::EnsureUploaded()
{
    if ( nGLBuffer == 0 )
        glGenBuffers( 1, &nGLBuffer );
    glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, nGLBuffer );
    if ( bDirty && !data.empty() )
    {
        glBufferData( GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)data.size(), &data[0],
                      GL_DYNAMIC_DRAW );
        bDirty = false;
    }
}

// ---------------------------------------------------------------------------
// The program
// ---------------------------------------------------------------------------
namespace {

// The vertex stage. Pre-transformed vertices arrive in pixels and are mapped
// straight to clip space; everything else goes through the combined matrix.
const char *VERTEX_SOURCE =
    "#version 300 es\n"
    "precision highp float;\n"
    "in vec4 aPosition;\n"
    "in vec4 aDiffuse;\n"
    "in vec4 aSpecular;\n"
    "in vec2 aTexCoord0;\n"
    "in vec2 aTexCoord1;\n"
    "uniform mat4 uTransform;\n"
    "uniform int  uTransformed;\n"
    "uniform vec2 uViewportSize;\n"
    "out vec4 vDiffuse;\n"
    "out vec4 vSpecular;\n"
    "out vec2 vTexCoord0;\n"
    "out vec2 vTexCoord1;\n"
    "void main()\n"
    "{\n"
    "    if ( uTransformed != 0 )\n"
    "    {\n"
    "        // x and y are pixels from the top left; z is already in [0,1]\n"
    "        float x = aPosition.x / uViewportSize.x * 2.0 - 1.0;\n"
    "        float y = 1.0 - aPosition.y / uViewportSize.y * 2.0;\n"
    "        gl_Position = vec4( x, y, aPosition.z * 2.0 - 1.0, 1.0 );\n"
    "    }\n"
    "    else\n"
    "    {\n"
    "        gl_Position = uTransform * vec4( aPosition.xyz, 1.0 );\n"
    "    }\n"
    "    vDiffuse = aDiffuse.bgra;\n"
    "    vSpecular = aSpecular.bgra;\n"
    "    vTexCoord0 = aTexCoord0;\n"
    "    vTexCoord1 = aTexCoord1;\n"
    "}\n";

// The fragment stage reproduces the fixed-function texture stages. Only the
// five operations the engine selects are implemented, and the arguments are
// resolved the way Direct3D resolves them: TEXTURE, DIFFUSE, CURRENT, TFACTOR
// and SPECULAR, with the complement bit inverting the result.
const char *FRAGMENT_SOURCE =
    "#version 300 es\n"
    "precision mediump float;\n"
    "in vec4 vDiffuse;\n"
    "in vec4 vSpecular;\n"
    "in vec2 vTexCoord0;\n"
    "in vec2 vTexCoord1;\n"
    "uniform sampler2D uTexture0;\n"
    "uniform sampler2D uTexture1;\n"
    "uniform vec4  uTextureFactor;\n"
    "uniform ivec2 uColorOp;\n"     // one per stage
    "uniform ivec4 uColorArg;\n"    // arg1 and arg2 per stage
    "uniform ivec2 uAlphaOp;\n"
    "uniform ivec4 uAlphaArg;\n"
    "uniform vec3  uAlphaTest;\n"   // enabled, function, reference
    "out vec4 oColor;\n"
    "\n"
    "vec4 Sample( int nStage )\n"
    "{\n"
    "    return nStage == 0 ? texture( uTexture0, vTexCoord0 )\n"
    "                       : texture( uTexture1, vTexCoord1 );\n"
    "}\n"
    "\n"
    "vec4 Arg( int nArg, int nStage, vec4 current )\n"
    "{\n"
    "    int nSelect = nArg & 15;\n"
    "    vec4 v;\n"
    "    if ( nSelect == 2 )      v = Sample( nStage );\n"   // D3DTA_TEXTURE
    "    else if ( nSelect == 1 ) v = current;\n"            // D3DTA_CURRENT
    "    else if ( nSelect == 3 ) v = uTextureFactor;\n"     // D3DTA_TFACTOR
    "    else if ( nSelect == 4 ) v = vSpecular;\n"          // D3DTA_SPECULAR
    "    else                     v = vDiffuse;\n"           // D3DTA_DIFFUSE
    "    if ( ( nArg & 16 ) != 0 ) v = vec4( 1.0 ) - v;\n"   // D3DTA_COMPLEMENT
    "    return v;\n"
    "}\n"
    "\n"
    "vec4 Operate( int nOp, vec4 a, vec4 b, vec4 current )\n"
    "{\n"
    "    if ( nOp == 2 ) return a;\n"           // SELECTARG1
    "    if ( nOp == 3 ) return b;\n"           // SELECTARG2
    "    if ( nOp == 4 ) return a * b;\n"       // MODULATE
    "    if ( nOp == 5 ) return a * b * 2.0;\n" // MODULATE2X
    "    if ( nOp == 6 ) return a * b * 4.0;\n" // MODULATE4X
    "    if ( nOp == 7 ) return a + b;\n"       // ADD
    "    return current;\n"                     // DISABLE
    "}\n"
    "\n"
    "void main()\n"
    "{\n"
    "    vec4 current = vDiffuse;\n"
    "    for ( int i = 0; i < 2; ++i )\n"
    "    {\n"
    "        int nColorOp = i == 0 ? uColorOp.x : uColorOp.y;\n"
    "        if ( nColorOp == 1 ) break;\n"     // DISABLE ends the chain
    "        int nA = i == 0 ? uColorArg.x : uColorArg.z;\n"
    "        int nB = i == 0 ? uColorArg.y : uColorArg.w;\n"
    "        vec3 rgb = Operate( nColorOp, Arg( nA, i, current ),\n"
    "                            Arg( nB, i, current ), current ).rgb;\n"
    "\n"
    "        int nAlphaOp = i == 0 ? uAlphaOp.x : uAlphaOp.y;\n"
    "        float a = current.a;\n"
    "        if ( nAlphaOp != 1 )\n"
    "        {\n"
    "            int nAA = i == 0 ? uAlphaArg.x : uAlphaArg.z;\n"
    "            int nAB = i == 0 ? uAlphaArg.y : uAlphaArg.w;\n"
    "            a = Operate( nAlphaOp, Arg( nAA, i, current ),\n"
    "                         Arg( nAB, i, current ), current ).a;\n"
    "        }\n"
    "        current = vec4( rgb, a );\n"
    "    }\n"
    "\n"
    "    // The alpha test, which GLES has no fixed-function equivalent for.\n"
    "    if ( uAlphaTest.x != 0.0 )\n"
    "    {\n"
    "        int nFunc = int( uAlphaTest.y );\n"
    "        float fRef = uAlphaTest.z;\n"
    "        bool bPass = true;\n"
    "        if ( nFunc == 1 )      bPass = false;\n"                    // NEVER
    "        else if ( nFunc == 2 ) bPass = current.a <  fRef;\n"        // LESS
    "        else if ( nFunc == 3 ) bPass = current.a == fRef;\n"        // EQUAL
    "        else if ( nFunc == 4 ) bPass = current.a <= fRef;\n"        // LESSEQUAL
    "        else if ( nFunc == 5 ) bPass = current.a >  fRef;\n"        // GREATER
    "        else if ( nFunc == 6 ) bPass = current.a != fRef;\n"        // NOTEQUAL
    "        else if ( nFunc == 7 ) bPass = current.a >= fRef;\n"        // GREATEREQUAL
    "        if ( !bPass ) discard;\n"
    "    }\n"
    "    oColor = current;\n"
    "}\n";

GLuint CompileStage( GLenum eStage, const char *pszSource )
{
    const GLuint nShader = glCreateShader( eStage );
    glShaderSource( nShader, 1, &pszSource, 0 );
    glCompileShader( nShader );

    GLint nCompiled = 0;
    glGetShaderiv( nShader, GL_COMPILE_STATUS, &nCompiled );
    if ( nCompiled == 0 )
    {
        char szLog[2048];
        GLsizei nLength = 0;
        glGetShaderInfoLog( nShader, sizeof( szLog ), &nLength, szLog );
        OutputDebugStringA( "Direct3D replacement: shader failed to compile" );
        OutputDebugStringA( szLog );
        glDeleteShader( nShader );
        return 0;
    }
    return nShader;
}

}   // anonymous namespace

bool BuildProgram( SProgram *pProgram )
{
    if ( pProgram == 0 )
        return false;

    const GLuint nVertex = CompileStage( GL_VERTEX_SHADER, VERTEX_SOURCE );
    if ( nVertex == 0 )
        return false;
    const GLuint nFragment = CompileStage( GL_FRAGMENT_SHADER, FRAGMENT_SOURCE );
    if ( nFragment == 0 )
    {
        glDeleteShader( nVertex );
        return false;
    }

    const GLuint nProgram = glCreateProgram();
    glAttachShader( nProgram, nVertex );
    glAttachShader( nProgram, nFragment );
    glLinkProgram( nProgram );
    glDeleteShader( nVertex );
    glDeleteShader( nFragment );

    GLint nLinked = 0;
    glGetProgramiv( nProgram, GL_LINK_STATUS, &nLinked );
    if ( nLinked == 0 )
    {
        char szLog[2048];
        GLsizei nLength = 0;
        glGetProgramInfoLog( nProgram, sizeof( szLog ), &nLength, szLog );
        OutputDebugStringA( "Direct3D replacement: program failed to link" );
        OutputDebugStringA( szLog );
        glDeleteProgram( nProgram );
        return false;
    }

    pProgram->nProgram = nProgram;
    pProgram->nPositionAttrib = glGetAttribLocation( nProgram, "aPosition" );
    pProgram->nDiffuseAttrib = glGetAttribLocation( nProgram, "aDiffuse" );
    pProgram->nSpecularAttrib = glGetAttribLocation( nProgram, "aSpecular" );
    pProgram->nTexCoordAttrib[0] = glGetAttribLocation( nProgram, "aTexCoord0" );
    pProgram->nTexCoordAttrib[1] = glGetAttribLocation( nProgram, "aTexCoord1" );

    pProgram->nTransformUniform = glGetUniformLocation( nProgram, "uTransform" );
    pProgram->nTransformedUniform = glGetUniformLocation( nProgram, "uTransformed" );
    pProgram->nTextureFactorUniform = glGetUniformLocation( nProgram, "uTextureFactor" );
    pProgram->nSamplerUniform[0] = glGetUniformLocation( nProgram, "uTexture0" );
    pProgram->nSamplerUniform[1] = glGetUniformLocation( nProgram, "uTexture1" );
    pProgram->nStageOpUniform = glGetUniformLocation( nProgram, "uColorOp" );
    pProgram->nStageArgUniform = glGetUniformLocation( nProgram, "uColorArg" );
    pProgram->nAlphaOpUniform = glGetUniformLocation( nProgram, "uAlphaOp" );
    pProgram->nAlphaArgUniform = glGetUniformLocation( nProgram, "uAlphaArg" );
    pProgram->nAlphaTestUniform = glGetUniformLocation( nProgram, "uAlphaTest" );
    return true;
}

}   // namespace NBk1D3D
