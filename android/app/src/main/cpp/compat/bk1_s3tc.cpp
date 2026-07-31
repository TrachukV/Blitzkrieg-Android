// A DXT block compressor, written against the format itself, standing in for
// the S3TC library the original build linked against.
//
// The format: an image is cut into 4x4 blocks. A colour block is eight bytes --
// two RGB565 endpoints and sixteen two-bit indices into a palette derived from
// them. DXT1 blocks are colour only; DXT2/3 prefix eight bytes of four-bit
// alpha; DXT4/5 prefix eight bytes of two alpha endpoints and sixteen
// three-bit indices. DXT2 and DXT4 differ from DXT3 and DXT5 only in that
// their colour is premultiplied, which is the caller's business, not the
// encoder's.
#include "s3tc.h"

#include <math.h>
#include <string.h>

namespace {

int g_nAlphaReference = 0;

struct SColorRGBA
{
    int r, g, b, a;
};

struct SWeights
{
    float fR, fG, fB;
};

// --- 565 quantisation -------------------------------------------------------
inline int To5( int v ) { return ( v * 31 + 127 ) / 255; }
inline int To6( int v ) { return ( v * 63 + 127 ) / 255; }
inline int From5( int v ) { return ( v << 3 ) | ( v >> 2 ); }
inline int From6( int v ) { return ( v << 2 ) | ( v >> 4 ); }

inline unsigned short Pack565( const SColorRGBA &c )
{
    return (unsigned short)( ( To5( c.r ) << 11 ) | ( To6( c.g ) << 5 ) | To5( c.b ) );
}

inline SColorRGBA Unpack565( unsigned short v )
{
    SColorRGBA c;
    c.r = From5( ( v >> 11 ) & 0x1F );
    c.g = From6( ( v >> 5 ) & 0x3F );
    c.b = From5( v & 0x1F );
    c.a = 255;
    return c;
}

inline float Distance( const SColorRGBA &a, const SColorRGBA &b, const SWeights &w )
{
    const float dr = (float)( a.r - b.r );
    const float dg = (float)( a.g - b.g );
    const float db = (float)( a.b - b.b );
    return w.fR * dr * dr + w.fG * dg * dg + w.fB * db * db;
}

// --- endpoint selection -----------------------------------------------------
// The colours of a block lie close to a line through RGB space. The endpoints
// are found by projecting onto the principal axis of the block and taking the
// extremes, which is both cheaper and better than a plain bounding box when
// the block is not axis-aligned.
void FindEndpoints( const SColorRGBA *pColors, int nCount, const SWeights &w,
                    SColorRGBA *pMin, SColorRGBA *pMax )
{
    if ( nCount <= 0 )
    {
        pMin->r = pMin->g = pMin->b = 0;
        pMax->r = pMax->g = pMax->b = 0;
        pMin->a = pMax->a = 255;
        return;
    }

    // centroid
    float fMean[3] = { 0.0f, 0.0f, 0.0f };
    for ( int i = 0; i < nCount; ++i )
    {
        fMean[0] += (float)pColors[i].r;
        fMean[1] += (float)pColors[i].g;
        fMean[2] += (float)pColors[i].b;
    }
    fMean[0] /= (float)nCount;
    fMean[1] /= (float)nCount;
    fMean[2] /= (float)nCount;

    // covariance of the block about its centroid
    float fCov[6] = { 0, 0, 0, 0, 0, 0 };   // xx xy xz yy yz zz
    for ( int i = 0; i < nCount; ++i )
    {
        const float dx = (float)pColors[i].r - fMean[0];
        const float dy = (float)pColors[i].g - fMean[1];
        const float dz = (float)pColors[i].b - fMean[2];
        fCov[0] += dx * dx;
        fCov[1] += dx * dy;
        fCov[2] += dx * dz;
        fCov[3] += dy * dy;
        fCov[4] += dy * dz;
        fCov[5] += dz * dz;
    }

    // dominant eigenvector by power iteration; a handful of steps is plenty
    // for a 4x4 block
    float fAxis[3] = { 1.0f, 1.0f, 1.0f };
    for ( int nIter = 0; nIter < 8; ++nIter )
    {
        const float x = fCov[0] * fAxis[0] + fCov[1] * fAxis[1] + fCov[2] * fAxis[2];
        const float y = fCov[1] * fAxis[0] + fCov[3] * fAxis[1] + fCov[4] * fAxis[2];
        const float z = fCov[2] * fAxis[0] + fCov[4] * fAxis[1] + fCov[5] * fAxis[2];
        const float fLen = sqrtf( x * x + y * y + z * z );
        if ( fLen < 1e-6f )
            break;                          // the block is a single colour
        fAxis[0] = x / fLen;
        fAxis[1] = y / fLen;
        fAxis[2] = z / fLen;
    }

    // extremes along that axis
    float fMinProj = 1e30f, fMaxProj = -1e30f;
    int nMinIdx = 0, nMaxIdx = 0;
    for ( int i = 0; i < nCount; ++i )
    {
        const float fProj = (float)pColors[i].r * fAxis[0] +
                            (float)pColors[i].g * fAxis[1] +
                            (float)pColors[i].b * fAxis[2];
        if ( fProj < fMinProj ) { fMinProj = fProj; nMinIdx = i; }
        if ( fProj > fMaxProj ) { fMaxProj = fProj; nMaxIdx = i; }
    }

    *pMin = pColors[nMinIdx];
    *pMax = pColors[nMaxIdx];
    (void)w;
}

// Builds the four-entry palette of a colour block.
void BuildPalette4( unsigned short n0, unsigned short n1, SColorRGBA *pPalette )
{
    pPalette[0] = Unpack565( n0 );
    pPalette[1] = Unpack565( n1 );
    pPalette[2].r = ( 2 * pPalette[0].r + pPalette[1].r ) / 3;
    pPalette[2].g = ( 2 * pPalette[0].g + pPalette[1].g ) / 3;
    pPalette[2].b = ( 2 * pPalette[0].b + pPalette[1].b ) / 3;
    pPalette[2].a = 255;
    pPalette[3].r = ( pPalette[0].r + 2 * pPalette[1].r ) / 3;
    pPalette[3].g = ( pPalette[0].g + 2 * pPalette[1].g ) / 3;
    pPalette[3].b = ( pPalette[0].b + 2 * pPalette[1].b ) / 3;
    pPalette[3].a = 255;
}

// ...and of a colour-key block, whose fourth entry is transparent.
void BuildPalette3( unsigned short n0, unsigned short n1, SColorRGBA *pPalette )
{
    pPalette[0] = Unpack565( n0 );
    pPalette[1] = Unpack565( n1 );
    pPalette[2].r = ( pPalette[0].r + pPalette[1].r ) / 2;
    pPalette[2].g = ( pPalette[0].g + pPalette[1].g ) / 2;
    pPalette[2].b = ( pPalette[0].b + pPalette[1].b ) / 2;
    pPalette[2].a = 255;
    pPalette[3].r = pPalette[3].g = pPalette[3].b = 0;
    pPalette[3].a = 0;
}

void WriteU16( unsigned char *p, unsigned short v )
{
    p[0] = (unsigned char)( v & 0xFF );
    p[1] = (unsigned char)( v >> 8 );
}

// --- colour block -----------------------------------------------------------
// The block is addressed as a 4x4 grid throughout, because that is how the
// two-bit indices are laid out. An edge block of an image whose dimensions are
// not multiples of four simply has positions that carry no texel, marked by
// pValid; compacting them would put every index in the wrong place.
void EncodeColorBlock( const SColorRGBA *pBlock, const bool *pValid,
                       const bool *pOpaque, bool bColorKey, const SWeights &w,
                       unsigned char *pOut )
{
    // The endpoints are fitted to the opaque texels only: a transparent texel's
    // colour is not reproduced, so letting it pull the line is pure loss.
    SColorRGBA fitted[16];
    int nFitted = 0;
    bool bHasTransparent = false;
    for ( int i = 0; i < 16; ++i )
    {
        if ( !pValid[i] )
            continue;
        if ( bColorKey && !pOpaque[i] )
        {
            bHasTransparent = true;
            continue;
        }
        fitted[nFitted++] = pBlock[i];
    }

    // every texel transparent: fit to whatever colour is there
    if ( nFitted == 0 )
    {
        for ( int i = 0; i < 16; ++i )
        {
            if ( pValid[i] )
                fitted[nFitted++] = pBlock[i];
        }
    }

    SColorRGBA cMin, cMax;
    FindEndpoints( fitted, nFitted, w, &cMin, &cMax );

    unsigned short n0 = Pack565( cMax );
    unsigned short n1 = Pack565( cMin );

    // The mode is chosen by the ordering of the two endpoints: n0 > n1 selects
    // four colours, n0 <= n1 selects three plus transparent.
    if ( bHasTransparent )
    {
        if ( n0 > n1 )
        {
            const unsigned short t = n0;
            n0 = n1;
            n1 = t;
        }
        if ( n0 == n1 && n0 != 0 )
            --n0;                           // keep n0 < n1 so the mode holds
    }
    else
    {
        if ( n0 < n1 )
        {
            const unsigned short t = n0;
            n0 = n1;
            n1 = t;
        }
        if ( n0 == n1 )
        {
            // A single-colour block: nudge the second endpoint down so the
            // four-colour mode is still selected and index 0 reproduces it.
            if ( n1 > 0 )
                --n1;
            else
                n0 = 1;
        }
    }

    SColorRGBA palette[4];
    if ( bHasTransparent )
        BuildPalette3( n0, n1, palette );
    else
        BuildPalette4( n0, n1, palette );

    unsigned int nIndices = 0;
    for ( int i = 0; i < 16; ++i )
    {
        int nBest = 0;
        if ( pValid[i] )
        {
            if ( bHasTransparent && !pOpaque[i] )
            {
                nBest = 3;                  // the transparent entry
            }
            else
            {
                float fBest = 1e30f;
                const int nEntries = bHasTransparent ? 3 : 4;
                for ( int k = 0; k < nEntries; ++k )
                {
                    const float fDist = Distance( pBlock[i], palette[k], w );
                    if ( fDist < fBest )
                    {
                        fBest = fDist;
                        nBest = k;
                    }
                }
            }
        }
        nIndices |= ( (unsigned int)nBest ) << ( i * 2 );
    }

    WriteU16( pOut, n0 );
    WriteU16( pOut + 2, n1 );
    pOut[4] = (unsigned char)( nIndices & 0xFF );
    pOut[5] = (unsigned char)( ( nIndices >> 8 ) & 0xFF );
    pOut[6] = (unsigned char)( ( nIndices >> 16 ) & 0xFF );
    pOut[7] = (unsigned char)( ( nIndices >> 24 ) & 0xFF );
}

// --- explicit alpha, four bits per texel ------------------------------------
void EncodeAlphaExplicit( const SColorRGBA *pBlock, const bool *pValid,
                          unsigned char *pOut )
{
    for ( int i = 0; i < 8; ++i )
        pOut[i] = 0;
    for ( int i = 0; i < 16; ++i )
    {
        const int nAlpha = pValid[i] ? pBlock[i].a : 255;
        // rounding to nearest of the sixteen levels
        const int nQuant = ( nAlpha * 15 + 127 ) / 255;
        pOut[i >> 1] |= (unsigned char)( ( i & 1 ) ? ( nQuant << 4 ) : nQuant );
    }
}

// --- interpolated alpha, two endpoints and three bits per texel --------------
void EncodeAlphaInterpolated( const SColorRGBA *pBlock, const bool *pValid,
                              unsigned char *pOut )
{
    int nMin = 255, nMax = 0;
    int nCount = 0;
    for ( int i = 0; i < 16; ++i )
    {
        if ( !pValid[i] )
            continue;
        ++nCount;
        if ( pBlock[i].a < nMin ) nMin = pBlock[i].a;
        if ( pBlock[i].a > nMax ) nMax = pBlock[i].a;
    }
    if ( nCount == 0 )
    {
        nMin = 0;
        nMax = 255;
    }

    // a0 > a1 selects the eight-value ramp; a0 <= a1 selects six values plus
    // explicit 0 and 255, which is the better fit when the block is flat
    unsigned char a0 = (unsigned char)nMax;
    unsigned char a1 = (unsigned char)nMin;
    if ( a0 == a1 )
    {
        // a flat block: keep the eight-value mode with index 0 exact
        if ( a1 > 0 )
            --a1;
        else
            a0 = 1;
    }

    int alpha[8];
    alpha[0] = a0;
    alpha[1] = a1;
    for ( int k = 1; k <= 6; ++k )
        alpha[k + 1] = ( ( 7 - k ) * a0 + k * a1 ) / 7;

    unsigned long long nIndices = 0;
    for ( int i = 0; i < 16; ++i )
    {
        const int nAlpha = pValid[i] ? pBlock[i].a : 255;
        int nBest = 0;
        int nBestErr = 1 << 30;
        for ( int k = 0; k < 8; ++k )
        {
            const int nErr = ( nAlpha - alpha[k] ) * ( nAlpha - alpha[k] );
            if ( nErr < nBestErr )
            {
                nBestErr = nErr;
                nBest = k;
            }
        }
        nIndices |= ( (unsigned long long)nBest ) << ( i * 3 );
    }

    pOut[0] = a0;
    pOut[1] = a1;
    for ( int i = 0; i < 6; ++i )
        pOut[2 + i] = (unsigned char)( ( nIndices >> ( i * 8 ) ) & 0xFF );
}

// --- source access ----------------------------------------------------------
// The engine describes its buffer as 32-bit with the masks 0x00FF0000 for red,
// 0x0000FF00 for green, 0x000000FF for blue and 0xFF000000 for alpha, which is
// B, G, R, A in memory order on a little-endian machine.
SColorRGBA ReadPixel( const unsigned char *pRow, int x )
{
    const unsigned char *p = pRow + x * 4;
    SColorRGBA c;
    c.b = p[0];
    c.g = p[1];
    c.r = p[2];
    c.a = p[3];
    return c;
}

int BlocksAcross( int n ) { return ( n + 3 ) / 4; }

// --- decoding ---------------------------------------------------------------
inline unsigned short ReadU16( const unsigned char *p )
{
    return (unsigned short)( p[0] | ( p[1] << 8 ) );
}

// Which DXT variant a surface holds, as its FourCC says. 1 for DXT1, 3 for the
// explicit-alpha pair and 5 for the interpolated-alpha pair; DXT2 and DXT4
// differ only in premultiplication, which does not change the bit layout.
int VariantFromFourCC( DWORD dwFourCC )
{
    switch ( dwFourCC )
    {
    case MAKEFOURCC( 'D', 'X', 'T', '1' ): return 1;
    case MAKEFOURCC( 'D', 'X', 'T', '2' ):
    case MAKEFOURCC( 'D', 'X', 'T', '3' ): return 3;
    case MAKEFOURCC( 'D', 'X', 'T', '4' ):
    case MAKEFOURCC( 'D', 'X', 'T', '5' ): return 5;
    default: return 0;
    }
}

void DecodeColorBlock( const unsigned char *pBlock, bool bAllowTransparent,
                       SColorRGBA *pOut )
{
    const unsigned short n0 = ReadU16( pBlock );
    const unsigned short n1 = ReadU16( pBlock + 2 );

    SColorRGBA palette[4];
    // Only DXT1 carries the three-colour mode; the alpha-bearing variants
    // always read four colours whatever the endpoint ordering says.
    if ( bAllowTransparent && n0 <= n1 )
        BuildPalette3( n0, n1, palette );
    else
        BuildPalette4( n0, n1, palette );

    const unsigned int nIndices = (unsigned int)pBlock[4] |
                                  ( (unsigned int)pBlock[5] << 8 ) |
                                  ( (unsigned int)pBlock[6] << 16 ) |
                                  ( (unsigned int)pBlock[7] << 24 );
    for ( int i = 0; i < 16; ++i )
        pOut[i] = palette[( nIndices >> ( i * 2 ) ) & 3];
}

void DecodeAlphaExplicit( const unsigned char *pBlock, SColorRGBA *pOut )
{
    for ( int i = 0; i < 16; ++i )
    {
        const int nNibble = ( i & 1 ) ? ( pBlock[i >> 1] >> 4 ) : ( pBlock[i >> 1] & 0x0F );
        pOut[i].a = ( nNibble << 4 ) | nNibble;   // 4 bits expanded to 8
    }
}

void DecodeAlphaInterpolated( const unsigned char *pBlock, SColorRGBA *pOut )
{
    const int a0 = pBlock[0];
    const int a1 = pBlock[1];

    int alpha[8];
    alpha[0] = a0;
    alpha[1] = a1;
    if ( a0 > a1 )
    {
        for ( int k = 1; k <= 6; ++k )
            alpha[k + 1] = ( ( 7 - k ) * a0 + k * a1 ) / 7;
    }
    else
    {
        for ( int k = 1; k <= 4; ++k )
            alpha[k + 1] = ( ( 5 - k ) * a0 + k * a1 ) / 5;
        alpha[6] = 0;
        alpha[7] = 255;
    }

    unsigned long long nIndices = 0;
    for ( int i = 0; i < 6; ++i )
        nIndices |= ( (unsigned long long)pBlock[2 + i] ) << ( i * 8 );

    for ( int i = 0; i < 16; ++i )
        pOut[i].a = alpha[( nIndices >> ( i * 3 ) ) & 7];
}

}   // anonymous namespace

