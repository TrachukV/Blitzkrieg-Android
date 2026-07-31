#pragma once
// The video player interface Scene/BinkVideoPlayer.cpp is written against.
//
// Bink is RAD Game Tools' proprietary codec, and there is no decoder for it
// here. Writing one is not a matter of effort -- the format is not ours to
// implement -- so the port takes the route real ports take: the thirteen .bik
// files in the game's data are transcoded once, at packaging time, into a
// format Android decodes natively, and this interface is backed by the
// platform decoder rather than by Bink.
//
// The engine keeps its own player unchanged: it asks for frames, copies them
// into its textures in one of the pixel formats below, and drives timing and
// seeking itself. Only what sits under these calls differs.
//
// The frame decoding is wired to Android's MediaCodec through
// Bk1BinkSetDecoderBackend; until a backend is installed, opening a file
// fails and the engine's own "no video" path runs, which is what it does on a
// machine with a missing movie.
#include "bk1_win32_types.h"

// Bink's own scalar names, which the engine uses at its call sites.
typedef signed char        s8;
typedef unsigned char      u8;
typedef short              s16;
typedef unsigned short     u16;
typedef int                s32;
typedef unsigned int       u32;

// --- open flags ---
#define BINKFROMMEMORY      0x04000000
#define BINKALPHA           0x00100000
#define BINKNOSKIP          0x00080000
#define BINKPRELOADALL      0x00002000

// --- surface formats BinkCopyToBufferRect writes ---
#define BINKSURFACE8P       0
#define BINKSURFACE24       1
#define BINKSURFACE24R      2
#define BINKSURFACE32       3
#define BINKSURFACE32R      4
#define BINKSURFACE32A      5
#define BINKSURFACE32RA     6
#define BINKSURFACE4444     7
#define BINKSURFACE5551     8
#define BINKSURFACE555      9
#define BINKSURFACE565      10
#define BINKSURFACE655      11
#define BINKSURFACE664      12
#define BINKSURFACEMASK     15

#define BINKCOPYALL         0x80000000
#define BINKSURFACEFAST     0x00000000

#define BINKMAXDIRTYRECTS   8

typedef struct BINKRECT {
    int Left, Top, Width, Height;
} BINKRECT;

// What the engine reads from the handle: the frame size it sizes its textures
// to, and the frame counter it drives playback with.
typedef struct BINK {
    unsigned int Width;
    unsigned int Height;
    unsigned int Frames;
    unsigned int FrameNum;
    unsigned int LastFrameNum;
    unsigned int FrameRate;
    unsigned int FrameRateDiv;
    unsigned int ReadError;
    unsigned int OpenFlags;
    unsigned int NumRects;
    BINKRECT     FrameRects[BINKMAXDIRTYRECTS];
    void        *pInternal;      // the platform decoder's own state
} BINK, *HBINK;

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
// The decoder backend, installed by the Android layer
// ---------------------------------------------------------------------------
// Blitzkrieg names its movies "<name>.bik"; the packaged application carries
// the transcoded ones. 'Resolve' maps the engine's name to whatever the
// backend can open, and the rest is per-file decoding.
typedef struct SBk1VideoBackend {
    // Opens a movie by the name the engine asked for, or returns null.
    void *( *Open )( const char *pszName );
    void  ( *Close )( void *pMovie );
    // Frame geometry and count, so the engine can size its textures.
    void  ( *GetInfo )( void *pMovie, unsigned int *pnWidth, unsigned int *pnHeight,
                        unsigned int *pnFrames, unsigned int *pnRateNum,
                        unsigned int *pnRateDen );
    // Decodes the frame at the current position; returns non-zero on success.
    int   ( *DecodeFrame )( void *pMovie, unsigned int nFrame );
    // Copies the decoded frame, converted to the requested surface format.
    int   ( *CopyFrame )( void *pMovie, void *pDest, int nDestPitch,
                          unsigned int nDestX, unsigned int nDestY,
                          unsigned int nSurfaceFormat );
    void  ( *SetVolume )( void *pMovie, int nVolume );
    void  ( *Pause )( void *pMovie, int bPause );
} SBk1VideoBackend;

void Bk1BinkSetDecoderBackend( const SBk1VideoBackend *pBackend );
int  Bk1BinkHasDecoderBackend( void );

// ---------------------------------------------------------------------------
// What the engine calls
// ---------------------------------------------------------------------------
HBINK BinkOpen( const char *pszName, unsigned int nFlags );
void  BinkClose( HBINK hBink );
int   BinkWait( HBINK hBink );
int   BinkDoFrame( HBINK hBink );
void  BinkNextFrame( HBINK hBink );
void  BinkGoto( HBINK hBink, unsigned int nFrame, int nFlags );
int   BinkPause( HBINK hBink, int bPause );
void  BinkSetVolume( HBINK hBink, unsigned int nTrack, int nVolume );
// Returns how many dirty rectangles the last frame produced.
int   BinkGetRects( HBINK hBink, unsigned int nFlags );
int   BinkCopyToBufferRect( HBINK hBink, void *pDest, int nDestPitch,
                            unsigned int nDestHeight, unsigned int nDestX,
                            unsigned int nDestY, unsigned int nSrcX,
                            unsigned int nSrcY, unsigned int nSrcWidth,
                            unsigned int nSrcHeight, unsigned int nFlags );
int   BinkSoundUseDirectSound( void *pDirectSound );

#ifdef __cplusplus
}
#endif
