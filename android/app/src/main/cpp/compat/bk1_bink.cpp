// The video player behind bink.h.
//
// Everything here is the bookkeeping the engine's player expects -- handle
// lifetime, frame counting, seeking, dirty rectangles, the pixel format the
// frame is copied out in. The decoding itself belongs to the backend the
// Android layer installs, because Bink's own format is not ours to implement.
#include "bink.h"

#include <stdlib.h>
#include <string.h>

namespace {

const SBk1VideoBackend *g_pBackend = 0;

struct SMovie
{
    void        *pBackendMovie;
    unsigned int nRateNum;
    unsigned int nRateDen;
    bool         bPaused;

    SMovie() : pBackendMovie( 0 ), nRateNum( 30 ), nRateDen( 1 ), bPaused( false ) {}
};

SMovie *MovieOf( HBINK hBink )
{
    return ( hBink != 0 ) ? (SMovie *)hBink->pInternal : 0;
}

}   // anonymous namespace

extern "C" {

void Bk1BinkSetDecoderBackend( const SBk1VideoBackend *pBackend )
{
    g_pBackend = pBackend;
}

int Bk1BinkHasDecoderBackend( void )
{
    return ( g_pBackend != 0 ) ? 1 : 0;
}

HBINK BinkOpen( const char *pszName, unsigned int nFlags )
{
    // BINKFROMMEMORY hands a buffer rather than a name. The engine only takes
    // that path when it has already read the file itself, and a backend that
    // decodes from a path cannot use it, so it is refused rather than
    // misinterpreted as a name.
    if ( g_pBackend == 0 || pszName == 0 || ( nFlags & BINKFROMMEMORY ) != 0 )
        return 0;

    void *pBackendMovie = g_pBackend->Open( pszName );
    if ( pBackendMovie == 0 )
        return 0;

    HBINK hBink = (HBINK)calloc( 1, sizeof( BINK ) );
    if ( hBink == 0 )
    {
        g_pBackend->Close( pBackendMovie );
        return 0;
    }

    SMovie *pMovie = new SMovie();
    pMovie->pBackendMovie = pBackendMovie;

    g_pBackend->GetInfo( pBackendMovie, &hBink->Width, &hBink->Height,
                         &hBink->Frames, &pMovie->nRateNum, &pMovie->nRateDen );
    hBink->FrameNum = 1;                    // Bink counts frames from one
    hBink->LastFrameNum = 0;
    hBink->FrameRate = pMovie->nRateNum;
    hBink->FrameRateDiv = pMovie->nRateDen != 0 ? pMovie->nRateDen : 1;
    hBink->OpenFlags = nFlags;
    hBink->pInternal = pMovie;
    return hBink;
}

void BinkClose( HBINK hBink )
{
    SMovie *pMovie = MovieOf( hBink );
    if ( pMovie != 0 )
    {
        if ( g_pBackend != 0 && pMovie->pBackendMovie != 0 )
            g_pBackend->Close( pMovie->pBackendMovie );
        delete pMovie;
    }
    if ( hBink != 0 )
        free( hBink );
}

// Bink returned non-zero while the next frame was not due yet, so that the
// caller could do other work. The backend paces itself, so there is never
// anything to wait for here.
int BinkWait( HBINK )
{
    return 0;
}

int BinkDoFrame( HBINK hBink )
{
    SMovie *pMovie = MovieOf( hBink );
    if ( pMovie == 0 || g_pBackend == 0 )
        return 0;

    if ( !g_pBackend->DecodeFrame( pMovie->pBackendMovie, hBink->FrameNum ) )
    {
        hBink->ReadError = 1;
        return 0;
    }

    // The whole frame is treated as dirty: the engine uploads it to a texture
    // wholesale, so a finer list would buy nothing.
    hBink->NumRects = 1;
    hBink->FrameRects[0].Left = 0;
    hBink->FrameRects[0].Top = 0;
    hBink->FrameRects[0].Width = (int)hBink->Width;
    hBink->FrameRects[0].Height = (int)hBink->Height;
    return 1;
}

void BinkNextFrame( HBINK hBink )
{
    if ( hBink == 0 )
        return;
    hBink->LastFrameNum = hBink->FrameNum;
    ++hBink->FrameNum;
    if ( hBink->Frames > 0 && hBink->FrameNum > hBink->Frames )
        hBink->FrameNum = 1;                // Bink loops rather than stopping
}

void BinkGoto( HBINK hBink, unsigned int nFrame, int )
{
    if ( hBink == 0 )
        return;
    if ( nFrame < 1 )
        nFrame = 1;
    if ( hBink->Frames > 0 && nFrame > hBink->Frames )
        nFrame = hBink->Frames;
    hBink->LastFrameNum = hBink->FrameNum;
    hBink->FrameNum = nFrame;
}

int BinkPause( HBINK hBink, int bPause )
{
    SMovie *pMovie = MovieOf( hBink );
    if ( pMovie == 0 )
        return 0;
    pMovie->bPaused = ( bPause != 0 );
    if ( g_pBackend != 0 && g_pBackend->Pause != 0 )
        g_pBackend->Pause( pMovie->pBackendMovie, bPause );
    return 1;
}

void BinkSetVolume( HBINK hBink, unsigned int, int nVolume )
{
    SMovie *pMovie = MovieOf( hBink );
    if ( pMovie != 0 && g_pBackend != 0 && g_pBackend->SetVolume != 0 )
        g_pBackend->SetVolume( pMovie->pBackendMovie, nVolume );
}

int BinkGetRects( HBINK hBink, unsigned int )
{
    // BinkDoFrame already filled the list; this call only asks how many.
    return ( hBink != 0 ) ? (int)hBink->NumRects : 0;
}

int BinkCopyToBufferRect( HBINK hBink, void *pDest, int nDestPitch,
                          unsigned int, unsigned int nDestX, unsigned int nDestY,
                          unsigned int, unsigned int, unsigned int, unsigned int,
                          unsigned int nFlags )
{
    SMovie *pMovie = MovieOf( hBink );
    if ( pMovie == 0 || pDest == 0 || g_pBackend == 0 )
        return 0;
    return g_pBackend->CopyFrame( pMovie->pBackendMovie, pDest, nDestPitch,
                                  nDestX, nDestY, nFlags & BINKSURFACEMASK );
}

// Bink drove its own audio through DirectSound. The backend owns playback
// here, so there is no device to hand it.
int BinkSoundUseDirectSound( void * )
{
    return 1;
}

}   // extern "C"