extern "C" {

void S3TCsetAlphaReference( int nReference )
{
    g_nAlphaReference = nReference;
}

int S3TCgetEncodeSize( const DDSURFACEDESC *pSrc, DWORD dwEncodeType )
{
    if ( pSrc == 0 )
        return 0;
    const int nBlocks = BlocksAcross( (int)pSrc->dwWidth ) *
                        BlocksAcross( (int)pSrc->dwHeight );
    const bool bHasAlphaBlock =
        ( dwEncodeType & ( S3TC_ENCODE_ALPHA_EXPLICIT | S3TC_ENCODE_ALPHA_INTERPOLATED ) ) != 0;
    return nBlocks * ( bHasAlphaBlock ? 16 : 8 );
}

void S3TCencode( const DDSURFACEDESC *pSrc, void * /*pMipMaps*/,
                 DDSURFACEDESC *pDestDesc, void *pDest,
                 DWORD dwEncodeType, const float *pWeights )
{
    if ( pSrc == 0 || pDest == 0 || pSrc->lpSurface == 0 )
        return;

    const int nWidth = (int)pSrc->dwWidth;
    const int nHeight = (int)pSrc->dwHeight;
    const int nPitch = ( pSrc->lPitch != 0 ) ? (int)pSrc->lPitch : nWidth * 4;
    const unsigned char *pSurface = (const unsigned char *)pSrc->lpSurface;

    SWeights w;
    if ( pWeights != 0 && ( pWeights[0] + pWeights[1] + pWeights[2] ) > 0.0f )
    {
        w.fR = pWeights[0];
        w.fG = pWeights[1];
        w.fB = pWeights[2];
    }
    else
    {
        w.fR = w.fG = w.fB = 1.0f;
    }

    const bool bColorKey = ( dwEncodeType & S3TC_ENCODE_RGB_COLOR_KEY ) != 0;
    const bool bAlphaExplicit = ( dwEncodeType & S3TC_ENCODE_ALPHA_EXPLICIT ) != 0;
    const bool bAlphaInterpolated = ( dwEncodeType & S3TC_ENCODE_ALPHA_INTERPOLATED ) != 0;
    const int nBlockBytes = ( bAlphaExplicit || bAlphaInterpolated ) ? 16 : 8;

    unsigned char *pOut = (unsigned char *)pDest;

    for ( int nBlockY = 0; nBlockY < BlocksAcross( nHeight ); ++nBlockY )
    {
        for ( int nBlockX = 0; nBlockX < BlocksAcross( nWidth ); ++nBlockX )
        {
            SColorRGBA block[16];
            bool       valid[16];
            bool       opaque[16];

            // Addressed by grid position, not by a running count: the indices
            // written below are laid out as y * 4 + x, so an edge block must
            // leave the positions it does not cover empty rather than shift
            // the rest into them.
            for ( int i = 0; i < 16; ++i )
            {
                block[i].r = block[i].g = block[i].b = 0;
                block[i].a = 255;
                valid[i] = false;
                opaque[i] = true;
            }

            for ( int y = 0; y < 4; ++y )
            {
                const int nSrcY = nBlockY * 4 + y;
                if ( nSrcY >= nHeight )
                    continue;
                const unsigned char *pRow = pSurface + (size_t)nSrcY * nPitch;
                for ( int x = 0; x < 4; ++x )
                {
                    const int nSrcX = nBlockX * 4 + x;
                    if ( nSrcX >= nWidth )
                        continue;
                    const int nAt = y * 4 + x;
                    block[nAt] = ReadPixel( pRow, nSrcX );
                    valid[nAt] = true;
                    opaque[nAt] = ( block[nAt].a > g_nAlphaReference );
                }
            }

            unsigned char *pBlockOut = pOut;
            if ( bAlphaExplicit )
            {
                EncodeAlphaExplicit( block, valid, pBlockOut );
                pBlockOut += 8;
            }
            else if ( bAlphaInterpolated )
            {
                EncodeAlphaInterpolated( block, valid, pBlockOut );
                pBlockOut += 8;
            }

            EncodeColorBlock( block, valid, opaque, bColorKey, w, pBlockOut );
            pOut += nBlockBytes;
        }
    }

    if ( pDestDesc != 0 )
    {
        pDestDesc->dwWidth = (DWORD)nWidth;
        pDestDesc->dwHeight = (DWORD)nHeight;
        pDestDesc->dwLinearSize = (DWORD)S3TCgetEncodeSize( pSrc, dwEncodeType );
        pDestDesc->dwFlags |= DDSD_WIDTH | DDSD_HEIGHT | DDSD_LINEARSIZE;
    }
}

int S3TCgetDecodeSize( const DDSURFACEDESC *pSrc )
{
    if ( pSrc == 0 )
        return 0;
    return (int)( pSrc->dwWidth * pSrc->dwHeight * 4 );
}

void S3TCdecode( const DDSURFACEDESC *pSrc, DDSURFACEDESC *pDestDesc, void *pDest )
{
    if ( pSrc == 0 || pDest == 0 || pSrc->lpSurface == 0 )
        return;

    const int nWidth = (int)pSrc->dwWidth;
    const int nHeight = (int)pSrc->dwHeight;
    const int nVariant = VariantFromFourCC( pSrc->ddpfPixelFormat.dwFourCC );
    if ( nVariant == 0 )
        return;                             // not a compressed surface

    const int nBlockBytes = ( nVariant == 1 ) ? 8 : 16;
    const unsigned char *pIn = (const unsigned char *)pSrc->lpSurface;
    unsigned char *pOutBase = (unsigned char *)pDest;

    for ( int nBlockY = 0; nBlockY < BlocksAcross( nHeight ); ++nBlockY )
    {
        for ( int nBlockX = 0; nBlockX < BlocksAcross( nWidth ); ++nBlockX )
        {
            SColorRGBA texels[16];
            const unsigned char *pBlock = pIn;

            if ( nVariant == 3 )
            {
                DecodeColorBlock( pBlock + 8, false, texels );
                DecodeAlphaExplicit( pBlock, texels );
            }
            else if ( nVariant == 5 )
            {
                DecodeColorBlock( pBlock + 8, false, texels );
                DecodeAlphaInterpolated( pBlock, texels );
            }
            else
            {
                // DXT1 carries its one-bit alpha in the colour block itself
                DecodeColorBlock( pBlock, true, texels );
            }

            for ( int y = 0; y < 4; ++y )
            {
                const int nDstY = nBlockY * 4 + y;
                if ( nDstY >= nHeight )
                    continue;
                for ( int x = 0; x < 4; ++x )
                {
                    const int nDstX = nBlockX * 4 + x;
                    if ( nDstX >= nWidth )
                        continue;
                    const SColorRGBA &c = texels[y * 4 + x];
                    unsigned char *p = pOutBase + ( (size_t)nDstY * nWidth + nDstX ) * 4;
                    // the same B, G, R, A order the encoder reads
                    p[0] = (unsigned char)c.b;
                    p[1] = (unsigned char)c.g;
                    p[2] = (unsigned char)c.r;
                    p[3] = (unsigned char)c.a;
                }
            }
            pIn += nBlockBytes;
        }
    }

    if ( pDestDesc != 0 )
    {
        pDestDesc->dwWidth = (DWORD)nWidth;
        pDestDesc->dwHeight = (DWORD)nHeight;
        pDestDesc->lPitch = nWidth * 4;
        pDestDesc->dwFlags |= DDSD_WIDTH | DDSD_HEIGHT | DDSD_PITCH;
        pDestDesc->ddpfPixelFormat.dwSize = sizeof( DDPIXELFORMAT );
        pDestDesc->ddpfPixelFormat.dwFlags = DDPF_ALPHAPIXELS | DDPF_RGB;
        pDestDesc->ddpfPixelFormat.dwRGBBitCount = 32;
        pDestDesc->ddpfPixelFormat.dwRBitMask = 0x00FF0000;
        pDestDesc->ddpfPixelFormat.dwGBitMask = 0x0000FF00;
        pDestDesc->ddpfPixelFormat.dwBBitMask = 0x000000FF;
        pDestDesc->ddpfPixelFormat.dwRGBAlphaBitMask = 0xFF000000;
    }
}

}   // extern "C"
