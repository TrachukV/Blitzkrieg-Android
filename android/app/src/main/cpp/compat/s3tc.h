#pragma once
// Stands in for the S3TC texture compression library the original build linked.
//
// Image/ImageProcessor.cpp calls three entry points from it to turn a 32-bit
// image into a DXT1..DXT5 surface. The implementation beside this header is a
// block compressor written against the DXT format itself.
#include "bk1_win32_types.h"
#include "ddraw.h"

// --- encode type, as the engine composes it ---
// The colour half of a block is one of these:
#define S3TC_ENCODE_RGB_FULL             0x00000001  // four-colour blocks
#define S3TC_ENCODE_RGB_COLOR_KEY        0x00000002  // three colours plus transparent
#define S3TC_ENCODE_RGB_ALPHA_COMPARE    0x00000004  // four colours, alpha carried separately
// ...and the alpha half, when there is one:
#define S3TC_ENCODE_ALPHA_EXPLICIT       0x00000010  // four bits per pixel
#define S3TC_ENCODE_ALPHA_INTERPOLATED   0x00000020  // two endpoints, three bits per pixel

#ifdef __cplusplus
extern "C" {
#endif

// Bytes the encoded surface will occupy.
int S3TCgetEncodeSize( const DDSURFACEDESC *pSrc, DWORD dwEncodeType );

// Alpha below this is transparent in the colour-key encoding. Default 0.
void S3TCsetAlphaReference( int nReference );

// Compresses pSrc into pDest. 'pMipMaps' is unused here, as it is in the
// engine's only call. 'pDestDesc' receives the encoded surface's description
// and may be null. 'pWeights' gives the per-channel weights of the error
// metric -- the engine passes luminance weights -- and may be null for equal
// weighting.
void S3TCencode( const DDSURFACEDESC *pSrc, void *pMipMaps,
                 DDSURFACEDESC *pDestDesc, void *pDest,
                 DWORD dwEncodeType, const float *pWeights );

// Bytes the decoded surface will occupy: 32-bit pixels, so width * height * 4.
int S3TCgetDecodeSize( const DDSURFACEDESC *pSrc );

// Expands a compressed surface into 32-bit pixels. The variant is taken from
// the source's FourCC. 'pDestDesc' receives the decoded description and may be
// null.
void S3TCdecode( const DDSURFACEDESC *pSrc, DDSURFACEDESC *pDestDesc, void *pDest );

#ifdef __cplusplus
}
#endif
