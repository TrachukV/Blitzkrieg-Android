// The audio device behind fmod.h.
//
// Output is AAudio, which is part of the NDK -- no third-party library, which
// matters because this repository has none. AAudio picks the fastest path the
// device offers and hands us a callback on a real-time thread.
//
// Two kinds of sound, decoded two different ways, because the engine asks for
// them two different ways:
//
//   Samples arrive as a block of memory the engine has already read out of the
//   game archives. They are WAV, and WAV is parsed here -- a RIFF walk and a
//   PCM conversion, a hundred lines and no decoder at all. Handing a memory
//   block to the platform media stack would mean writing it back out to a file
//   first, which is slower and can fail, to decode a format that needs no
//   decoding.
//
//   Music is a real file on disk, mp3 or ogg, and those do need a decoder. The
//   NDK's media codecs are the decoder, running on their own thread and
//   filling a ring buffer the mixer reads from.
//
// The mixer runs in the AAudio callback: it walks the channels, resamples each
// by its rate ratio, applies volume, pan and the 3D attenuation, and sums into
// the output. Everything it touches is either atomic or guarded by the one
// mutex, and the callback never allocates, never opens a file and never
// blocks -- an underrun is an audible click, so the slow work belongs to the
// threads that feed it.
#include "fmod.h"
#include "bk1_wave.h"

#include <aaudio/AAudio.h>
#include <android/log.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaExtractor.h>
#include <media/NdkMediaFormat.h>

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#define LOG_TAG "Blitzkrieg.sfx"
#define LOGI( ... ) __android_log_print( ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__ )
#define LOGW( ... ) __android_log_print( ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__ )
#define LOGE( ... ) __android_log_print( ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__ )

namespace {

// The device runs at one rate and everything is resampled to it. 44100 is what
// the game's own data is recorded at, so the common case resamples by exactly
// one and costs nothing.
const int32_t OUTPUT_RATE = 44100;
const int32_t OUTPUT_CHANNELS = 2;

// FMOD's volume and pan are 0..255, with 128 the centre.
const int FMOD_VOLUME_MAX = 255;
const int FMOD_PAN_CENTRE = 128;
const int FMOD_PAN_MAX = 255;

int g_nLastError = FMOD_ERR_NONE;

void SetError( int nError ) { g_nLastError = nError; }

}   // anonymous namespace

// ---------------------------------------------------------------------------
// The handles the engine holds
// ---------------------------------------------------------------------------
struct FSOUND_SAMPLE
{
    SWaveData wave;
    unsigned int nMode;
    int   nLoopStart;
    int   nLoopEnd;      // -1 for "to the end"
    float fMinDistance;
    float fMaxDistance;

    FSOUND_SAMPLE()
        : nMode( 0 ), nLoopStart( 0 ), nLoopEnd( -1 ),
          fMinDistance( 1.0f ), fMaxDistance( 1000000.0f ) {}

    int Frames() const { return wave.Frames(); }
    bool Looping() const { return ( nMode & ( FSOUND_LOOP_NORMAL | FSOUND_LOOP_BIDI ) ) != 0; }
};

// A decoded music stream, filled by a worker thread and drained by the mixer.
struct FSOUND_STREAM
{
    std::string szFileName;
    unsigned int nMode;

    // The ring the decoder fills and the mixer empties. Interleaved stereo at
    // the output rate, so the mixer does no conversion on the audio thread.
    std::vector<int16_t> ring;
    std::atomic<size_t>  nWritePos;
    std::atomic<size_t>  nReadPos;

    std::thread       decoder;
    std::atomic<bool> bStop;
    std::atomic<bool> bFinished;      // decoder reached the end of the file
    std::atomic<bool> bReportedEnd;   // the end callback has been fired once

    FSOUND_STREAMCALLBACK pEndCallback;
    int                   nEndParam;
    FSOUND_STREAMCALLBACK pSynchCallback;
    int                   nSynchParam;

    int nChannel;                     // where it is playing, or -1

    FSOUND_STREAM()
        : nMode( 0 ), nWritePos( 0 ), nReadPos( 0 ), bStop( false ),
          bFinished( false ), bReportedEnd( false ), pEndCallback( 0 ),
          nEndParam( 0 ), pSynchCallback( 0 ), nSynchParam( 0 ), nChannel( -1 )
    {
        // Two seconds of slack. The decoder wakes on a timer, and a short ring
        // turns any scheduling hiccup into a gap in the music.
        ring.resize( (size_t)OUTPUT_RATE * OUTPUT_CHANNELS * 2 );
    }

    size_t Available() const
    {
        return nWritePos.load( std::memory_order_acquire ) -
               nReadPos.load( std::memory_order_relaxed );
    }
};

namespace {

// ---------------------------------------------------------------------------
// A voice
// ---------------------------------------------------------------------------
struct SChannel
{
    FSOUND_SAMPLE *pSample;
    FSOUND_STREAM *pStream;      // set instead of pSample for music

    double dPosition;            // in frames, fractional for resampling
    bool   bActive;
    bool   bPaused;
    int    nVolume;              // 0..255
    int    nPan;                 // 0..255, or FSOUND_STEREOPAN
    float  fPos[3];
    bool   bHasPosition;

    SChannel() { Reset(); }

