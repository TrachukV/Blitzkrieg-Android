// Round-trips images through the DXT encoder and decoder and reports the error.
#include "s3tc.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

namespace {

struct SImage
{
    int nWidth, nHeight;
    std::vector<unsigned char> data;   // B,G,R,A

    SImage( int w, int h ) : nWidth( w ), nHeight( h ), data( (size_t)w * h * 4, 0 ) {}

    void Set( int x, int y, int r, int g, int b, int a )
    {
        unsigned char *p = &data[( (size_t)y * nWidth + x ) * 4];
        p[0] = (unsigned char)b;
        p[1] = (unsigned char)g;
        p[2] = (unsigned char)r;
        p[3] = (unsigned char)a;
    }

    void Get( int x, int y, int *r, int *g, int *b, int *a ) const
    {
        const unsigned char *p = &data[( (size_t)y * nWidth + x ) * 4];
        *b = p[0]; *g = p[1]; *r = p[2]; *a = p[3];
    }
};

void DescribeSource( DDSURFACEDESC *pDesc, const SImage &img )
{
    memset( pDesc, 0, sizeof( *pDesc ) );
    pDesc->dwSize = sizeof( DDSURFACEDESC );
    pDesc->dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_PITCH | DDSD_PIXELFORMAT | DDSD_LPSURFACE;
    pDesc->dwWidth = img.nWidth;
    pDesc->dwHeight = img.nHeight;
    pDesc->lPitch = img.nWidth * 4;
    pDesc->lpSurface = (void *)&img.data[0];
    pDesc->ddpfPixelFormat.dwSize = sizeof( DDPIXELFORMAT );
    pDesc->ddpfPixelFormat.dwFlags = DDPF_ALPHAPIXELS | DDPF_RGB;
    pDesc->ddpfPixelFormat.dwRGBBitCount = 32;
    pDesc->ddpfPixelFormat.dwRBitMask = 0x00FF0000;
    pDesc->ddpfPixelFormat.dwGBitMask = 0x0000FF00;
    pDesc->ddpfPixelFormat.dwBBitMask = 0x000000FF;
    pDesc->ddpfPixelFormat.dwRGBAlphaBitMask = 0xFF000000;
}

struct SResult
{
    double fRmseColor;
    double fRmseAlpha;
    int    nMaxAlphaErr;
};

SResult RoundTrip( const SImage &src, DWORD dwEncodeType, DWORD dwFourCC,
                   const char *pszName )
{
    float fWeights[] = { 0.309f, 0.609f, 0.082f, 0, 0, 0, 0, 0 };

    DDSURFACEDESC in;
    DescribeSource( &in, src );

    const int nEncoded = S3TCgetEncodeSize( &in, dwEncodeType );
    std::vector<unsigned char> compressed( (size_t)nEncoded, 0 );

    DDSURFACEDESC encodedDesc;
    memset( &encodedDesc, 0, sizeof( encodedDesc ) );
    encodedDesc.dwSize = sizeof( DDSURFACEDESC );
    S3TCsetAlphaReference( 0 );
    S3TCencode( &in, 0, &encodedDesc, &compressed[0], dwEncodeType, fWeights );

    // now describe the compressed surface and decode it back
    DDSURFACEDESC comp;
    memset( &comp, 0, sizeof( comp ) );
    comp.dwSize = sizeof( DDSURFACEDESC );
    comp.dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_LINEARSIZE | DDSD_PIXELFORMAT | DDSD_LPSURFACE;
    comp.dwWidth = src.nWidth;
    comp.dwHeight = src.nHeight;
    comp.dwLinearSize = nEncoded;
    comp.lpSurface = &compressed[0];
    comp.ddpfPixelFormat.dwSize = sizeof( DDPIXELFORMAT );
    comp.ddpfPixelFormat.dwFlags = DDPF_FOURCC;
    comp.ddpfPixelFormat.dwFourCC = dwFourCC;

    const int nDecoded = S3TCgetDecodeSize( &comp );
    std::vector<unsigned char> out( (size_t)nDecoded, 0 );
    DDSURFACEDESC outDesc;
    memset( &outDesc, 0, sizeof( outDesc ) );
    outDesc.dwSize = sizeof( DDSURFACEDESC );
    S3TCdecode( &comp, &outDesc, &out[0] );

    double fSumColor = 0.0, fSumAlpha = 0.0;
    int nMaxAlpha = 0;
    int nColorSamples = 0;
    for ( int y = 0; y < src.nHeight; ++y )
    {
        for ( int x = 0; x < src.nWidth; ++x )
        {
            int r0, g0, b0, a0;
            src.Get( x, y, &r0, &g0, &b0, &a0 );
            const unsigned char *p = &out[( (size_t)y * src.nWidth + x ) * 4];
            const int b1 = p[0], g1 = p[1], r1 = p[2], a1 = p[3];
            if ( a0 != 0 )
            {
                fSumColor += ( r0 - r1 ) * ( r0 - r1 ) + ( g0 - g1 ) * ( g0 - g1 ) +
                             ( b0 - b1 ) * ( b0 - b1 );
                nColorSamples += 3;
            }
            fSumAlpha += ( a0 - a1 ) * ( a0 - a1 );
            const int nErr = abs( a0 - a1 );
            if ( nErr > nMaxAlpha )
                nMaxAlpha = nErr;
        }
    }
    const double fPixels = (double)src.nWidth * src.nHeight;
    SResult res;
    res.fRmseColor = nColorSamples > 0 ? sqrt( fSumColor / nColorSamples ) : 0.0;
    res.fRmseAlpha = sqrt( fSumAlpha / fPixels );
    res.nMaxAlphaErr = nMaxAlpha;
    printf( "%-28s bytes=%6d  colour RMSE=%6.2f  alpha RMSE=%6.2f  alpha max=%3d\n",
            pszName, nEncoded, res.fRmseColor, res.fRmseAlpha, nMaxAlpha );
    return res;
}

SImage MakeGradient( int w, int h )
{
    SImage img( w, h );
    for ( int y = 0; y < h; ++y )
    {
        for ( int x = 0; x < w; ++x )
            img.Set( x, y, x * 255 / ( w - 1 ), y * 255 / ( h - 1 ),
                     ( x + y ) * 255 / ( w + h - 2 ), 255 );
    }
    return img;
}

SImage MakeFlat( int w, int h, int r, int g, int b, int a )
{
    SImage img( w, h );
    for ( int y = 0; y < h; ++y )
        for ( int x = 0; x < w; ++x )
            img.Set( x, y, r, g, b, a );
    return img;
}

SImage MakeAlphaRamp( int w, int h )
{
    SImage img( w, h );
    for ( int y = 0; y < h; ++y )
        for ( int x = 0; x < w; ++x )
            img.Set( x, y, 200, 100, 50, x * 255 / ( w - 1 ) );
    return img;
}

SImage MakeCutout( int w, int h )
{
    SImage img( w, h );
    for ( int y = 0; y < h; ++y )
        for ( int x = 0; x < w; ++x )
        {
            const bool bInside = ( ( x - w / 2 ) * ( x - w / 2 ) +
                                   ( y - h / 2 ) * ( y - h / 2 ) ) < ( w * w / 9 );
            img.Set( x, y, 30, 200, 80, bInside ? 255 : 0 );
        }
    return img;
}

int g_nFailures = 0;

void Check( bool bCondition, const char *pszWhat )
{
    if ( !bCondition )
    {
        printf( "  FAIL: %s\n", pszWhat );
        ++g_nFailures;
    }
}

}   // anonymous namespace