    void Reset()
    {
        pSample = 0;
        pStream = 0;
        dPosition = 0.0;
        bActive = false;
        bPaused = false;
        nVolume = FMOD_VOLUME_MAX;
        nPan = FMOD_PAN_CENTRE;
        fPos[0] = fPos[1] = fPos[2] = 0.0f;
        bHasPosition = false;
    }
};

// ---------------------------------------------------------------------------
// The device
// ---------------------------------------------------------------------------
struct SMixer
{
    std::mutex            mutex;
    std::vector<SChannel> channels;
    AAudioStream         *pStream;
    bool                  bInitialised;
    std::string           szRootDirectory;

    // The listener, as the engine last set it.
    float fListenerPos[3];
    float fListenerFront[3];
    float fListenerTop[3];
    float fDistanceFactor;
    float fRolloffFactor;

    SMixer()
        : pStream( 0 ), bInitialised( false ), fDistanceFactor( 1.0f ),
          fRolloffFactor( 1.0f )
    {
        fListenerPos[0] = fListenerPos[1] = fListenerPos[2] = 0.0f;
        fListenerFront[0] = 0.0f; fListenerFront[1] = 0.0f; fListenerFront[2] = 1.0f;
        fListenerTop[0] = 0.0f; fListenerTop[1] = 1.0f; fListenerTop[2] = 0.0f;
    }
};

SMixer g_mixer;

// How loud a positioned sound is, and how far to one side. FMOD's model is an
// inverse rolloff: full volume within the minimum distance, falling as
// min/distance beyond it, silent past the maximum.
void Compute3D( const SChannel &channel, float *pfGain, float *pfPan )
{
    *pfGain = 1.0f;
    *pfPan = 0.0f;
    if ( !channel.bHasPosition || channel.pSample == 0 )
        return;

    const float dx = ( channel.fPos[0] - g_mixer.fListenerPos[0] ) * g_mixer.fDistanceFactor;
    const float dy = ( channel.fPos[1] - g_mixer.fListenerPos[1] ) * g_mixer.fDistanceFactor;
    const float dz = ( channel.fPos[2] - g_mixer.fListenerPos[2] ) * g_mixer.fDistanceFactor;
    const float fDistance = sqrtf( dx * dx + dy * dy + dz * dz );

    const float fMin = channel.pSample->fMinDistance;
    const float fMax = channel.pSample->fMaxDistance;

    if ( fDistance <= fMin || fMin <= 0.0f )
        *pfGain = 1.0f;
    else if ( fDistance >= fMax )
        *pfGain = 0.0f;
    else
        *pfGain = fMin / ( fMin + g_mixer.fRolloffFactor * ( fDistance - fMin ) );

    if ( fDistance > 0.0001f )
    {
        // Right is front x top, and the component of the offset along it says
        // which ear the sound belongs in.
        const float rx = g_mixer.fListenerFront[1] * g_mixer.fListenerTop[2] -
                         g_mixer.fListenerFront[2] * g_mixer.fListenerTop[1];
        const float ry = g_mixer.fListenerFront[2] * g_mixer.fListenerTop[0] -
                         g_mixer.fListenerFront[0] * g_mixer.fListenerTop[2];
        const float rz = g_mixer.fListenerFront[0] * g_mixer.fListenerTop[1] -
                         g_mixer.fListenerFront[1] * g_mixer.fListenerTop[0];
        const float fRightLen = sqrtf( rx * rx + ry * ry + rz * rz );
        if ( fRightLen > 0.0001f )
        {
            const float fDot = ( dx * rx + dy * ry + dz * rz ) / ( fRightLen * fDistance );
            *pfPan = fDot < -1.0f ? -1.0f : ( fDot > 1.0f ? 1.0f : fDot );
        }
    }
}

// Equal-power panning, so a sound crossing the centre keeps its loudness.
void PanGains( float fPan, float *pfLeft, float *pfRight )
{
    const float fAngle = ( fPan + 1.0f ) * 0.25f * 3.14159265f;
    *pfLeft = cosf( fAngle );
    *pfRight = sinf( fAngle );
}

// One channel's contribution. Runs on the audio thread: no allocation, no
// locking beyond the caller's, no system calls.
void MixChannel( SChannel *pChannel, float *pOut, int32_t nFrames )
{
    if ( !pChannel->bActive || pChannel->bPaused )
        return;

    float fGain = (float)pChannel->nVolume / (float)FMOD_VOLUME_MAX;
    float fPan = 0.0f;

    if ( pChannel->nPan != FSOUND_STEREOPAN )
        fPan = ( (float)pChannel->nPan - FMOD_PAN_CENTRE ) / (float)FMOD_PAN_CENTRE;

    if ( pChannel->bHasPosition )
    {
        float f3DGain = 1.0f, f3DPan = 0.0f;
        Compute3D( *pChannel, &f3DGain, &f3DPan );
        fGain *= f3DGain;
        fPan = f3DPan;
    }

    float fLeft = 1.0f, fRight = 1.0f;
    PanGains( fPan, &fLeft, &fRight );
    fLeft *= fGain;
    fRight *= fGain;

    // --- music ---
    if ( pChannel->pStream != 0 )
    {
        FSOUND_STREAM *pStream = pChannel->pStream;
        for ( int32_t i = 0; i < nFrames; ++i )
        {
            const size_t nRead = pStream->nReadPos.load( std::memory_order_relaxed );
            if ( nRead + OUTPUT_CHANNELS > pStream->nWritePos.load( std::memory_order_acquire ) )
            {
                // Nothing decoded yet. If the decoder has also finished, the
                // track is over; otherwise this is a hiccup and silence is the
                // right thing to emit for one buffer.
                if ( pStream->bFinished.load( std::memory_order_acquire ) )
                {
                    pChannel->bActive = false;
                    pChannel->pStream = 0;
                    pStream->nChannel = -1;
                }
                break;
            }
            const size_t nRingSize = pStream->ring.size();
            const float fL = pStream->ring[nRead % nRingSize] / 32768.0f;
            const float fR = pStream->ring[( nRead + 1 ) % nRingSize] / 32768.0f;
            pOut[i * 2] += fL * fLeft;
            pOut[i * 2 + 1] += fR * fRight;
            pStream->nReadPos.store( nRead + OUTPUT_CHANNELS, std::memory_order_release );
        }
        return;
    }

    // --- sample ---
    FSOUND_SAMPLE *pSample = pChannel->pSample;
    if ( pSample == 0 || pSample->wave.samples.empty() )
    {
        pChannel->bActive = false;
        return;
    }

    const int nSourceChannels = pSample->wave.nChannels;
    const int nTotalFrames = pSample->Frames();
    const double dStep = (double)pSample->wave.nRate / (double)OUTPUT_RATE;

    const int nLoopStart = pSample->nLoopStart;
    const int nLoopEnd = ( pSample->nLoopEnd > 0 && pSample->nLoopEnd < nTotalFrames )
                             ? pSample->nLoopEnd : nTotalFrames;

    for ( int32_t i = 0; i < nFrames; ++i )
    {
        int nFrame = (int)pChannel->dPosition;
        if ( nFrame >= nLoopEnd )
        {
            if ( pSample->Looping() )
            {
                pChannel->dPosition = nLoopStart;
                nFrame = nLoopStart;
            }
            else
            {
                pChannel->bActive = false;
                pChannel->pSample = 0;
                break;
            }
        }

        // Linear interpolation between neighbouring frames. At the common case
        // of a 44100 sample on a 44100 device the fraction is zero and this is
        // an exact copy.
        const double dFraction = pChannel->dPosition - nFrame;
        int nNext = nFrame + 1;
        if ( nNext >= nLoopEnd )
            nNext = pSample->Looping() ? nLoopStart : nFrame;

        float fL, fR;
        if ( nSourceChannels == 1 )
        {
            const float a = pSample->wave.samples[nFrame] / 32768.0f;
            const float b = pSample->wave.samples[nNext] / 32768.0f;
            fL = fR = (float)( a + ( b - a ) * dFraction );
        }
        else
        {
            const float aL = pSample->wave.samples[nFrame * 2] / 32768.0f;
            const float bL = pSample->wave.samples[nNext * 2] / 32768.0f;
            const float aR = pSample->wave.samples[nFrame * 2 + 1] / 32768.0f;
            const float bR = pSample->wave.samples[nNext * 2 + 1] / 32768.0f;
            fL = (float)( aL + ( bL - aL ) * dFraction );
            fR = (float)( aR + ( bR - aR ) * dFraction );
        }

        pOut[i * 2] += fL * fLeft;
        pOut[i * 2 + 1] += fR * fRight;
        pChannel->dPosition += dStep;
    }
}

aaudio_data_callback_result_t DataCallback( AAudioStream *, void *, void *pAudioData,
                                            int32_t nFrames )
{
    float *pOut = (float *)pAudioData;
    memset( pOut, 0, (size_t)nFrames * OUTPUT_CHANNELS * sizeof( float ) );

    {
        std::lock_guard<std::mutex> lock( g_mixer.mutex );
        for ( size_t i = 0; i < g_mixer.channels.size(); ++i )
            MixChannel( &g_mixer.channels[i], pOut, nFrames );
    }

    // Summing many voices can exceed full scale; clipping here is preferable
    // to letting the device wrap, which is a loud crack rather than a soft one.
    const int32_t nTotal = nFrames * OUTPUT_CHANNELS;
    for ( int32_t i = 0; i < nTotal; ++i )
    {
        if ( pOut[i] > 1.0f ) pOut[i] = 1.0f;
        else if ( pOut[i] < -1.0f ) pOut[i] = -1.0f;
    }
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

int FindFreeChannel()
{
    for ( size_t i = 0; i < g_mixer.channels.size(); ++i )
    {
        if ( !g_mixer.channels[i].bActive )
            return (int)i;
    }
    return -1;
}

bool ValidChannel( int nChannel )
{
    return nChannel >= 0 && nChannel < (int)g_mixer.channels.size();
}

// ---------------------------------------------------------------------------
// The music decoder
// ---------------------------------------------------------------------------
// Runs on its own thread, so a slow decode never reaches the audio callback.
void DecodeStream( FSOUND_STREAM *pStream )
{
    AMediaExtractor *pExtractor = AMediaExtractor_new();
    if ( pExtractor == 0 )
    {
        pStream->bFinished.store( true, std::memory_order_release );
        return;
    }

    int nFd = open( pStream->szFileName.c_str(), O_RDONLY );
    if ( nFd < 0 )
    {
        LOGW( "cannot open %s", pStream->szFileName.c_str() );
        AMediaExtractor_delete( pExtractor );
        pStream->bFinished.store( true, std::memory_order_release );
        return;
    }
    const off_t nSize = lseek( nFd, 0, SEEK_END );
    lseek( nFd, 0, SEEK_SET );

    media_status_t status =
        AMediaExtractor_setDataSourceFd( pExtractor, nFd, 0, (off64_t)nSize );
    if ( status != AMEDIA_OK )
    {
        close( nFd );
        AMediaExtractor_delete( pExtractor );
        pStream->bFinished.store( true, std::memory_order_release );
        return;
    }

    // The first audio track is the one; these files carry exactly one.
    AMediaCodec *pCodec = 0;
    int32_t nSourceRate = OUTPUT_RATE;
    int32_t nSourceChannels = 2;
    const size_t nTracks = AMediaExtractor_getTrackCount( pExtractor );
    for ( size_t i = 0; i < nTracks; ++i )
    {
        AMediaFormat *pFormat = AMediaExtractor_getTrackFormat( pExtractor, i );
        const char *pszMime = 0;
        if ( AMediaFormat_getString( pFormat, AMEDIAFORMAT_KEY_MIME, &pszMime ) &&
             pszMime != 0 && strncmp( pszMime, "audio/", 6 ) == 0 )
        {
            AMediaFormat_getInt32( pFormat, AMEDIAFORMAT_KEY_SAMPLE_RATE, &nSourceRate );
            AMediaFormat_getInt32( pFormat, AMEDIAFORMAT_KEY_CHANNEL_COUNT, &nSourceChannels );
            AMediaExtractor_selectTrack( pExtractor, i );
            pCodec = AMediaCodec_createDecoderByType( pszMime );
            if ( pCodec != 0 )
                AMediaCodec_configure( pCodec, pFormat, 0, 0, 0 );
            AMediaFormat_delete( pFormat );
            break;
        }
        AMediaFormat_delete( pFormat );
    }

    if ( pCodec == 0 )
    {
        close( nFd );
        AMediaExtractor_delete( pExtractor );
        pStream->bFinished.store( true, std::memory_order_release );
        return;
    }
    AMediaCodec_start( pCodec );

    const double dStep = (double)nSourceRate / (double)OUTPUT_RATE;
    double dFractional = 0.0;
    std::vector<int16_t> pending;      // decoded frames not yet resampled out
    bool bInputDone = false;

    while ( !pStream->bStop.load( std::memory_order_acquire ) )
    {
        // Do not run ahead of the mixer; the ring is the whole point.
        const size_t nRingSize = pStream->ring.size();
        if ( pStream->Available() > nRingSize - OUTPUT_RATE / 4 )
        {
            usleep( 20000 );
            continue;
        }

        if ( !bInputDone )
        {
            const ssize_t nInputIndex = AMediaCodec_dequeueInputBuffer( pCodec, 10000 );
            if ( nInputIndex >= 0 )
            {
                size_t nBufferSize = 0;
                uint8_t *pBuffer =
                    AMediaCodec_getInputBuffer( pCodec, (size_t)nInputIndex, &nBufferSize );
                const ssize_t nRead =
                    AMediaExtractor_readSampleData( pExtractor, pBuffer, nBufferSize );
                if ( nRead < 0 )
                {
                    AMediaCodec_queueInputBuffer( pCodec, (size_t)nInputIndex, 0, 0, 0,
                                                  AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM );
                    bInputDone = true;
                }
                else
                {
                    const int64_t nTime = AMediaExtractor_getSampleTime( pExtractor );
                    AMediaCodec_queueInputBuffer( pCodec, (size_t)nInputIndex, 0,
                                                  (size_t)nRead, (uint64_t)nTime, 0 );
                    AMediaExtractor_advance( pExtractor );
                }
            }
        }

        AMediaCodecBufferInfo info;
        const ssize_t nOutputIndex = AMediaCodec_dequeueOutputBuffer( pCodec, &info, 10000 );
        if ( nOutputIndex >= 0 )
        {
            size_t nBufferSize = 0;
            uint8_t *pBuffer =
                AMediaCodec_getOutputBuffer( pCodec, (size_t)nOutputIndex, &nBufferSize );
            if ( pBuffer != 0 && info.size > 0 )
            {
                const int16_t *pPcm = (const int16_t *)( pBuffer + info.offset );
                const size_t nCount = (size_t)info.size / 2;
                pending.insert( pending.end(), pPcm, pPcm + nCount );

                // Resample and widen to stereo straight into the ring.
                const size_t nFrames = pending.size() / (size_t)nSourceChannels;
                size_t nConsumed = 0;
                while ( (size_t)( dFractional ) + 1 < nFrames )
                {
                    const size_t nFrame = (size_t)dFractional;
                    int16_t nL, nR;
                    if ( nSourceChannels == 1 )
                    {
                        nL = nR = pending[nFrame];
                    }
                    else
                    {
                        nL = pending[nFrame * nSourceChannels];
                        nR = pending[nFrame * nSourceChannels + 1];
                    }
                    const size_t nWrite = pStream->nWritePos.load( std::memory_order_relaxed );
                    pStream->ring[nWrite % nRingSize] = nL;
                    pStream->ring[( nWrite + 1 ) % nRingSize] = nR;
                    pStream->nWritePos.store( nWrite + OUTPUT_CHANNELS,
                                              std::memory_order_release );
                    dFractional += dStep;
                    nConsumed = nFrame;
                }
                // Keep the frame the fraction still points into.
                if ( nConsumed > 0 )
                {
                    const size_t nDrop = nConsumed * (size_t)nSourceChannels;
                    pending.erase( pending.begin(), pending.begin() + nDrop );
                    dFractional -= nConsumed;
                }
            }
            const bool bEnd = ( info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM ) != 0;
            AMediaCodec_releaseOutputBuffer( pCodec, (size_t)nOutputIndex, false );
            if ( bEnd )
                break;
        }
    }

    AMediaCodec_stop( pCodec );
    AMediaCodec_delete( pCodec );
    AMediaExtractor_delete( pExtractor );
    close( nFd );
    pStream->bFinished.store( true, std::memory_order_release );
}

}   // anonymous namespace

// ---------------------------------------------------------------------------
// The FMOD surface
// ---------------------------------------------------------------------------
extern "C" {

signed char FSOUND_SetOutput( int )
{
    // There is one output on Android. The engine probes several and writes the
    // one it chose into the user's profile; whichever it names, it gets this.
    return 1;
}

signed char FSOUND_SetDriver( int ) { return 1; }
signed char FSOUND_SetHWND( void * ) { return 1; }

int FSOUND_GetNumDrivers( void ) { return 1; }

const char *FSOUND_GetDriverName( int ) { return "Android"; }

signed char FSOUND_GetDriverCaps( int, unsigned int *pnCaps )
{
    // No hardware voices, no EAX, no geometry. Saying so keeps the engine from
    // asking for effects nothing here can apply.
    if ( pnCaps != 0 )
        *pnCaps = 0;
    return 1;
}

int FSOUND_GetOutputHandle( void ) { return 0; }
int FSOUND_GetMixer( void ) { return FSOUND_MIXER_QUALITY_FPU; }
int FSOUND_GetError( void ) { return g_nLastError; }
float FSOUND_GetVersion( void ) { return FMOD_VERSION; }

signed char FSOUND_Init( int, int nMaxSoftwareChannels, unsigned int )
{
    std::lock_guard<std::mutex> lock( g_mixer.mutex );
    if ( g_mixer.bInitialised )
        return 1;

    // The requested mix rate is ignored: the device has its own and everything
    // is resampled to it, which is both simpler and what a modern audio stack
    // would do internally anyway.
    if ( nMaxSoftwareChannels < 8 )
        nMaxSoftwareChannels = 8;
    if ( nMaxSoftwareChannels > 256 )
        nMaxSoftwareChannels = 256;
    g_mixer.channels.assign( (size_t)nMaxSoftwareChannels, SChannel() );

    AAudioStreamBuilder *pBuilder = 0;
    if ( AAudio_createStreamBuilder( &pBuilder ) != AAUDIO_OK )
    {
        SetError( FMOD_ERR_INIT );
        return 0;
    }

    AAudioStreamBuilder_setFormat( pBuilder, AAUDIO_FORMAT_PCM_FLOAT );
    AAudioStreamBuilder_setChannelCount( pBuilder, OUTPUT_CHANNELS );
    AAudioStreamBuilder_setSampleRate( pBuilder, OUTPUT_RATE );
    AAudioStreamBuilder_setPerformanceMode( pBuilder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY );
    AAudioStreamBuilder_setSharingMode( pBuilder, AAUDIO_SHARING_MODE_SHARED );
    AAudioStreamBuilder_setDataCallback( pBuilder, DataCallback, 0 );

    const aaudio_result_t result = AAudioStreamBuilder_openStream( pBuilder, &g_mixer.pStream );
    AAudioStreamBuilder_delete( pBuilder );
    if ( result != AAUDIO_OK || g_mixer.pStream == 0 )
    {
        LOGE( "AAudio open failed: %s", AAudio_convertResultToText( result ) );
        SetError( FMOD_ERR_OUTPUT_FORMAT );
        return 0;
    }

    AAudioStream_requestStart( g_mixer.pStream );
    g_mixer.bInitialised = true;
    LOGI( "audio started: %d Hz, %d channels, %d voices",
          AAudioStream_getSampleRate( g_mixer.pStream ),
          AAudioStream_getChannelCount( g_mixer.pStream ),
          nMaxSoftwareChannels );
    SetError( FMOD_ERR_NONE );
    return 1;
}

void FSOUND_Close( void )
{
    std::lock_guard<std::mutex> lock( g_mixer.mutex );
    if ( g_mixer.pStream != 0 )
    {
        AAudioStream_requestStop( g_mixer.pStream );
        AAudioStream_close( g_mixer.pStream );
        g_mixer.pStream = 0;
    }
    g_mixer.channels.clear();
    g_mixer.bInitialised = false;
}

int FSOUND_GetChannelsPlaying( void )
{
    std::lock_guard<std::mutex> lock( g_mixer.mutex );
    int nPlaying = 0;
    for ( size_t i = 0; i < g_mixer.channels.size(); ++i )
    {
        if ( g_mixer.channels[i].bActive && !g_mixer.channels[i].bPaused )
            ++nPlaying;
    }
    return nPlaying;
}

// --- samples ---

FSOUND_SAMPLE *FSOUND_Sample_Load( int, const char *pData, unsigned int nMode,
                                   int nLength )
{
    if ( pData == 0 || nLength <= 0 )
    {
        SetError( FMOD_ERR_INVALID_PARAM );
        return 0;
    }

    FSOUND_SAMPLE *pSample = new FSOUND_SAMPLE();
    if ( !ParseWave( (const unsigned char *)pData, (size_t)nLength, &pSample->wave ) )
    {
        delete pSample;
        SetError( FMOD_ERR_FILE_FORMAT );
        return 0;
    }
    pSample->nMode = nMode;
    SetError( FMOD_ERR_NONE );
    return pSample;
}

void FSOUND_Sample_Free( FSOUND_SAMPLE *pSample )
{
    if ( pSample == 0 )
        return;
    {
        // A channel still playing it would read freed memory on the audio
        // thread; stop them first, under the lock the callback also takes.
        std::lock_guard<std::mutex> lock( g_mixer.mutex );
        for ( size_t i = 0; i < g_mixer.channels.size(); ++i )
        {
            if ( g_mixer.channels[i].pSample == pSample )
                g_mixer.channels[i].Reset();
        }
    }
    delete pSample;
}

signed char FSOUND_Sample_SetLoopMode( FSOUND_SAMPLE *pSample, unsigned int nLoopMode )
{
    if ( pSample == 0 )
        return 0;
    pSample->nMode &= ~( FSOUND_LOOP_OFF | FSOUND_LOOP_NORMAL | FSOUND_LOOP_BIDI );
    pSample->nMode |= nLoopMode;
    return 1;
}

signed char FSOUND_Sample_SetLoopPoints( FSOUND_SAMPLE *pSample, int nLoopStart,
                                         int nLoopEnd )
{
    if ( pSample == 0 )
        return 0;
    pSample->nLoopStart = nLoopStart;
    pSample->nLoopEnd = nLoopEnd;
    return 1;
}

signed char FSOUND_Sample_SetMinMaxDistance( FSOUND_SAMPLE *pSample, float fMin,
                                             float fMax )
{
    if ( pSample == 0 )
        return 0;
    pSample->fMinDistance = fMin;
    pSample->fMaxDistance = fMax;
    return 1;
}

signed char FSOUND_Sample_GetDefaults( FSOUND_SAMPLE *pSample, int *pnDefFreq,
                                       int *pnDefVol, int *pnDefPan, int *pnDefPri )
{
    if ( pSample == 0 )
        return 0;
    if ( pnDefFreq != 0 ) *pnDefFreq = pSample->wave.nRate;
    if ( pnDefVol != 0 )  *pnDefVol = FMOD_VOLUME_MAX;
    if ( pnDefPan != 0 )  *pnDefPan = FMOD_PAN_CENTRE;
    if ( pnDefPri != 0 )  *pnDefPri = 128;
    return 1;
}

unsigned int FSOUND_Sample_GetLength( FSOUND_SAMPLE *pSample )
{
    return pSample != 0 ? (unsigned int)pSample->Frames() : 0;
}

// --- playing ---

int FSOUND_PlaySoundEx( int nChannel, FSOUND_SAMPLE *pSample, FSOUND_DSPUNIT *,
                        signed char bPaused )
{
    if ( pSample == 0 )
    {
        SetError( FMOD_ERR_INVALID_PARAM );
        return -1;
    }
    std::lock_guard<std::mutex> lock( g_mixer.mutex );
    if ( g_mixer.channels.empty() )
        return -1;

    if ( nChannel == FSOUND_FREE )
        nChannel = FindFreeChannel();
    if ( !ValidChannel( nChannel ) )
    {
        SetError( FMOD_ERR_CHANNEL_ALLOC );
        return -1;
    }

    SChannel &channel = g_mixer.channels[nChannel];
    channel.Reset();
    channel.pSample = pSample;
    channel.dPosition = 0.0;
    channel.bActive = true;
    channel.bPaused = bPaused != 0;
    channel.bHasPosition = ( pSample->nMode & ( FSOUND_HW3D | FSOUND_3D ) ) != 0;
    SetError( FMOD_ERR_NONE );
    return nChannel;
}

int FSOUND_PlaySound( int nChannel, FSOUND_SAMPLE *pSample )
{
    return FSOUND_PlaySoundEx( nChannel, pSample, 0, 0 );
}

int FSOUND_PlaySound3DAttrib( int nChannel, FSOUND_SAMPLE *pSample, int nFreq,
                              int nVol, int nPan, float *pPos, float * )
{
    const int nPlaying = FSOUND_PlaySoundEx( nChannel, pSample, 0, 1 );
    if ( nPlaying < 0 )
        return -1;
    {
        std::lock_guard<std::mutex> lock( g_mixer.mutex );
        SChannel &channel = g_mixer.channels[nPlaying];
        if ( nVol >= 0 ) channel.nVolume = nVol;
        if ( nPan >= 0 ) channel.nPan = nPan;
        if ( pPos != 0 )
        {
            channel.fPos[0] = pPos[0];
            channel.fPos[1] = pPos[1];
            channel.fPos[2] = pPos[2];
            channel.bHasPosition = true;
        }
        channel.bPaused = false;
    }
    (void)nFreq;
    return nPlaying;
}

signed char FSOUND_StopSound( int nChannel )
{
    std::lock_guard<std::mutex> lock( g_mixer.mutex );
    if ( nChannel == FSOUND_ALL )
    {
        for ( size_t i = 0; i < g_mixer.channels.size(); ++i )
            g_mixer.channels[i].Reset();
        return 1;
    }
    if ( !ValidChannel( nChannel ) )
        return 0;
    g_mixer.channels[nChannel].Reset();
    return 1;
}

signed char FSOUND_SetPaused( int nChannel, signed char bPaused )
{
    std::lock_guard<std::mutex> lock( g_mixer.mutex );
    if ( nChannel == FSOUND_ALL )
    {
        for ( size_t i = 0; i < g_mixer.channels.size(); ++i )
            g_mixer.channels[i].bPaused = bPaused != 0;
        return 1;
    }
    if ( !ValidChannel( nChannel ) )
        return 0;
    g_mixer.channels[nChannel].bPaused = bPaused != 0;
    return 1;
}

signed char FSOUND_SetVolume( int nChannel, int nVolume )
{
    if ( nVolume < 0 ) nVolume = 0;
    if ( nVolume > FMOD_VOLUME_MAX ) nVolume = FMOD_VOLUME_MAX;
    std::lock_guard<std::mutex> lock( g_mixer.mutex );
    if ( nChannel == FSOUND_ALL )
    {
        for ( size_t i = 0; i < g_mixer.channels.size(); ++i )
            g_mixer.channels[i].nVolume = nVolume;
        return 1;
    }
    if ( !ValidChannel( nChannel ) )
        return 0;
    g_mixer.channels[nChannel].nVolume = nVolume;
    return 1;
}

signed char FSOUND_SetPan( int nChannel, int nPan )
{
    std::lock_guard<std::mutex> lock( g_mixer.mutex );
    if ( !ValidChannel( nChannel ) )
        return 0;
    if ( nPan != FSOUND_STEREOPAN )
    {
        if ( nPan < 0 ) nPan = 0;
        if ( nPan > FMOD_PAN_MAX ) nPan = FMOD_PAN_MAX;
    }
    g_mixer.channels[nChannel].nPan = nPan;
    return 1;
}

signed char FSOUND_IsPlaying( int nChannel )
{
    std::lock_guard<std::mutex> lock( g_mixer.mutex );
    if ( !ValidChannel( nChannel ) )
        return 0;
    return g_mixer.channels[nChannel].bActive ? 1 : 0;
}

signed char FSOUND_SetCurrentPosition( int nChannel, unsigned int nOffset )
{
    std::lock_guard<std::mutex> lock( g_mixer.mutex );
    if ( !ValidChannel( nChannel ) )
        return 0;
    g_mixer.channels[nChannel].dPosition = (double)nOffset;
    return 1;
}

unsigned int FSOUND_GetCurrentPosition( int nChannel )
{
    std::lock_guard<std::mutex> lock( g_mixer.mutex );
    if ( !ValidChannel( nChannel ) )
        return 0;
    return (unsigned int)g_mixer.channels[nChannel].dPosition;
}

FSOUND_SAMPLE *FSOUND_GetCurrentSample( int nChannel )
{
    std::lock_guard<std::mutex> lock( g_mixer.mutex );
    if ( !ValidChannel( nChannel ) )
        return 0;
    return g_mixer.channels[nChannel].pSample;
}

// --- three dimensions ---

signed char FSOUND_3D_SetAttributes( int nChannel, float *pPos, float * )
{
    std::lock_guard<std::mutex> lock( g_mixer.mutex );
    if ( !ValidChannel( nChannel ) )
        return 0;
    if ( pPos != 0 )
    {
        SChannel &channel = g_mixer.channels[nChannel];
        channel.fPos[0] = pPos[0];
        channel.fPos[1] = pPos[1];
        channel.fPos[2] = pPos[2];
        channel.bHasPosition = true;
    }
    return 1;
}

void FSOUND_3D_Listener_SetAttributes( float *pPos, float *, float fFrontX,
                                       float fFrontY, float fFrontZ, float fTopX,
                                       float fTopY, float fTopZ )
{
    std::lock_guard<std::mutex> lock( g_mixer.mutex );
    if ( pPos != 0 )
    {
        g_mixer.fListenerPos[0] = pPos[0];
        g_mixer.fListenerPos[1] = pPos[1];
        g_mixer.fListenerPos[2] = pPos[2];
    }
    g_mixer.fListenerFront[0] = fFrontX;
    g_mixer.fListenerFront[1] = fFrontY;
    g_mixer.fListenerFront[2] = fFrontZ;
    g_mixer.fListenerTop[0] = fTopX;
    g_mixer.fListenerTop[1] = fTopY;
    g_mixer.fListenerTop[2] = fTopZ;
}

void FSOUND_3D_Listener_SetDistanceFactor( float fScale )
{
    std::lock_guard<std::mutex> lock( g_mixer.mutex );
    g_mixer.fDistanceFactor = fScale;
}

void FSOUND_3D_Listener_SetRolloffFactor( float fScale )
{
    std::lock_guard<std::mutex> lock( g_mixer.mutex );
    g_mixer.fRolloffFactor = fScale;
}

void FSOUND_3D_Update( void )
{
    // The mixer reads the listener on every buffer, so there is nothing to
    // commit here. What this is the right place for is the end-of-track
    // callback: FMOD fires it from its own thread, and firing it from the
    // audio callback would run engine code on the real-time thread.
    std::vector<FSOUND_STREAM *> finished;
    {
        std::lock_guard<std::mutex> lock( g_mixer.mutex );
        for ( size_t i = 0; i < g_mixer.channels.size(); ++i )
        {
            FSOUND_STREAM *pStream = g_mixer.channels[i].pStream;
            if ( pStream == 0 )
                continue;
            if ( pStream->bFinished.load( std::memory_order_acquire ) &&
                 pStream->Available() == 0 &&
                 !pStream->bReportedEnd.load( std::memory_order_relaxed ) )
            {
                pStream->bReportedEnd.store( true, std::memory_order_relaxed );
                finished.push_back( pStream );
            }
        }
    }
    // Outside the lock: the callback goes back into the engine, which will
    // open the next track and call in here again.
    for ( size_t i = 0; i < finished.size(); ++i )
    {
        if ( finished[i]->pEndCallback != 0 )
            finished[i]->pEndCallback( finished[i], 0, 0, finished[i]->nEndParam );
    }
}

// --- streams ---

FSOUND_STREAM *FSOUND_Stream_OpenFile( const char *pszFileName, unsigned int nMode, int )
{
    if ( pszFileName == 0 )
    {
        SetError( FMOD_ERR_INVALID_PARAM );
        return 0;
    }

    FSOUND_STREAM *pStream = new FSOUND_STREAM();
    pStream->nMode = nMode;

    // The engine builds an absolute path from the data storage's own name, so
    // it is used as given; the root is only prepended when it is relative.
    if ( pszFileName[0] == '/' || g_mixer.szRootDirectory.empty() )
        pStream->szFileName = pszFileName;
    else
        pStream->szFileName = g_mixer.szRootDirectory + "/" + pszFileName;

    // Windows paths in the data files; the filesystem here wants the other slash.
    for ( size_t i = 0; i < pStream->szFileName.size(); ++i )
    {
        if ( pStream->szFileName[i] == '\\' )
            pStream->szFileName[i] = '/';
    }

    if ( access( pStream->szFileName.c_str(), R_OK ) != 0 )
    {
        delete pStream;
        SetError( FMOD_ERR_FILE_NOTFOUND );
        return 0;
    }

    pStream->decoder = std::thread( DecodeStream, pStream );
    SetError( FMOD_ERR_NONE );
    return pStream;
}

int FSOUND_Stream_Play( int nChannel, FSOUND_STREAM *pStream )
{
    if ( pStream == 0 )
        return -1;
    std::lock_guard<std::mutex> lock( g_mixer.mutex );
    if ( nChannel == FSOUND_FREE )
        nChannel = FindFreeChannel();
    if ( !ValidChannel( nChannel ) )
        return -1;

    SChannel &channel = g_mixer.channels[nChannel];
    channel.Reset();
    channel.pStream = pStream;
    channel.bActive = true;
    pStream->nChannel = nChannel;
    return nChannel;
}

signed char FSOUND_Stream_Close( FSOUND_STREAM *pStream )
{
    if ( pStream == 0 )
        return 0;
    {
        std::lock_guard<std::mutex> lock( g_mixer.mutex );
        for ( size_t i = 0; i < g_mixer.channels.size(); ++i )
        {
            if ( g_mixer.channels[i].pStream == pStream )
                g_mixer.channels[i].Reset();
        }
    }
    // Joined before the ring goes away, or the decoder writes into freed memory.
    pStream->bStop.store( true, std::memory_order_release );
    if ( pStream->decoder.joinable() )
        pStream->decoder.join();
    delete pStream;
    return 1;
}

signed char FSOUND_Stream_SetEndCallback( FSOUND_STREAM *pStream,
                                          FSOUND_STREAMCALLBACK pCallback, int nParam )
{
    if ( pStream == 0 )
        return 0;
    std::lock_guard<std::mutex> lock( g_mixer.mutex );
    pStream->pEndCallback = pCallback;
    pStream->nEndParam = nParam;
    return 1;
}

signed char FSOUND_Stream_SetSynchCallback( FSOUND_STREAM *pStream,
                                            FSOUND_STREAMCALLBACK pCallback, int nParam )
{
    if ( pStream == 0 )
        return 0;
    std::lock_guard<std::mutex> lock( g_mixer.mutex );
    pStream->pSynchCallback = pCallback;
    pStream->nSynchParam = nParam;
    return 1;
}

}   // extern "C"

// ---------------------------------------------------------------------------
// The port's own handles
// ---------------------------------------------------------------------------
void Bk1SoundSetRootDirectory( const char *pszPath )
{
    std::lock_guard<std::mutex> lock( g_mixer.mutex );
    g_mixer.szRootDirectory = ( pszPath != 0 ) ? pszPath : "";
}

void Bk1SoundSetSuspended( bool bSuspended )
{
    std::lock_guard<std::mutex> lock( g_mixer.mutex );
    if ( g_mixer.pStream == 0 )
        return;
    if ( bSuspended )
        AAudioStream_requestPause( g_mixer.pStream );
    else
        AAudioStream_requestStart( g_mixer.pStream );
}