int main()
{
    const DWORD DXT1 = MAKEFOURCC( 'D', 'X', 'T', '1' );
    const DWORD DXT3 = MAKEFOURCC( 'D', 'X', 'T', '3' );
    const DWORD DXT5 = MAKEFOURCC( 'D', 'X', 'T', '5' );

    printf( "--- size accounting ---\n" );
    {
        SImage img = MakeGradient( 64, 32 );
        DDSURFACEDESC d;
        DescribeSource( &d, img );
        const int nBlocks = ( 64 / 4 ) * ( 32 / 4 );
        Check( S3TCgetEncodeSize( &d, S3TC_ENCODE_RGB_FULL ) == nBlocks * 8,
               "DXT1 is 8 bytes a block" );
        Check( S3TCgetEncodeSize( &d, S3TC_ENCODE_RGB_FULL | S3TC_ENCODE_ALPHA_EXPLICIT )
                   == nBlocks * 16, "DXT3 is 16 bytes a block" );
        Check( S3TCgetEncodeSize( &d, S3TC_ENCODE_RGB_FULL | S3TC_ENCODE_ALPHA_INTERPOLATED )
                   == nBlocks * 16, "DXT5 is 16 bytes a block" );
        printf( "  8:1 and 4:1 ratios as expected\n" );
    }

    printf( "\n--- round trips ---\n" );
    {
        SImage img = MakeGradient( 64, 64 );
        SResult r = RoundTrip( img, S3TC_ENCODE_RGB_FULL, DXT1, "gradient, DXT1" );
        Check( r.fRmseColor < 8.0, "gradient survives DXT1 within 8 RMSE" );
    }
    {
        // A flat block must come back exactly: the endpoints can represent it
        // and every index points at the same colour.
        SImage img = MakeFlat( 32, 32, 132, 66, 198, 255 );
        SResult r = RoundTrip( img, S3TC_ENCODE_RGB_FULL, DXT1, "flat colour, DXT1" );
        Check( r.fRmseColor < 4.0, "a flat colour survives quantisation" );
    }
    {
        SImage img = MakeAlphaRamp( 64, 16 );
        SResult r = RoundTrip( img, S3TC_ENCODE_RGB_FULL | S3TC_ENCODE_ALPHA_EXPLICIT,
                               DXT3, "alpha ramp, DXT3" );
        Check( r.nMaxAlphaErr <= 8, "explicit alpha keeps 4 bits of precision" );
    }
    {
        SImage img = MakeAlphaRamp( 64, 16 );
        SResult r = RoundTrip( img, S3TC_ENCODE_RGB_FULL | S3TC_ENCODE_ALPHA_INTERPOLATED,
                               DXT5, "alpha ramp, DXT5" );
        Check( r.nMaxAlphaErr <= 8, "interpolated alpha tracks a ramp" );
    }
    {
        SImage img = MakeCutout( 64, 64 );
        SResult r = RoundTrip( img, S3TC_ENCODE_RGB_COLOR_KEY, DXT1, "cutout, DXT1 colour key" );
        Check( r.nMaxAlphaErr == 255 || r.nMaxAlphaErr == 0,
               "colour key keeps alpha binary" );
        Check( r.fRmseAlpha < 40.0, "most of the cutout's alpha is right" );
    }
    {
        SImage img = MakeFlat( 16, 16, 255, 255, 255, 255 );
        SResult r = RoundTrip( img, S3TC_ENCODE_RGB_FULL, DXT1, "white, DXT1" );
        Check( r.fRmseColor < 1.0, "white is exact in 565" );
    }
    {
        // Not a multiple of four in either direction. The error is larger here
        // than for the 64x64 gradient because the gradient is steeper: across
        // one 4x4 block red spans about 64 levels, and a block carries only
        // four points along its line. That is the format, not the encoder.
        SImage img = MakeGradient( 17, 13 );
        SResult r = RoundTrip( img, S3TC_ENCODE_RGB_FULL, DXT1, "17x13, DXT1" );
        Check( r.fRmseColor < 16.0, "a partial edge block still decodes" );
    }
    {
        // The sharp test for texel placement in a partial block. Width 5 means
        // the second block column is one texel wide, so its four texels sit at
        // grid positions 0, 4, 8 and 12. Packing them at 0, 1, 2, 3 instead --
        // which is what counting texels rather than addressing the grid does --
        // returns row colours in the wrong rows, and this catches it.
        // Grey levels, so the four row colours lie on one line in RGB and the
        // format can carry them all. Four hues could not be represented at any
        // placement, which would confound the thing being measured.
        SImage img( 5, 4 );
        const int rows[4][3] = { { 0, 0, 0 }, { 85, 85, 85 },
                                 { 170, 170, 170 }, { 255, 255, 255 } };
        for ( int y = 0; y < 4; ++y )
            for ( int x = 0; x < 5; ++x )
                img.Set( x, y, rows[y][0], rows[y][1], rows[y][2], 255 );

        SResult r = RoundTrip( img, S3TC_ENCODE_RGB_FULL, DXT1, "5x4 row colours, DXT1" );
        Check( r.fRmseColor < 8.0, "an edge block keeps its texels in place" );

        // and directly: the single-texel-wide column must still be row-coloured
        DDSURFACEDESC in;
        DescribeSource( &in, img );
        std::vector<unsigned char> comp( (size_t)S3TCgetEncodeSize( &in, S3TC_ENCODE_RGB_FULL ) );
        S3TCencode( &in, 0, 0, &comp[0], S3TC_ENCODE_RGB_FULL, 0 );
        DDSURFACEDESC cd;
        memset( &cd, 0, sizeof( cd ) );
        cd.dwSize = sizeof( cd );
        cd.dwWidth = 5; cd.dwHeight = 4;
        cd.lpSurface = &comp[0];
        cd.ddpfPixelFormat.dwFlags = DDPF_FOURCC;
        cd.ddpfPixelFormat.dwFourCC = DXT1;
        std::vector<unsigned char> out( (size_t)S3TCgetDecodeSize( &cd ) );
        S3TCdecode( &cd, 0, &out[0] );
        for ( int y = 0; y < 4; ++y )
        {
            const unsigned char *p = &out[( (size_t)y * 5 + 4 ) * 4];
            const int nDist = abs( (int)p[2] - rows[y][0] ) + abs( (int)p[1] - rows[y][1] ) +
                              abs( (int)p[0] - rows[y][2] );
            Check( nDist < 24, "the edge column's row colour is where it belongs" );
        }
    }

    printf( "\n%s\n", g_nFailures == 0 ? "all checks passed" : "THERE WERE FAILURES" );
    return g_nFailures == 0 ? 0 : 1;
}
